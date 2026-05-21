#!/bin/sh
set -e

# clean
rm -r ../LinuxMakefile/build

# build release
make CONFIG=Release -j8 -C ../LinuxMakefile/
make strip -C ../LinuxMakefile/

# copy Audionaut to this directory -> lower case "audionaut"
cp ../LinuxMakefile/build/audionaut audionaut

# create debian package
equivs-build audionaut-equvis.conf





