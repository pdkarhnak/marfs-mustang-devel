CC := gcc
MARFS_INSTALL_PATH := /opt/campaign/install/
MARFS_CFLAGS := -I$(MARFS_INSTALL_PATH)/include 
MARFS_SRC_INCLUDE := -I/$(MARFS_INSTALL_PATH)/marfs/src/
INCLUDE_XML := -I/usr/include/libxml2/
MARFS_GENERAL_INCLUDE := $(MARFS_SRC_INCLUDE) $(INCLUDE_XML)
CFLAGS := -Wall -O2 -g -DDEBUG
LDFLAGS := 
MARFS_LDFLAGS := -L$(MARFS_INSTALL_PATH)/lib
LDLIBS := -lpthread

all: mustang_engine
	
mustang_engine: mustang_engine.c thread_main.o libmustang.a
	$(CC) $(CFLAGS) $(MARFS_CFLAGS) $(MARFS_GENERAL_INCLUDE) $< thread_main.o -o $@ \
		$(LDFLAGS) -lpthread -L. -lmustang $(MARFS_LDFLAGS) \
		-Wl,-rpath,$(MARFS_INSTALL_PATH)/lib -lmarfs -llogging

libmustang.a: pthread_vector.o mustang_threading.o hashtable.o retcode_ll.o
	ar rcs $@ $^

thread_main.o: thread_main.c
	$(CC) $(CFLAGS) $(MARFS_GENERAL_INCLUDE) -c $< -llogging

%.o: %.c %.h
	$(CC) $(CFLAGS) $(MARFS_GENERAL_INCLUDE) $< -c $(LDLIBS)

clean:
	rm -f libmustang.a ./*.o mustang_engine ./*.tar.gz
	rm -f ./mustang-output-* ./*.log

archive:
	make clean
	./archive.sh
