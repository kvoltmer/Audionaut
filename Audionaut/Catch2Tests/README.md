

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
