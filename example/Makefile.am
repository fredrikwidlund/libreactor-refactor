ACLOCAL_AMFLAGS			= ${ACLOCAL_FLAGS} -I m4L
AM_CFLAGS			= -std=gnu2x -g -O3 -flto -march=native -I../src -fPIC
AM_LDFLAGS			= -static
LDADD				= ../libreactor.la

bin_PROGRAMS 			= \
httpd-minimal \
http-client \
next \
async \
lookup \
timer \
queue \
server \
cancel \
event \
notify

MAINTAINERCLEANFILES 		= Makefile.in
