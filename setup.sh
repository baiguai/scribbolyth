#!/bin/bash

set -e

echo "=== scribbolyth setup ==="

# Check prerequisites
command -v cmake >/dev/null 2>&1 || { echo "ERROR: cmake is required (3.16+). Install it and try again."; exit 1; }
CMAKE_VER=$(cmake --version | head -1 | grep -oP '\d+\.\d+')
if (( $(echo "$CMAKE_VER < 3.16" | bc -l) )); then
    echo "ERROR: cmake 3.16+ required (found $CMAKE_VER)"
    exit 1
fi
echo "[OK] cmake $CMAKE_VER"

if command -v g++ &>/dev/null; then
    echo "[OK] g++ $(g++ -dumpversion)"
elif command -v clang++ &>/dev/null; then
    echo "[OK] clang++"
else
    echo "ERROR: no C++ compiler found (g++ or clang++)"
    exit 1
fi


echo ""
echo "=== Setup complete ==="
echo "Run ./build.sh to build."
