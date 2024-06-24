CC := gcc
MARFS_INSTALL_PATH := /opt/campaign/install/
MARFS_CFLAGS := -I$(MARFS_INSTALL_PATH)/include 
MARFS_SRC_INCLUDE := -I/$(MARFS_INSTALL_PATH)/marfs/src
CFLAGS := -Wall -pedantic -O2 -g -DDEBUG
LDFLAGS := #-fsanitize=address
MARFS_LDFLAGS := -L$(MARFS_INSTALL_PATH)/lib
INCLUDE_XML := -I/usr/include/libxml2/
LDLIBS := -lpthread

all: mustang_engine
	
mustang_engine: mustang_engine.c thread_main.o libmustang.a
	$(CC) $(CFLAGS) $(MARFS_CFLAGS) $(MARFS_SRC_INCLUDE) $(INCLUDE_XML) $< thread_main.o -o $@ \
		$(LDFLAGS) -lpthread -L. -lmustang $(MARFS_LDFLAGS) \
		-Wl,-rpath,$(MARFS_INSTALL_PATH)/lib -lmarfs

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
