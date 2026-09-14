#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [ "$#" -lt 1 ] || [ ! -f "$1" ]; then
    echo 'Usage: ./scripts/play_breakout.sh CHECKPOINT.bin [--headless --base.eval_episodes=10]' >&2
    exit 2
fi
mkdir -p build
./build.sh breakout build/breakout --cpu
exec ./build/breakout "$@"
