#!/usr/bin/env bash
# Builds the J6 firmware package into build/. Needs JIELI_TOOLCHAIN and AC79_SDK (the CI workflow sets both).
#   ./build.sh                    build/j6.fwsc
#   ./build.sh --release 1.0.0    build/j6-1.0.0.fwsc, identity FM-1_66010000
set -euo pipefail
cd "$(dirname "$0")"
exec python3 fm1-base/tools/build.py . "$@"
