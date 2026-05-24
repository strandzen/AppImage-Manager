#!/usr/bin/env bash
# Build the dev preset and run the CTest suite. Intended for CI; safe to run locally.
#
# Exit code: non-zero if configure, build, or any test fails.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

cmake --preset dev
cmake --build --preset dev
ctest --test-dir build/dev --output-on-failure
