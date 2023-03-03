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
  char request[1024];
  data method, target, body;
  http_field fields[16];
  size_t fields_count;
  int n;

  fields_count = 16;
  n = http_read_request(
    data_string("GET / HTTP/1.0\r\nHost: test\r\n\r\n"),
    &method, &target, fields, &fields_count, &body);
  assert_true(n > 0);
  assert_int_equal(fields_count, 1);

  /* partial post header */
  fields_count = 16;
  n = http_read_request(
    data_string("POST / HTTP/1.0\r\n"),
    &method, &target, fields, &fields_count, &body);
  assert_true(n == -2);

  /* length encoded */
  strcpy(request,
         "POST / HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Content-Length: 4\r\n"
         "\r\n"
         "test");
  fields_count = 16;
  n = http_read_request(data_string(request), &method, &target, fields, &fields_count, &body);
  assert_true(n > 0);
  assert_int_equal(fields_count, 2);
  assert_true(data_equal(body, data_string("test")));

  /* partial length encoded */
  strcpy(request,
         "POST / HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Content-Length: 4\r\n"
         "\r\n");
  fields_count = 16;
  n = http_read_request(data_string(request), &method, &target, fields, &fields_count, &body);
  assert_true(n == -2);

  /* no encoding for length */
  strcpy(request,
         "POST / HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "\r\n");
  fields_count = 16;
  n = http_read_request(data_string(request), &method, &target, fields, &fields_count, &body);
  assert_true(n == -1);

  /* both length and chunked encoded is illegal */
  strcpy(request,
         "POST / HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Transfer-Encoding: chunked\r\n"
         "Content-Length: 4\r\n"
         "\r\n"
         "test");
  fields_count = 16;
  n = http_read_request(data_string(request), &method, &target, fields, &fields_count, &body);
  assert_true(n == -1);

  /* chunked */
  strcpy(request,
         "POST / HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "4\r\n"
         "test\r\n"
         "0\r\n"
         "\r\n");
  fields_count = 16;
  n = http_read_request(data_string(request), &method, &target, fields, &fields_count, &body);
  assert_true(n > 0);
  assert_int_equal(fields_count, 2);
  assert_true(data_equal(body, data_string("test")));

  /* illegal transfer encoding */
  strcpy(request,
         "POST / HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Transfer-Encoding: weird\r\n"
         "\r\n"
         "4\r\n"
         "test\r\n"
         "0\r\n"
         "\r\n");
  fields_count = 16;
  n = http_read_request(data_string(request), &method, &target, fields, &fields_count, &body);
  assert_true(n == -1);

  /* partial chunked */
  strcpy(request,
         "POST / HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "4");
  fields_count = 16;
  n = http_read_request(data_string(request), &method, &target, fields, &fields_count, &body);
  assert_true(n == -2);

  /* input to small to read chunk */
  strcpy(request,
         "POST / HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "4\r\n");
  fields_count = 16;
  n = http_read_request(data_string(request), &method, &target, fields, &fields_count, &body);
  assert_true(n == -2);
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
