#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <string.h>

#include <cmocka.h>

#include "reactor.h"

static void test_data(__attribute__((unused)) void **arg)
{
  data d;

  d = data_null();
  assert_true(data_empty(d));

  d = data_string("test");
  assert_true(strcmp(data_base(d), "test") == 0);
  assert_true(data_equal(data_offset(d, 2), data_string("st")));

  /* compare */
  assert_true(data_equal(data_string("test"), data_string("test")));
  assert_false(data_equal(data_string("test1"), data_string("test2")));
  assert_false(data_equal(data_string("test"), data_string("test2")));

  assert_true(data_equal_case(data_string("TeSt"), data_string("test")));
  assert_false(data_equal_case(data_string("TeSta"), data_string("test")));
  assert_false(data_equal_case(data_string("TeSa"), data_string("test")));
}

int main()
{
  const struct CMUnitTest tests[] =
      {
          cmocka_unit_test(test_data)
      };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
