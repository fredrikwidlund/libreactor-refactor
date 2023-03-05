AUTOMAKE_OPTIONS		= subdir-objects
ACLOCAL_AMFLAGS			= ${ACLOCAL_FLAGS} -I m4
AM_CFLAGS			= -std=gnu2x -g -O3 -flto -march=native -I$(srcdir)/src -fPIC
AM_LDFLAGS			= -static

SOURCE_FILES = \
src/picohttpparser/picohttpparser.c \
src/reactor/reactor.c \
src/reactor/data.c \
src/reactor/hash.c \
src/reactor/map.c \
src/reactor/mapi.c \
src/reactor/maps.c \
src/reactor/list.c \
src/reactor/buffer.c \
src/reactor/vector.c \
src/reactor/pool.c \
src/reactor/resolver.c \
src/reactor/event.c \
src/reactor/timer.c \
src/reactor/stream.c \
src/reactor/http.c \
src/reactor/server.c

HEADER_FILES = \
src/picohttpparser/picohttpparser.h \
src/reactor/reactor.h \
src/reactor/data.h \
src/reactor/hash.h \
src/reactor/map.h \
src/reactor/mapi.h \
src/reactor/maps.h \
src/reactor/list.h \
src/reactor/buffer.h \
src/reactor/vector.h \
src/reactor/pool.h \
src/reactor/resolver.h \
src/reactor/event.h \
src/reactor/timer.h \
src/reactor/stream.h \
src/reactor/http.h \
src/reactor/server.h

lib_LTLIBRARIES 		= libreactor.la
libreactor_la_SOURCES 		= $(SOURCE_FILES) $(HEADER_FILES)

headerfilesdir 			= $(includedir)/reactor
headerfiles_HEADERS 		= $(HEADER_FILES)

mainheaderdir 			= $(includedir)
mainheader_HEADERS 		= src/reactor.h

pkgconfigdir 			= $(libdir)/pkgconfig
pkgconfig_DATA 			= libreactor.pc

MAINTAINERCLEANFILES 		= aclocal.m4 config.h.in configure Makefile.in libreactor-?.?.?.tar.gz

CLEANFILES 			= {.,test/,src/reactor/,src/picohttpparser}/*.{gcno,gcda,gcov}

maintainer-clean-local:
	rm -rf autotools m4 libreactor-?.?.?

indent:
	clang-format -i src/reactor/*.c test/*.c

### unit testing ###

CHECK_CFLAGS 			= -std=gnu2x -g -O0 -ftest-coverage -fprofile-arcs -I$(srcdir)/src -DGCOV_BUILD -DUNIT_TESTING
CHECK_LDADD 			= -L. libreactor_test.a -lcmocka

check_LIBRARIES 		= libreactor_test.a
libreactor_test_a_CFLAGS 	= $(CHECK_CFLAGS)
libreactor_test_a_SOURCES 	= $(SOURCE_FILES) $(HEADER_FILES)

check_PROGRAMS 			=

check_PROGRAMS			+= test/reactor
test_reactor_CFLAGS 		= $(CHECK_CFLAGS)
test_reactor_LDADD 		= $(CHECK_LDADD)

check_PROGRAMS			+= test/data
test_data_CFLAGS 		= $(CHECK_CFLAGS)
test_data_LDADD 		= $(CHECK_LDADD)

check_PROGRAMS			+= test/hash
test_hash_CFLAGS 		= $(CHECK_CFLAGS)
test_hash_LDADD 		= $(CHECK_LDADD)

check_PROGRAMS			+= test/map
test_map_CFLAGS 		= $(CHECK_CFLAGS)
test_map_LDADD 			= $(CHECK_LDADD)

check_PROGRAMS			+= test/mapi
test_mapi_CFLAGS 		= $(CHECK_CFLAGS)
test_mapi_LDADD 		= $(CHECK_LDADD)

check_PROGRAMS			+= test/maps
test_maps_CFLAGS 		= $(CHECK_CFLAGS)
test_maps_LDADD 		= $(CHECK_LDADD)

check_PROGRAMS			+= test/list
test_list_CFLAGS 		= $(CHECK_CFLAGS)
test_list_LDADD 		= $(CHECK_LDADD)

check_PROGRAMS			+= test/buffer
test_buffer_CFLAGS 		= $(CHECK_CFLAGS)
test_buffer_LDADD 		= $(CHECK_LDADD)

check_PROGRAMS			+= test/vector
test_vector_CFLAGS 		= $(CHECK_CFLAGS)
test_vector_LDADD 		= $(CHECK_LDADD)

check_PROGRAMS			+= test/pool
test_pool_CFLAGS 		= $(CHECK_CFLAGS)
test_pool_LDADD 		= $(CHECK_LDADD)

check_PROGRAMS			+= test/resolver
test_resolver_CFLAGS 		= $(CHECK_CFLAGS)
test_resolver_LDADD 		= $(CHECK_LDADD)

check_PROGRAMS			+= test/timer
test_timer_CFLAGS 		= $(CHECK_CFLAGS)
test_timer_LDADD 		= $(CHECK_LDADD)

check_PROGRAMS			+= test/stream
test_stream_CFLAGS 		= $(CHECK_CFLAGS)
test_stream_LDADD 		= $(CHECK_LDADD)

check_PROGRAMS			+= test/http
test_http_CFLAGS 		= $(CHECK_CFLAGS)
test_http_LDADD 		= $(CHECK_LDADD)

check_PROGRAMS			+= test/server
test_server_CFLAGS 		= $(CHECK_CFLAGS)
test_server_LDADD 		= $(CHECK_LDADD)

check_PROGRAMS			+= test/event
test_event_CFLAGS 		= $(CHECK_CFLAGS)
test_event_LDADD 		= $(CHECK_LDADD)

TESTS 				= $(check_PROGRAMS) test/valgrind.sh test/coverage.sh
