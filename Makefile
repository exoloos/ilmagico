CC=gcc
CFLAGS=-O1
LIBS=-lm -lnettle -lgmp

OBJ = lib-misc.o smpc.o smpc_io.o test.o 
DEPENDECIES = smpc.h bitut.h lib-misc.h smpc_io.h


%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

test: $(OBJ) $(DEPENDECIES)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f *.o
