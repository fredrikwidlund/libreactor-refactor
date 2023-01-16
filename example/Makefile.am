ACLOCAL_AMFLAGS			= ${ACLOCAL_FLAGS} -I m4L
AM_CFLAGS			= -std=gnu2x -g -O3 -flto -march=native -I../src -fPIC
AM_LDFLAGS			= -static
LDADD				= ../libreactor.la

bin_PROGRAMS 			= \
httpd-minimal \
http-client \
stream \
next \
async

#tls_server_raw_LDADD	= $(LDADD) -lssl -lcrypto

MAINTAINERCLEANFILES 	= Makefile.in
