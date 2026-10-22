#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
exec "${ACNH_PYTHON:-python3}" tools/public_setup.py "$@"
