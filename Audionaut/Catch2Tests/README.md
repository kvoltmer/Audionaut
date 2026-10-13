

## tests using catch2

build using cmake (run from the source root folder):

```
cmake -B build -S Audionaut/Catch2Tests
cmake --build build
```

run:

```
./build/AudionautTests_artefacts/AudionautTests
```

run a single tag/scenario (tests are tagged, e.g. `[engine]`, `[cli]`, `[essentia]`):

```
./build/AudionautTests_artefacts/AudionautTests "[cli]"
```

or run everything through ctest (includes the CliSmoke binary test):

```
ctest --test-dir build --output-on-failure
```

## audionaut-cli

the same CMake project also builds the headless CLI (see the root README for
usage). It is on by default; `-DAUDIONAUT_BUILD_CLI=OFF` disables it, and
coverage builds skip it automatically.

```
cmake -B build -S Audionaut/Catch2Tests
cmake --build build --target AudionautCli
./build/AudionautCli_artefacts/AudionautCli --help
```

note: CMake globs the source files at configure time - after adding a new
.cpp (a test, a CLI command, or a new Source subdirectory) re-run the
`cmake -B build ...` configure step or the file won't be compiled.

update Xcode project (creates a xcode project in the "_build" folder):

```
cmake -G Xcode -H. -B_build
```

if you run into this linker error:
clang: error: no such file or directory: '/Users/klausvoltmer/dev/Audium/Lola/Catch2Tests/_build/catch2-bin/src/Debug/libCatch2Maind.a'
clang: error: no such file or directory: '/Users/klausvoltmer/dev/Audium/Lola/Catch2Tests/_build/catch2-bin/src/Debug/libCatch2d.a'

-> open the catch2 code project an compile it (_build/catch2-bin/Catch2.xcodeproj)

note:
you can use find_package(Catch2) in case catch2 is installed

XCode filter arguments when running tests:

```
-r console [Filter] -d yes
```
