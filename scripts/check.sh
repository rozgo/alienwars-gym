#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
for env in breakout minimal; do
    ./build.sh "$env" "build/$env-debug" --cpu --debug
    "./build/$env-debug" --headless
done
git diff --check
echo 'PASS: Breakout and Minimal CPU debug builds and 1024-step headless rollouts'
