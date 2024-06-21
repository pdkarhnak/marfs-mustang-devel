CC := gcc
MARFS_PREFIX := /opt/campaign/install/
MARFS_CFLAGS := -I$(MARFS_PREFIX)/include -I/$(MARFS_PREFIX)/src
CFLAGS := -Wall -pedantic -O2 -g -DDEBUG
LDFLAGS := #-fsanitize=address
MARFS_LDFLAGS := -L$(MARFS_PREFIX)/lib
LDLIBS := -lpthread

all: mustang_engine
	
mustang_engine: mustang_engine.c thread_main.o libmustang.a
	$(CC) $(CFLAGS) $(MARFS_CFLAGS) $< thread_main.o -o $@ $(LDFLAGS) -lpthread -L. -lmustang $(MARFS_LDFLAGS) -Wl,-rpath,$(MARFS_PREFIX)/lib -lmarfs

libmustang.a: pthread_vector.o mustang_threading.o hashtable.o retcode_ll.o
	ar rcs $@ $^

thread_main.o: thread_main.c
	$(CC) $(CFLAGS) -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) $< -c $(LDLIBS)

clean:
	rm -f libmustang.a ./*.o mustang_engine ./*.tar.gz
	rm -f ./mustang-output-* ./*.log

archive:
	make clean
	./archive.sh
