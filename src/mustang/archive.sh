#!/usr/bin/bash

timestamp=$(date +%Y-%m-%d-%H-%M-%S)
dir_stem="mustang-build-$timestamp" 

mkdir ./$dir_stem
cp ./*.c ./$dir_stem
cp ./*.h ./$dir_stem
cp ./mustang ./$dir_stem
tar -cvz ./$dir_stem/* -f "$dir_stem.tar.gz" --force-local
rm -r ./$dir_stem

