CC = gcc
RANLIB = ranlib

LIBSRC = libopencl.c
LIBOBJ=$(LIBSRC:.c=.o)

CFLAGS = -O2 -fPIC -I ./include

LIBOPENCL = libOpenCL.a
TARGETS = $(LIBOPENCL)

all: $(TARGETS)

libopencl.o: libopencl.c
	$(CC) $(CFLAGS) -c libopencl.c -o libopencl.o

$(TARGETS): $(LIBOBJ)
	ar rcs $(LIBOPENCL) libopencl.o
	$(RANLIB) $(LIBOPENCL)

clean:
	rm -f $(TARGETS) $(LIBOBJ)

