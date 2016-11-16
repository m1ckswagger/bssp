#!/bin/bash

make
mkdir -p important_stuff/a/b/c/d/e/f/g
cp bkupentwurf.pdf important_stuff/
cp Makefile important_stuff/a
cp bkup.c important_stuff/a/b
mkfifo mynamedpipe

export BACKUPTARGET="archive"
./bkup bkupentwurf.pdf Makefile mynamedpipe


cd testdir
../restore ../archive

cd ..
make clean
rm -rf important_stuff
