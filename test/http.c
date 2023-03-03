#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>
#include <errno.h>
#include <limits.h>
#include <cmocka.h>
#include <string.h>

#include "reactor.h"

static void test_request(__attribute__((unused)) void **arg)
{
  data method, target;
  http_field fields[16];
  size_t fields_count = 16;
  int n;

  n = http_read_request(
    data_string("GET / HTTP/1.0\r\n\r\n"),
    &method, &target, fields, &fields_count);
  assert_true(n > 0);

}

static void test_response(__attribute__((unused)) void **arg)
{
  buffer buffer;
  char large[4096] = {0};

  buffer_construct(&buffer);
  http_write_response(&buffer, data_string("200 OK"), data_string("Fri, 03 Mar 2023 10:28:49 GMT"), data_string("text/html"), data_string("test"));
  assert_true(strncmp(
                "HTTP/1.1 200 OK\r\n"
                "Server: reactor\r\n"
                "Date: Fri, 03 Mar 2023 10:28:49 GMT\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: 4\r\n"
                "\r\n"
                "test",
                buffer_base(&buffer), buffer_size(&buffer)) == 0);

  /* empty body */
  buffer_clear(&buffer);
  http_write_response(&buffer, data_string("200 OK"), data_string("Fri, 03 Mar 2023 10:28:49 GMT"), data_null(), data_null());
  assert_true(strncmp(
                "HTTP/1.1 200 OK\r\n"
                "Server: reactor\r\n"
                "Date: Fri, 03 Mar 2023 10:28:49 GMT\r\n"
                "Content-Length: 0\r\n"
                "\r\n",
                buffer_base(&buffer), buffer_size(&buffer)) == 0);

  /* larger body */
  buffer_clear(&buffer);
  http_write_response(&buffer, data_string("200 OK"), data_string("Fri, 03 Mar 2023 10:28:49 GMT"), data_string("text/html"), data_define(large, sizeof large));
  assert_int_equal(buffer_size(&buffer), 4216);
}

int main()
{
  const struct CMUnitTest tests[] =
    {
      cmocka_unit_test(test_request),
      cmocka_unit_test(test_response)
    };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
