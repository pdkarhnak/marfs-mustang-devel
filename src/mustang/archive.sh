#!/usr/bin/bash

mkdir ./archive
cp ./*.c ./archive
cp ./*.h ./archive
cp ./mustang ./archive
timestamp=$(date +%Y-%m-%d-%H-%M-%S)
tar -cvz ./archive/* -f "mustang-build-$timestamp.tar.gz" --force-local
rm -r ./archive	
