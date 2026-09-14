#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [ "$(uname -s)" != Linux ]; then
    echo 'Native PufferLib 5 training requires Linux with an NVIDIA CUDA toolkit.' >&2
    exit 2
fi
export CUDA_HOME="${CUDA_HOME:-/usr/local/cuda}"
export PATH="$CUDA_HOME/bin:$PATH"
for tool in nvcc nvidia-smi ccache clang timeout python3; do
    command -v "$tool" >/dev/null || { echo "Missing dependency: $tool" >&2; exit 2; }
done
run_id="${1:-breakout_$(date -u +%Y%m%dT%H%M%SZ)}"
case "$run_id" in
    ''|*[!a-zA-Z0-9_-]*) echo 'Run ID must use letters, digits, _ or -.' >&2; exit 2 ;;
esac
if [ "${#run_id}" -gt 60 ]; then
    echo 'Run ID must be at most 60 characters.' >&2
    exit 2
fi
run_dir="outputs/$run_id"
mkdir -p outputs build
mkdir "$run_dir"   # Refuse to overwrite an earlier run.
git rev-parse HEAD > "$run_dir/source-commit.txt"
git status --short > "$run_dir/source-status.txt"
git diff HEAD --binary > "$run_dir/source.patch"
cp config/default.ini "$run_dir/default.ini"
cp config/breakout.ini "$run_dir/breakout.ini"
{
    date -u +%Y-%m-%dT%H:%M:%SZ
    uname -sm
    nvcc --version
    clang --version
    nvidia-smi --query-gpu=name,driver_version,memory.total,memory.used,utilization.gpu --format=csv
    nvidia-smi --query-compute-apps=pid,process_name,used_gpu_memory --format=csv
} > "$run_dir/machine.txt"
./build.sh breakout build/puffer-breakout --cu 2>&1 | tee "$run_dir/build.log"
command=(./build/puffer-breakout train
    --base.run_id="$run_id" --base.seed=73 --train.gpus=1
    --train.total_timesteps=55000000 --base.eval_episodes=256
    --base.checkpoint_dir="$run_dir/checkpoints" --base.log_dir="$run_dir/logs")
printf '%q ' "${command[@]}" > "$run_dir/command.txt"
printf '\n' >> "$run_dir/command.txt"
python3 - "$run_dir" "${command[@]}" <<'PY'
import array
import configparser
import hashlib
import json
import math
import pathlib
import subprocess
import sys
import time
from datetime import datetime, timezone

out = pathlib.Path(sys.argv[1])
command = sys.argv[2:]
report = {
    'started_utc': datetime.now(timezone.utc).isoformat(),
    'command': command,
    'source_commit': (out / 'source-commit.txt').read_text().strip(),
    'source_dirty': bool((out / 'source-status.txt').read_text().strip()),
    'backend': 'CUDA environment + CUDA learner',
    'precision': 'bf16',
    'training_seed': 73,
    'requested_steps': 55000000,
    'timeout_seconds': 180,
}
start = time.monotonic()
with (out / 'train.log').open('w') as log:
    process = subprocess.Popen(
        ['timeout', '--signal=TERM', '--kill-after=10s', '180s', *command],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    for line in process.stdout:
        log.write(line)
        log.flush()
        print(line, end='', flush=True)
    report['exit_code'] = process.wait()
report['process_seconds_including_initialization_and_evaluation'] = time.monotonic() - start
report['checkpoints'] = [
    {'path': str(p), 'bytes': p.stat().st_size,
     'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
    for p in sorted((out / 'checkpoints').rglob('*.bin'))
]
errors = []
if report['exit_code'] == 0:
    config = configparser.ConfigParser(interpolation=None)
    config.read(out / 'logs' / 'breakout' / (out.name + '.ini'))
    metrics = dict(config['metrics']) if config.has_section('metrics') else {}
    if not any(key.startswith('loss/') for key in metrics):
        errors.append('Missing loss metrics')
    report['metrics_finite'] = bool(metrics) and all(
        math.isfinite(float(value))
        for values in metrics.values() for value in values.split(','))
    if not report['metrics_finite']:
        errors.append('Missing or nonfinite metrics')
    report['native_run'] = dict(config['run']) if config.has_section('run') else {}
    for item in report['checkpoints']:
        weights = array.array('f')
        weights.frombytes(pathlib.Path(item['path']).read_bytes())
        item['weights_finite'] = bool(weights) and all(map(math.isfinite, weights))
        if not item['weights_finite']:
            errors.append('Nonfinite or empty checkpoint: ' + item['path'])
    if not report['checkpoints']:
        errors.append('Training exited without saving a checkpoint')
report['validation_errors'] = errors
(out / 'run.json').write_text(json.dumps(report, indent=2) + '\n')
if errors:
    raise SystemExit('; '.join(errors))
raise SystemExit(report['exit_code'])
PY
echo "Run artifacts: $run_dir"
