#!/usr/bin/env -S uv run
"""Local prerequisite checks. No installations, network requests or training jobs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
NEXT = {
    'preview': 'uv run python -m http.server 8781 --bind 127.0.0.1 --directory docs',
    'native': './scripts/check.sh',
    'web': 'uv run scripts/build_fleet_site.py',
    'train': './build.sh alienwars_shared build/puffer-shared',
}


def command(args):
    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=15)
        return result.returncode == 0, (result.stdout or result.stderr).strip()
    except (OSError, subprocess.TimeoutExpired) as error:
        return False, str(error)


def inspect(target):
    checks = []

    def check(name, ok, detail, fix):
        checks.append(dict(name=name, ok=bool(ok), detail=detail, fix='' if ok else fix))

    def tool(name, fix):
        path = shutil.which(name)
        check(name, path, path or 'Not found on PATH', fix)
        return path

    pinned = (ROOT / '.python-version').read_text().strip()
    check('Python', platform.python_version() == pinned, platform.python_version(),
          'Run uv sync --locked, then invoke this script with uv run.')
    tool('uv', 'Install uv: https://docs.astral.sh/uv/getting-started/installation/')

    if target == 'preview':
        try:
            manifest = json.loads((ROOT / 'docs/maplab/build.json').read_text())
            required = {'index.html', 'maplab.js', 'maplab.wasm', 'maplab.data'}
            if not required.issubset(manifest['artifacts']):
                raise ValueError('Incomplete Map Lab artifact manifest')
            for name in required:
                digest = hashlib.sha256((ROOT / 'docs/maplab' / name).read_bytes()).hexdigest()
                if digest != manifest['artifacts'][name]['sha256']:
                    raise ValueError(f'Hash mismatch: {name}')
            check('Packaged Map Lab', True, 'HTML, JS, WASM and policy data hashes match', '')
        except (OSError, ValueError, KeyError, TypeError) as error:
            check('Packaged Map Lab', False, str(error),
                  'Use a complete fresh clone, or rebuild with uv run scripts/build_fleet_site.py.')
        check('Learn page', (ROOT / 'docs/learn/index.html').is_file(), 'docs/learn/index.html',
              'Use the current checkout containing docs/learn/.')
    else:
        system = platform.system()
        check('Build platform', system in ('Darwin', 'Linux'), system,
              'Use macOS or Linux for builds; browser preview works without a compiler.')
        for name in ('git', 'bash', 'clang', 'make', 'curl', 'tar'):
            tool(name, f'Install {name} using your OS package manager; see docs/DEVELOPMENT.md.')
        omp_flags = ['-fopenmp']
        if system == 'Darwin':
            brew = tool('brew', 'Install Homebrew for the current macOS build scripts.')
            ok, prefix = command([brew, '--prefix', 'libomp']) if brew else (False, '')
            check('libomp', ok, prefix if ok else 'Homebrew libomp not found', 'brew install libomp')
            omp_flags = ['-Xclang', '-fopenmp', '-I' + prefix + '/include',
                         '-L' + prefix + '/lib', '-lomp']
        compiler = shutil.which('clang')
        if compiler and system in ('Darwin', 'Linux'):
            # Exercise headers and linking, not just the presence of a compiler.
            with tempfile.TemporaryDirectory(prefix='alienwars-doctor-') as directory:
                source = Path(directory) / 'probe.c'
                source.write_text('#include <omp.h>\nint main(void){return omp_get_max_threads()<1;}\n')
                ok, output = command([compiler, str(source), *omp_flags, '-o', str(Path(directory) / 'probe')])
                check('C + OpenMP link', ok, 'Compiler/link probe passed' if ok else output[-1200:],
                      'Install Clang and OpenMP development files; see docs/DEVELOPMENT.md.')
                if system == 'Linux':
                    source.write_text('#include <GL/gl.h>\nint main(void){glFinish();return 0;}\n')
                    ok, output = command([compiler, str(source), '-lGL', '-o', str(Path(directory) / 'gl-probe')])
                    check('OpenGL development files', ok, 'Header/link probe passed' if ok else output[-1200:],
                          'Install your distribution’s OpenGL development package; see docs/DEVELOPMENT.md.')
        if target == 'web':
            tool('unzip', 'Install unzip for the Raylib web archive.')
            tool('node', 'Install Node.js and put node on PATH for native/WASM checks.')
            emcc = shutil.which('emcc') or ROOT / '.local/emsdk/upstream/emscripten/emcc'
            ok, output = command([str(emcc), '--version'])
            check('Emscripten', ok, output.splitlines()[0] if output else 'Not installed',
                  'Install the isolated Emscripten 6.0.9 SDK using docs/WEB.md#install-the-compiler-once.')
            if ok and '6.0.9' not in output:
                check('Pinned Emscripten', False, 'This project verifies Emscripten 6.0.9',
                      'Select 6.0.9 using docs/WEB.md; other SDK versions are not validated here.')
        if target == 'train':
            check('CUDA training platform', system == 'Linux', system,
                  'Run native training on a Linux NVIDIA host; macOS supports playback and web builds.')
            tool('ccache', 'Install ccache for build.sh’s CUDA compiler wrapper.')
            cuda = Path(os.environ.get('CUDA_HOME', os.environ.get('CUDA_PATH', '/usr/local/cuda')))
            nvcc = shutil.which('nvcc') or cuda / 'bin/nvcc'
            ok, output = command([str(nvcc), '--version'])
            check('CUDA development toolkit', ok, output.splitlines()[-1] if output else 'Not found',
                  'Install the CUDA development toolkit and set CUDA_HOME; see docs/DEVELOPMENT.md.')
            gpu = shutil.which('nvidia-smi')
            ok, output = command([gpu, '--query-gpu=name', '--format=csv,noheader']) if gpu else (False, '')
            check('NVIDIA GPU', ok, output or 'No accessible NVIDIA GPU',
                  'Use your NVIDIA GPU host and check its driver with nvidia-smi.')
            check('NCCL headers', any(Path(p).is_file() for p in
                  (str(cuda / 'include/nccl.h'), '/usr/include/nccl.h', '/usr/local/include/nccl.h')),
                  'Checking standard NCCL development locations',
                  'Install NCCL development headers/libraries; custom layouts need the documented build setup.')
    return dict(target=target, platform=platform.system(), ok=all(c['ok'] for c in checks),
                checks=checks, next_command=NEXT[target],
                note='Local preflight only; builds and browser/GPU execution are validated separately.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', choices=NEXT, default='preview')
    parser.add_argument('--json', action='store_true', help='Machine-readable result; same exit status')
    args = parser.parse_args()
    report = inspect(args.target)
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(f"AlienWars / {args.target} / {report['platform']}")
        for item in report['checks']:
            print(f"  {'OK' if item['ok'] else 'MISSING'}  {item['name']}: {item['detail']}")
            if item['fix']:
                print(f"    Next: {item['fix']}")
        print('\n' + ('Preflight passed.' if report['ok'] else 'Resolve the missing prerequisites above.'))
        if report['ok']:
            print('From the repository root:\n  ' + report['next_command'])
            if args.target == 'preview':
                print('Then open http://127.0.0.1:8781/learn/ (Ctrl+C stops the server).')
        print(report['note'])
    return 0 if report['ok'] else 1


if __name__ == '__main__':
    sys.exit(main())
