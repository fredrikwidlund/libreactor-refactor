#ifndef REACTOR_H
#define REACTOR_H

#ifdef UNIT_TESTING
extern void mock_assert(const int result, const char* const expression,
                        const char * const file, const int line);
#undef assert
#define assert(expression) \
    mock_assert((int)(expression), #expression, __FILE__, __LINE__);
#endif

#define REACTOR_VERSION       "1.0.0"
#define REACTOR_VERSION_MAJOR 1
#define REACTOR_VERSION_MINOR 0
#define REACTOR_VERSION_PATCH 0

#include "reactor/buffer.h"
#include "reactor/vector.h"
#include "reactor/pool.h"
#include "reactor/reactor.h"

#endif /* REACTOR_H */
