#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
test -d .venv || python3 -m venv .venv
source .venv/bin/activate
pip install -q -r requirements.txt
exec uvicorn server:app --host 0.0.0.0 --port 8787
