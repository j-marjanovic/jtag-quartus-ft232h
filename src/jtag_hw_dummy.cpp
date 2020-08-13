

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "jtag_tap_controller.hpp"
#include "q_defs.h"
#include "sock_debug.hpp"

#define SOCK_DEBUG_PATH "/var/tmp/jtag-dummy.sock"

//==============================================================================

SockDebug sock_debug{SOCK_DEBUG_PATH};

JtagTapController jtag_controller{};

//==============================================================================

struct server_ops_t server_ops;
void *server_ops_data;
uint8_t tdo_data[8192] = {0};
uint8_t tms_data[8192] = {0};
uint8_t tdi_data[8192] = {0};
uint32_t tdo_data_count = 0;
uint32_t tdi_data_count = 0;

char find_devs(uint32_t dev_index, char *out_desc, uint32_t api_ver) {
  sock_debug.debug_write("find_devs(dev_index = %d, api_ver = %d)\n", dev_index,
                         api_ver);

  if (api_ver < 4) {
    sock_debug.debug_write("  return 0\n");
    return 0;
  }

  if (dev_index >= 1) {
    sock_debug.debug_write("  return 0\n");
    return 0;
  }

  strcpy(out_desc, "bus-instance");

  sock_debug.debug_write("  return 1\n");
  return 1;
}

char find_descriptions(const char *description) {
  sock_debug.debug_write("find_descriptions()\n");
  return !strcmp(description, "bus-instance");
}

int64_t init_dev(void **arg1, const char *desc,
                 struct server_ops_t *s_server_ops, void *parent) {
  sock_debug.debug_write(
      "init_dev(desc = %s, s_server_ops = %p, parent = %p)\n", desc,
      s_server_ops, parent);

  if (!parent) {
    sock_debug.debug_write("  return 1\n");
    return 1;
  }

  if (strcmp(desc, "bus-instance") != 0) {
    sock_debug.debug_write("  return 1\n");
    return 1;
  }

  memcpy(&server_ops, s_server_ops, sizeof(struct server_ops_t));
  server_ops_data = parent;

  sock_debug.debug_write("  return 0\n");
  return 1;
}

void do_close(void *unused_void) { sock_debug.debug_write("do_close()\n"); }

int64_t do_flush(void *unused_void, int bool_val, uint32_t index_val) {
  sock_debug.debug_write("do_flush()\n");

  for (unsigned int i = 0; i < tdi_data_count; i++) {
    uint32_t byte_sel = i / 8;
    uint32_t bit_sel = i % 8;

    int tms = (tms_data[byte_sel] >> bit_sel) & 1;
    int tdi = (tdi_data[byte_sel] >> bit_sel) & 1;
    int tdo;

    jtag_controller.step(tms, tdi, &tdo);
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

  sock_debug.debug_write("clock_raw(tms = %d, tdi = %d, len = %d)\n", jtag_tms,
                         jtag_tdi, len);

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
  sock_debug.debug_write(
      "clock_multiple(tmp = %d, bits = %p, len = %d, ? = %d)\n", jtag_tms,
      p_bits, len, field_144_minus_len);
  sock_debug.mini_hexdump(p_bits, (len + 7) / 8 + 1);

  uint8_t *in_ptr = (uint8_t *)(p_bits);
  for (unsigned int i = 0; i < len; i++) {
    uint32_t byte_sel = tdi_data_count / 8;
    uint32_t bit_sel = tdi_data_count % 8;

    uint8_t in_bit = (in_ptr[byte_sel] >> bit_sel) & 0x1;

    tdi_data[byte_sel] |= (in_bit << bit_sel);
    tms_data[byte_sel] |= (jtag_tms << bit_sel);

    tdi_data_count += 1;
  }
  return 0;
}

uint64_t set_param(void *arg1, char *arg2, uint32_t arg3) {
  sock_debug.debug_write("set_param(param = %s)\n", arg2);
  return 0;
}

int64_t get_param(void *arg1, char *arg2, uint32_t *arg3) {
  sock_debug.debug_write("get_param(param = %s)\n", arg2);
  return 0;
}

int64_t direct_control(void *arg1, int32_t arg2, int32_t *arg3) {
  sock_debug.debug_write("direct_control(arg2 = %d)\n", arg2);
  return 0;
}

//==============================================================================

struct virtual_fns_st fns = {sizeof(struct virtual_fns_st),
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
                             nullptr};

extern "C" {
__attribute__((visibility("default"))) struct virtual_fns_st *
get_supported_hardware(uint32_t hw_type) {
  sock_debug.debug_write("get_supported_hardware(hw_type = 0x%x)\n", hw_type);

  if (hw_type != 0) {
    return 0;
  }

  return &fns;
}
}
