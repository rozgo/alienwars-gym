#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
./build.sh alienwars build/maplab --cpu --debug
./build/maplab --headless --seed=73
./build.sh minimal build/minimal-debug --cpu --debug
./build/minimal-debug --headless
git diff --check
echo 'PASS: Map Lab CPU debug generation and Minimal 1024-step headless rollout'
