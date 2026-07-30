#!/bin/bash

# Requirements:
# sudo apt install -y mingw-w64

source ./config.sh

set -e

echo "Building $APP_NAME for Windows..."

BUILD_TYPE="Debug"
if [ "$1" == "r" ]; then
    BUILD_TYPE="Release"
fi
echo "Performing $BUILD_TYPE build."

TOOLCHAIN="$(dirname "$0")/cmake/mingw-x86_64.cmake"

mkdir -p build-windows

# Generate CMakeLists.txt from template
cp CMakeLists.txt.in CMakeLists.txt
sed -i "s/<<TARGET_NAME>>/$APP_NAME/g" CMakeLists.txt
SOURCES_TMP=$(mktemp)
for s in "${SOURCES[@]}"; do
    echo "    $s" >> "$SOURCES_TMP"
done
sed -i "/^<<SOURCES>>$/{
    r $SOURCES_TMP
    d
}" CMakeLists.txt
rm -f "$SOURCES_TMP"
for lib in "${LIBS[@]}"; do
    if [[ "$lib" == *::* ]]; then
        echo "target_link_libraries($APP_NAME PRIVATE $lib)" >> CMakeLists.txt
    else
        echo "target_link_libraries($APP_NAME PRIVATE \${CMAKE_SOURCE_DIR}/$lib)" >> CMakeLists.txt
    fi
done

cmake -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
      -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      -B build-windows \
      -S .

cmake --build build-windows

echo "Build complete: build-windows/bin/$APP_NAME.exe"
