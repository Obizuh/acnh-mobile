#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
PY="${ACNH_PYTHON:-python3}"
if [ -z "${ACNH_PYTHON:-}" ] && [ -x .venv/bin/python ]; then PY="$PWD/.venv/bin/python"; fi
exec "$PY" tools/one_click.py ipa "$@"
