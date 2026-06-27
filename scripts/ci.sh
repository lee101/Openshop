#!/usr/bin/env bash
set -euo pipefail

echo "==> Build Openshop"
make

echo "==> Run selftests"
make test

echo "==> Run SDL smoke test"
make test-sdl

echo "==> CI passed"
