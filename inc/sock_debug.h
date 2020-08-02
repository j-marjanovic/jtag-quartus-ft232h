
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

typedef struct {
  int _sock;
} debug_data_t;

void debug_init(debug_data_t *data, const char *path) {
  data->_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

  int rc = connect(data->_sock, (const struct sockaddr *)&addr, sizeof(addr));
  if (rc == -1) {
    // abort();
  }
}

void debug_write(debug_data_t *data, const char *format, ...) {
  char buf[4096];

  va_list arglist;
  va_start(arglist, format);
  vsprintf(buf, format, arglist);
  va_end(arglist);

  send(data->_sock, buf, strlen(buf), 0);
}

extern debug_data_t _debug_data;

void mini_hexdump(void *ptr, unsigned int len) {
  debug_write(&_debug_data, "mini_hexdump(ptr = %p)\n", ptr);
  uint8_t *p = (uint8_t *)ptr;

  for (unsigned int i = 0; i < len; i++) {
    if ((i % 8 == 0) && (i != 0)) {
      debug_write(&_debug_data, "\n");
    }
    if (i % 8 == 0) {
      debug_write(&_debug_data, "%04x: ", i);
    }
    debug_write(&_debug_data, "%02x ", p[i]);
  }

  if ((len % 8) != 0) {
    debug_write(&_debug_data, "\n");
  }

  debug_write(&_debug_data, "\n");
}
