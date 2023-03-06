#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <string.h>

#include <cmocka.h>

#include "reactor.h"

void destroy(void *arg)
{
  free(*(void **)arg);
}

static void test_object(__attribute__((unused)) void **arg)
{
  int *o;
  char *s, **so;

  o = object_create(NULL, sizeof(int), 42, NULL);
  *o = 4711;
  assert_int_equal(object_data(o), 42);
  assert_int_equal(*o, 4711);
  object_hold(o);
  object_release(o);
  object_release(o);

  s = malloc(5);
  strcpy(s, "test");
  so = object_create(&s, sizeof s, 1, destroy);
  assert_string_equal(*so, "test");
  object_release(so);
}

int main()
{
  const struct CMUnitTest tests[] =
      {
          cmocka_unit_test(test_object)
      };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
