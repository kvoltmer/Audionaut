# Audionaut
 

## CHECKOUT:

Make sure to clone with submodules:

```
git clone --recursive git@github.com:kvoltmer/Audionaut.git
```
or once cloned do:

```
git submodule update --init --recursive
```

## BUILD:

#### Xcode:

Open the Xcode project located here:

```
Audionaut/Builds/MacOSX/Audionaut.xcodeproj
```

#### Visual Studio 2022:

Open the Visual Studio solution located here:

```
Audionaut/Builds/VisualStudio2022/Audionaut.sln
```

#### Linux Makefile:

install dependencies:

```
sudo apt install libasound2-dev libjack-jackd2-dev ladspa-sdk libcurl4-openssl-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libwebkit2gtk-4.1-dev libglu1-mesa-dev mesa-common-dev
```

compile:

```
cd Audionaut/Builds/LinuxMakefile/
make -j8
```

## TESTS:

<a href="Audionaut/Catch2Tests/README.md">catch2 tests README</a>






