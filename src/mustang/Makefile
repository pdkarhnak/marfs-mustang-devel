CC := gcc
MARFS_PREFIX := /opt/campaign/install/
MARFS_CFLAGS := -I$(MARFS_PREFIX)/include -I/$(MARFS_PREFIX)/src
CFLAGS := -Wall -pedantic -O2 -g -DDEBUG
LDFLAGS := #-fsanitize=address
MARFS_LDFLAGS := -L$(MARFS_PREFIX)/lib
LDLIBS := -lpthread

all: mustang_engine
	
mustang_engine: mustang_engine.c libmustang.a
	$(CC) $(CFLAGS) $(MARFS_CFLAGS) $< -o $@ $(LDFLAGS) -lpthread -L. -lmustang $(MARFS_LDFLAGS) -Wl,-rpath,$(MARFS_PREFIX)/lib -lmarfs

libmustang.a: pthread_vector.o mustang_threading.o hashtable.o namecache.o
	ar rcs $@ $^

%.o: %.c %.h
	$(CC) $(CFLAGS) $< -c $(LDLIBS)

clean:
	rm -f libmustang.a ./*.o mustang_engine ./*.tar.gz
	rm -f mustang-output-*

archive:
	make clean
	./archive.sh
