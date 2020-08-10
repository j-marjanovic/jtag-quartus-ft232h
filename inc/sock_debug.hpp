
#pragma once

#include <stdarg.h>
#include <stdexcept>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

class SockDebug {
  int _sock;

public:
  explicit SockDebug(const char *path) {
    _sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    int rc = connect(_sock, (const struct sockaddr *)&addr, sizeof(addr));
    if (rc == -1) {
      // throw std::runtime_error("could not open socket");
      // TODO: check the effect on the program
    }

    debug_write("SockDebug is ready\n");
  }

  ~SockDebug() { close(_sock); }

  void debug_write(const char *format, ...) {
    char buf[4096];

    va_list arglist;
    va_start(arglist, format);
    vsprintf(buf, format, arglist);
    va_end(arglist);

    send(_sock, buf, strlen(buf), 0);
  }

  void mini_hexdump(void *ptr, unsigned int len) {
    debug_write("mini_hexdump(ptr = %p)\n", ptr);
    uint8_t *p = (uint8_t *)ptr;

    for (unsigned int i = 0; i < len; i++) {
      if ((i % 8 == 0) && (i != 0)) {
        debug_write("\n");
      }
      if (i % 8 == 0) {
        debug_write("%04x: ", i);
      }
      debug_write("%02x ", p[i]);
    }

    if ((len % 8) != 0) {
      debug_write("\n");
    }

    debug_write("\n");
  }
};