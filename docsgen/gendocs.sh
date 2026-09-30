#!/bin/bash

# Gendocs - Scribbolyth's mini-doxygen.
# Always a FULL rebuild: copies config/scribboleth.html fresh, harvests the
# doc comments from src/, and writes the result to developers/devnotes.html.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

exec "$SCRIPT_DIR/run.sh"
