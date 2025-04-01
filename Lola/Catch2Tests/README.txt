
tests using catch2

build using cmake:
cmake -B my-build
cmake --build my-build

run:
./my-build/AudiumTests_artefacts/AudiumTests

update Xcode project:
cmake -G Xcode -H. -B_build

if you run into this linker error:
clang: error: no such file or directory: '/Users/klausvoltmer/dev/Audium/Lola/Catch2Tests/_build/catch2-bin/src/Debug/libCatch2Maind.a'
clang: error: no such file or directory: '/Users/klausvoltmer/dev/Audium/Lola/Catch2Tests/_build/catch2-bin/src/Debug/libCatch2d.a'

-> open the catch2 code project an compile it (_build/catch2-bin/Catch2.xcodeproj)

note:
you can use find_package(Catch2) in case catch2 is installed

XCode arguments:
-r console [Filter] -d yes
