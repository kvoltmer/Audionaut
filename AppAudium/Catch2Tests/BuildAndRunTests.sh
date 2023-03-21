
#!/bin/bash

set -e

# Directories, paths and filenames
#CMAKE_PATH = "/opt/homebrew/bin/"

BUILD_DIR=cmake-build

/opt/homebrew/bin/cmake -B ${BUILD_DIR}
/opt/homebrew/bin/cmake --build ${BUILD_DIR}
