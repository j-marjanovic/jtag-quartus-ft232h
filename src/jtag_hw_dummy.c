

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "q_defs.h"
#include "sock_debug.h"

/**
 * nc -lkuU /var/tmp/jtag-dummy.sock
 *
 */

#define SOCK_PATH "/var/tmp/jtag-dummy.sock"

//==============================================================================
// debug

debug_data_t _debug_data;

static void __attribute__((constructor)) ctor() {
  debug_init(&_debug_data, SOCK_PATH);
  debug_write(&_debug_data, "JTAG Dummy: debug socket init\n");
}

//==============================================================================

char find_devs(uint32_t dev_index, char *out_desc, uint32_t api_ver) {
  debug_write(&_debug_data, "find_devs(dev_index = %d, api_ver = %d)\n",
              dev_index, api_ver);

  if (api_ver < 4) {
    debug_write(&_debug_data, "  return 0\n");
    return 0;
  }

  if (dev_index >= 1) {
    debug_write(&_debug_data, "  return 0\n");
    return 0;
  }

  strcpy(out_desc, "bus-instance");

  debug_write(&_debug_data, "  return 1\n");
  return 1;
}

char find_descriptions(const char *description) {
  debug_write(&_debug_data, "find_descriptions()\n");
  return !strcmp(description, "bus-instance");
}

int64_t init_dev(void **arg1, const char *desc,
                 struct server_ops_t *s_server_ops, void *parent) {
  debug_write(&_debug_data,
              "init_dev(desc = %s, s_server_ops = %p, parent = %p)\n", desc,
              s_server_ops, parent);

  if (!parent) {
    debug_write(&_debug_data, "  return 1\n");
    return 1;
  }

  if (strcmp(desc, "bus-instance") != 0) {
    debug_write(&_debug_data, "  return 1\n");
    return 1;
  }

  debug_write(&_debug_data, "  return 0\n");
  return 1;
}

void do_close(void *unused_void) { debug_write(&_debug_data, "do_close()\n"); }

int64_t do_flush(void *unused_void, int bool_val, uint32_t index_val) {
  debug_write(&_debug_data, "do_flush()\n");
  return 1;
}

void clock_raw(void *unused_void, uint32_t jtag_tms, uint32_t v,
               unsigned long len) {
  debug_write(&_debug_data, "clock_raw(tms = %d, v = %d, len = %d)\n", jtag_tms,
              v, len);
}

int64_t clock_multiple(void *unused_void, unsigned jtag_tms,
                       unsigned long *p_bits, unsigned long len,
                       unsigned long field_144_minus_len) {
  debug_write(&_debug_data, "clock_multiple()\n");
  debug_write(&_debug_data, "!!! not yet implemented !!!\n");
  return 0;
}

uint64_t set_param(void *arg1, char *arg2, uint32_t arg3) {
  debug_write(&_debug_data, "set_param(param = %s)\n", arg2);
  return 0;
}

int64_t get_param(void *arg1, char *arg2, uint32_t *arg3) {
  debug_write(&_debug_data, "get_param(param = %s)\n", arg2);
  return 0;
}

int64_t direct_control(void *arg1, int32_t arg2, int32_t *arg3) {
  debug_write(&_debug_data, "direct_control(arg2 = %d)\n", arg2);
  return 0;
}

//==============================================================================

struct virtual_fns_st fns = {
    .st_size = sizeof(struct virtual_fns_st),
    .dev_description = "Dummy JTAG device",
    .st_flags = 0x800,
    .p_find_devs = find_devs,
    .p_find_descriptions = find_descriptions,
    .p_init_dev = init_dev,
    .p_close = do_close,
    .p_set_param = set_param,
    .p_get_param = get_param,
    .p_direct_control = direct_control,
    .p_clock_raw = clock_raw,
    .p_clock_multiple = clock_multiple,
    .p_do_flush = do_flush,
};

__attribute__((visibility("default"))) extern struct virtual_fns_st *
get_supported_hardware(uint32_t hw_type) {
  debug_write(&_debug_data, "get_supported_hardware(hw_type = 0x%x)\n",
              hw_type);

  if (hw_type != 0) {
    return 0;
  }

  return &fns;
}
