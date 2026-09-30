#!/bin/bash

# Runs the docsgen harvester inside its own virtual environment
# (docsgen/.venv), creating and provisioning it on first use.
# Drop dependencies into docsgen/requirements.txt when we need them.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/.venv"
REQ="$SCRIPT_DIR/requirements.txt"

# Windows (git-bash / MSYS) venvs keep the interpreter under Scripts/
if [ -x "$VENV_DIR/Scripts/python.exe" ]; then
    PY="$VENV_DIR/Scripts/python.exe"
elif [ -x "$VENV_DIR/bin/python" ]; then
    PY="$VENV_DIR/bin/python"
else
    PY=""
fi

if [ -z "$PY" ]; then
    echo "Creating virtual environment at $VENV_DIR ..."
    python3 -m venv "$VENV_DIR"
    if [ -x "$VENV_DIR/Scripts/python.exe" ]; then
        PY="$VENV_DIR/Scripts/python.exe"
    else
        PY="$VENV_DIR/bin/python"
    fi
fi

if [ -f "$REQ" ]; then
    echo "Installing dependencies from requirements.txt ..."
    "$PY" -m pip install --quiet --upgrade pip
    "$PY" -m pip install --quiet -r "$REQ"
fi

exec "$PY" "$SCRIPT_DIR/harvest.py"