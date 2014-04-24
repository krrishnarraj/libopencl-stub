CC = gcc
RANLIB = ranlib

LIBSRC = libopencl.c
LIBOBJ=$(LIBSRC:.c=.o)

CFLAGS = -O2 -I ./include

LIBOPENCL = libOpenCL.a
TARGETS = $(LIBOPENCL)

all: $(TARGETS)

libopencl.o: libopencl.c
	$(CC) $(CFLAGS) -c libopencl.c -o libopencl.o

$(TARGETS): $(LIBOBJ)
	ar rcs $(LIBOPENCL) libopencl.o
	ranlib $(LIBOPENCL)

clean:
	rm -f libopencl.o $(TARGETS) $(LIBOPENCL) $(LIBOBJ)

