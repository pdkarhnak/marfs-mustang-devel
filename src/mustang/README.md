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

# MarFS

This tool requires a running MarFS instance on the target machine or cluster.
Naturally, MUSTANG requires [MarFS](https://github.com/mar-file-system) and its
dependencies. Installation instructions and documentation can be found
[here](http://mar-file-system.github.io/marfs/new_install.html).

# Modifying mustang

Modifying the source files for `mustang_engine.c` and the dependencies is
generally discouraged due to the strong interdependence of the various source
and header files on one another. However, some specific constants are likely
necessary to modify to accommodate testing constraints.

The `DEBUG_MUSTANG` flag in `mustang_logging.h` sets the verbosity of debug output in various `mustang` utilities (specifically, `mustang_engine.c`, `thread_main.c`, and `retcode_ll.c`):
* `#define DEBUG_MUSTANG 1` prints debug output of all priorities
* `#define DEBUG_MUSTANG 2` prints just priority `LOG_ERR` and `LOG_WARNING` debug messages
* `#define DEBUG_MUSTANG 3` prints just priority `LOG_ERR` debug messages

`RC_LL_LEN_MAX` in `retcode_ll.h` controls the amount of return code linked
list nodes (see `retcode_ll.h` for documentation) accumulated before flushing
nodes and their associated return code indicators to the program log file.
Greater values offer better optimization of log file writes since locking will
occur on a shared file "per batch", i.e., per flush of an entire linked list.
However, specifying greater values `RC_LL_LEN_MAX` creates the risk of losing
larger batches if a crash or error condition occurs during a write.

Additionally, the `KEY_SEED` constant in `hashtable.h` may be modified to
better seed the underlying MurmurHash3 algorithm which is invoked to map
entries to hash nodes.

# Building mustang

The current build system for `mustang` is a local `Makefile`; however, the
build process may be integrated with the general MarFS build system in a future
release.

Assuming that building with the `Makefile`, is necessary, users must edit the
`MARFS_PREFIX` variable within the provided `Makefile` to match the path which
was passed to the `--prefix=` argument when running `./configure` to build
MarFS. Other macros within the Makefile are defined relative to `MARFS_PREFIX`
and should not need to be edited. Some adjustment of the `INCLUDE_XML` macro
may be needed to match a different `libxml2` installation path on your system.

Simply running `make` followed by `make install` will properly build all
targets (`libmustang.a`, binary `mustang_engine`, and frontend `mustang`) and
copy them to accessible MarFS bin and library locations.

# Running mustang

Directly invoking the `mustang_engine` executable is discouraged since the
executable attempts no argument parsing. Instead, use the `mustang` frontend, 
which will appropriately parse arguments and can print help information.

The `mustang` frontend requires at least one absolute path argument
corresponding to an active MarFS location where traversal will begin. The
frontend will not check whether the absolute path actually maps to the MarFS
instance; rather, such an error will likely be caught within the engine itself
and reported as a failed call to the internal MarFS traversal routine. For a
complete listing of arguments and usage information, see `mustang -h`.

# Acknowledgments

# Universal Release

MarFS was originally developed for Los Alamos National Laboratory (LANL) and is
copyright (c) 2015, Los Alamos National Security, LLC with all rights reserved.
MarFS is released under the BSD License and has been reviewed and released by
LANL under Los Alamos Computer Code identifier LA-CC-15-039. This work
(mustang) is a MarFS utility, and is therefore released alongside MarFS under
the same conditions.
