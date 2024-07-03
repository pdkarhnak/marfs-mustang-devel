CC := gcc
CFLAGS := -Wall -O2 -g -DDEBUG
LDFLAGS := 
LDLIBS := -lpthread

# Should be defined according to the MarFS install prefix, i.e., the argument
# passed to `./configure --prefix=` when installing MarFS
MARFS_INSTALL_PATH := /opt/campaign/install/

# Target the MarFS-relevant include and source dirs for symbol definitions
MARFS_CFLAGS := -I$(MARFS_INSTALL_PATH)/include 
MARFS_SRC_INCLUDE := -I/$(MARFS_INSTALL_PATH)/marfs/src/
INCLUDE_XML := -I/usr/include/libxml2/
MARFS_GENERAL_INCLUDE := $(MARFS_SRC_INCLUDE) $(INCLUDE_XML)

# Link against MarFS libs using the system-specific installation prefix
MARFS_LDFLAGS := -L$(MARFS_INSTALL_PATH)/lib

# mustang_engine will be called by the mustang frontend/wrapper, but perform
# all the significant computational work via the thread routines
all: mustang_engine

mustang_engine: mustang_engine.c thread_main.o libmustang.a
	$(CC) $(CFLAGS) $(MARFS_CFLAGS) $(MARFS_GENERAL_INCLUDE) $< thread_main.o -o $@ \
		$(LDFLAGS) -lpthread -L. -lmustang $(MARFS_LDFLAGS) \
		-Wl,-rpath,$(MARFS_INSTALL_PATH)/lib -lmarfs -llogging

# A static library bundling all the data structures and threading code that the
# mustang engine relies on. mustang *must* still be invoked via the
# mustang_engine executable and/or via its "mustang" frontend---use in other
# applications by simply linking against the static library is insufficient
libmustang.a: pthread_vector.o mustang_threading.o hashtable.o retcode_ll.o ar
	rcs $@ $^

# thread_main and retcode_ll (specifically retcode_ll_flush) need to link 
# against the logging library. So, create special rules for those object files.
thread_main.o: thread_main.c
	$(CC) $(CFLAGS) $(MARFS_GENERAL_INCLUDE) -c $< -llogging

retcode_ll.o: retcode_ll.c retcode_ll.h
	$(CC) $(CFLAGS) $(MARFS_GENERAL_INCLUDE) -c $< -llogging

%.o: %.c %.h
	$(CC) $(CFLAGS) $(MARFS_GENERAL_INCLUDE) $< -c $(LDLIBS)

clean:
	rm -f libmustang.a ./*.o mustang_engine ./*.tar.gz
	rm -f ./mustang-output-* ./*.log

archive:
	make clean
	./archive.sh
