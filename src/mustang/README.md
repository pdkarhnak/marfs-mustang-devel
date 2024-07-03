# MUSTANG

Welcome to **MUSTANG**!
* **M**arFS (_or **M**archive/**M**etadata_)
* **U**nderlying
* **S**torage
* **T**ree
* **A**nd
* **N**amespace
* **G**atherer

This is a tool originally designed to traverse a MarFS metadata reference tree
and generate a list of relevant MarFS objects when given paths within a MarFS
instance. Why the name MUSTANG? A Mustang is a car, and cars traverse!

## MarFS

This tool requires a running MarFS instance on the target machine or cluster.
Naturally, MUSTANG requires [MarFS](https://github.com/mar-file-system) and its
dependencies. Installation instructions and documentation can be found [here](http://mar-file-system.github.io/marfs/new_install.html).

## Building mustang

The current build system for `mustang` is a local `Makefile`; however, the
build process may be integrated with the general MarFS build system in a future
release.

Assuming that building with the `Makefile`, is necessary, users must edit the
`MARFS_PREFIX` variable within the provided `Makefile` to match the path which
was passed to the `--prefix=` argument when running `./configure` to build
MarFS. Other macros within the Makefile are defined relative to `MARFS_PREFIX`
and should not need to be edited. Some adjustment of the `INCLUDE_XML` macro
may be needed to match a different `libxml2` installation path on your system.

Simply running `make` 

## Acknowledgments

## Universal Release

MarFS was originally developed for Los Alamos National Laboratory (LANL) and
is copyright (c) 2015, Los Alamos National Security, LLC with all rights
reserved. MarFS is released under the BSD License and has been reviewed and
released by LANL under Los Alamos Computer Code identifier LA-CC-15-039. This work (mustang) is a MarFS utility, and is therefore released alongside MarFS under the same conditions.
