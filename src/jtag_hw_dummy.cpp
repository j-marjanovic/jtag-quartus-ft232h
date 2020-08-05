

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "jtag_tap_controller.hpp"
#include "q_defs.h"
#include "sock_debug.h"

#define SOCK_PATH "/var/tmp/jtag-dummy.sock"

//==============================================================================
// debug

debug_data_t _debug_data;

static void __attribute__((constructor)) ctor() {
  debug_init(&_debug_data, SOCK_PATH);
  debug_write(&_debug_data, "JTAG Dummy: debug socket init\n");
}

//==============================================================================

struct jtag_tap_state _jtag_tap_state = {TAP_STATE_TEST_LOGIC_RESET};

//==============================================================================

struct server_ops_t server_ops;
void *server_ops_data;
uint8_t tdo_data[8192] = {0};
uint8_t tms_data[8192] = {0};
uint8_t tdi_data[8192] = {0};
uint32_t tdo_data_count = 0;
uint32_t tdi_data_count = 0;

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

  memcpy(&server_ops, s_server_ops, sizeof(struct server_ops_t));
  server_ops_data = parent;

  debug_write(&_debug_data, "  return 0\n");
  return 1;
}

void do_close(void *unused_void) { debug_write(&_debug_data, "do_close()\n"); }

int64_t do_flush(void *unused_void, int bool_val, uint32_t index_val) {
  debug_write(&_debug_data, "do_flush()\n");

  for (unsigned int i = 0; i < tdi_data_count; i++) {
    uint32_t byte_sel = i / 8;
    uint32_t bit_sel = i % 8;

    int tms = (tms_data[byte_sel] >> bit_sel) & 1;
    int tdi = (tdi_data[byte_sel] >> bit_sel) & 1;
    int tdo;

    jtag_tap_controller(&_jtag_tap_state, tms, tdi, &tdo);
    tdo_data[byte_sel] |= (tdo << bit_sel);
    tdo_data_count++;
  }

  server_ops.p_op_store_tdo(server_ops_data, (uint32_t *)tdo_data,
                            tdo_data_count);

  tdi_data_count = 0;
  tdo_data_count = 0;
  memset(tdi_data, 0, sizeof(tdo_data));
  memset(tms_data, 0, sizeof(tms_data));
  memset(tdo_data, 0, sizeof(tdo_data));

  server_ops.p_op_indicate_flush(server_ops_data);

  return 1;
}

void clock_raw(void *unused_void, uint32_t jtag_tms, uint32_t jtag_tdi,
               unsigned long len) {
  debug_write(&_debug_data, "clock_raw(tms = %d, tdi = %d, len = %d)\n",
              jtag_tms, jtag_tdi, len);

  for (unsigned int i = 0; i < len; i++) {
    uint32_t byte_sel = tdi_data_count / 8;
    uint32_t bit_sel = tdi_data_count % 8;

    tdi_data[byte_sel] |= (jtag_tdi << bit_sel);
    tms_data[byte_sel] |= (jtag_tms << bit_sel);

    tdi_data_count += 1;
  }
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
    sizeof(struct virtual_fns_st),
    "Dummy JTAG device",
    0x800,
    nullptr,
    nullptr,
    find_devs,
    find_descriptions,
    init_dev,
    do_close,
    set_param,
    get_param,
    direct_control,
    clock_raw,
    clock_multiple,
    nullptr,
    do_flush,
};

extern "C" { __attribute__((visibility("default"))) extern struct virtual_fns_st *
get_supported_hardware(uint32_t hw_type) {
  debug_write(&_debug_data, "get_supported_hardware(hw_type = 0x%x)\n",
              hw_type);

  if (hw_type != 0) {
    return 0;
  }

  return &fns;
}
}
