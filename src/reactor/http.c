#include <string.h>

#include "picohttpparser/picohttpparser.h"

#include "../reactor.h"

static size_t utility_u32_len(uint32_t n)
{
  static const uint32_t pow_10[] = {0, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
  uint32_t t;

  t = (32 - __builtin_clz(n | 1)) * 1233 >> 12;
  return t - (n < pow_10[t]) + 1;
}

static void utility_u32_sprint(uint32_t n, char *string)
{
  static const char digits[] =
      "0001020304050607080910111213141516171819"
      "2021222324252627282930313233343536373839"
      "4041424344454647484950515253545556575859"
      "6061626364656667686970717273747576777879"
      "8081828384858687888990919293949596979899";
  size_t i;

  while (n >= 100)
  {
    i = (n % 100) << 1;
    n /= 100;
    *--string = digits[i + 1];
    *--string = digits[i];
  }

  if (n < 10)
  {
    *--string = n + '0';
  }
  else
  {
    i = n << 1;
    *--string = digits[i + 1];
    *--string = digits[i];
  }
}

static void utility_move(void **pointer, size_t size)
{
  *(uint8_t **) pointer += size;
}
static void utility_push_data(void **pointer, data data)
{
  memcpy(*pointer, data_base(data), data_size(data));
  utility_move(pointer, data_size(data));
}

static void utility_push_byte(void **pointer, uint8_t byte)
{
  **((uint8_t **) pointer) = byte;
  utility_move(pointer, 1);
}

http_field http_field_define(data name, data value)
{
  return (http_field) {.name = name, .value = value};
}

void http_push_field(void **pointer, http_field field)
{
  utility_push_data(pointer, field.name);
  utility_push_byte(pointer, ':');
  utility_push_byte(pointer, ' ');
  utility_push_data(pointer, field.value);
  utility_push_byte(pointer, '\r');
  utility_push_byte(pointer, '\n');
}

int http_read_request(data buffer, data *method, data *target, http_field *fields, size_t *fields_count)
{
  int n, minor_version;

  n = phr_parse_request(data_base(buffer), data_size(buffer),
                        (const char **) &method->base, &method->size,
                        (const char **) &target->base, &target->size,
                        &minor_version,
                        (struct phr_header *) fields, fields_count, 0);
  asm volatile("": : :"memory");
  return n;
}

void http_write_response(buffer *buffer, data status, data date, data type, data body)
{
  char storage[16];
  size_t size;
  data length;
  void *p;

  size = utility_u32_len(data_size(body));
  utility_u32_sprint(data_size(body), storage + size);
  length = data_define(storage, size);

  p = buffer_allocate(buffer, 9 + data_size(status) + 2 + 17 + 37 +
                      (data_empty(type) ? 0 : 16 + data_size(type)) +
                      (18 + data_size(length)) + 2 + data_size(body));

  utility_push_data(&p, data_string("HTTP/1.1 "));
  utility_push_data(&p, status);
  utility_push_data(&p, data_string("\r\n"));
  http_push_field(&p, http_field_define(data_string("Server"), data_string("reactor")));
  date.size = 29;
  http_push_field(&p, http_field_define(data_string("Date"), date));
  if (!data_empty(type))
    http_push_field(&p, http_field_define(data_string("Content-Type"), type));
  http_push_field(&p, http_field_define(data_string("Content-Length"), length));
  utility_push_data(&p, data_string("\r\n"));
  utility_push_data(&p, body);
}
