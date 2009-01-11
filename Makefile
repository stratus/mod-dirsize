#
# Set APXS to apxs full path on your system
#
APXS=apxs

all: mod_dirsize.so;

mod_dirsize.so: mod_dirsize.c
	${APXS} -a -c $<

install:
	apxs -i mod_dirsize.so

clean:
	rm -fr *~ *.o *.so
