CC := gcc
CFLAGS := -Wall -pedantic -O2 -g -DDEBUG
LDFLAGS := -fsanitize=address
LDLIBS := -lpthread

all: mustang_engine
	
mustang_engine: mustang_engine.c libmustang.a
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread -L. -lmustang

libmustang.a: pthread_vector.o mustang_threading.o hashtable.o
	ar rcs $@ $^

%.o: %.c %.h
	$(CC) $(CFLAGS) $< -c $(LDLIBS)

clean:
	rm -f libmustang.a ./*.o mustang_engine ./*.tar.gz

archive:
	make clean
	./archive.sh
