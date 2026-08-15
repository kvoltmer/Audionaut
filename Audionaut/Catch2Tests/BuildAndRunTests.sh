
#!/bin/bash

set -e

# Directories, paths and filenames
#CMAKE_PATH = "/opt/homebrew/bin/"

BUILD_DIR=cmake-build

cmake -B ${BUILD_DIR}
cmake --build ${BUILD_DIR}
