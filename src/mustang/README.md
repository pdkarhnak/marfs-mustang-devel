# MUSTANG version 1.2.0

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

The `DEBUG_MUSTANG` flag in `mustang_logging.h` sets the verbosity of debug
output in various `mustang` utilities (specifically, `mustang_engine.c`,
`thread_main.c`, and `mustang_monitors.c`):
* `#define DEBUG_MUSTANG 1` prints debug output of all priorities
* `#define DEBUG_MUSTANG 2` prints just priority `LOG_ERR` and `LOG_WARNING`
  debug messages
* `#define DEBUG_MUSTANG 3` prints just priority `LOG_ERR` debug messages

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
executable attempts no substantive argument parsing. Instead, use the `mustang`
frontend, which will appropriately parse arguments and can print help
information.

The `mustang` frontend requires at least one absolute path argument
corresponding to an active MarFS location where traversal will begin. The
frontend will not check whether the absolute path actually maps to the MarFS
instance; rather, such an error will likely be caught within the engine itself
and reported as a failed call to the internal MarFS traversal routine. For a
complete listing of arguments and usage information, see `mustang -h`.

## Usage considerations

`-t` and its aliases (threads) represent the number of worker threads that will
be pooled to accept and execute traversal tasks throughout the target
filesystem. Any positive, nonzero integer less than or equal to $2^63$ will be 
_accepted_; however, the application will warn about excessively large argument
values. Be aware of system limits on the number of concurrent threads which 
may be created per process (e.g., those in `/proc/sys/kernel/threads-max` or 
`/proc/sys/vm/max_map_count`) and pass argument values responsibly.

`-hc` and its aliases (hashtable capacity) represent the _power of two_ that
the frontend computes to get the hashtable capacity. Hashtable capacity should
be specified proportionately to the anticipated number of MarFS objects that
will be encountered. Specifying small hashtable capacities for large targets
_will_ work (i.e., produce a complete hashtable on output) due to the
hashtable's separate chaining capabilities, but this is likely to needlessly
slow hashtable operations (and, therefore application performance) due to
requiring linear traversal and chaining operations for a progressively greater
frequency of hash collisions as the application runs.

`-tc` and its aliases (task queue capacity) correspond to the maximum length of
the thread pool's task queue that will be allowed before `pthread_cond_wait()`
calls when enqueueing tasks will "take effect" (i.e., force callers to sleep
and wait on the corresponding condition variable tied to available space). By
default, the task queue is effectively unbounded since the capacity is set to
Linux `SIZE_MAX` (i.e., $2^{64} - 1$). Since the queue dynamically expands as
tasks are enqueued, most applications need not worry about using an unbounded
task queue. In fact, if an insufficiently large task queue is specified, the
application may enter a state of livelock or deadlock as threads circularly
wait to enqueue tasks based on other threads' ability to dequeue tasks.

By default, output and logging files will be named based on timestamps recorded
at the beginning of the program run. This is the recommended usage so that logs 
and output files (i.e., files detailing hashtable contents) from multiple runs 
may be kept without being overwritten.

# Acknowledgments

This work would not be possible without Garrett Ransom and Dave Bonnie, who 
served as mentors during initial development from May 2024--August 2024 at 
Los Alamos National Laboratory.

# Universal Release

MarFS was originally developed for Los Alamos National Laboratory (LANL) and is
copyright (c) 2015, Los Alamos National Security, LLC with all rights reserved.
MarFS is released under the BSD License and has been reviewed and released by
LANL under Los Alamos Computer Code identifier LA-CC-15-039. This work
(mustang) is a MarFS utility, and is therefore released alongside MarFS under
the same conditions.
