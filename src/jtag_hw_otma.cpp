

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "jtag_tap_controller.hpp"
#include "q_defs.h"
#include "sock_debug.hpp"

extern "C" {
#include "jtag/drivers/mpsse.h"
}

#define SOCK_DEBUG_PATH "/var/tmp/jtag-otma.sock"

//==============================================================================

SockDebug sock_debug{SOCK_DEBUG_PATH};

#define JTAG_MODE (LSB_FIRST | POS_EDGE_IN | NEG_EDGE_OUT)

struct mpsse_ctx *ctx = NULL;

//==============================================================================

#define DEBUG

#ifdef DEBUG
#define LIBDEBUG(...) sock_debug.debug_write("[lib] " __VA_ARGS__)
#define LIBDEBUG_HEXDUMP(addr, len) sock_debug.mini_hexdump(addr, len)
#else
#define LIBDEBUG(...)                                                          \
  do {                                                                         \
  } while (0)
#define LIBDEBUG_HEXDUMP(...)                                                  \
  do {                                                                         \
  } while (0)
#endif

//==============================================================================

struct server_ops_t server_ops;
void *server_ops_data;

#define DATA_SIZE_BYTES (32 * 1024 * 1024)

uint8_t *tdo_data = NULL;
uint8_t *tms_data = NULL;
uint8_t *tdi_data = NULL;
uint32_t tdo_data_count = 0;
uint32_t tdi_data_count = 0;

char find_devs(uint32_t dev_index, char *out_desc, uint32_t api_ver) {
  LIBDEBUG("find_devs(dev_index = %d, api_ver = %d)\n", dev_index, api_ver);

  if (api_ver < 4) {
    LIBDEBUG("  return 0\n");
    return 0;
  }

  if (dev_index >= 1) {
    LIBDEBUG("  return 0\n");
    return 0;
  }

  const uint16_t vid = 0x0403;
  const uint16_t pid = 0x6014;
  if (!ctx) {
    ctx = mpsse_open(&vid, &pid, NULL, NULL, NULL, 0);
  }
  sock_debug.debug_write("[MPSSE] mpsse_open = %p\n", ctx);

  strcpy(out_desc, "bus-instance");

  LIBDEBUG("  return 1\n");
  return 1;
}

char find_descriptions(const char *description) {
  LIBDEBUG("find_descriptions()\n");
  return !strcmp(description, "bus-instance");
}

int64_t init_dev(void **arg1, const char *desc,
                 struct server_ops_t *s_server_ops, void *parent) {
  LIBDEBUG("init_dev(desc = %s, s_server_ops = %p, parent = %p)\n", desc,
           s_server_ops, parent);

  if (!parent) {
    LIBDEBUG("  return 1\n");
    return 1;
  }

  if (strcmp(desc, "bus-instance") != 0) {
    LIBDEBUG("  return 1\n");
    return 1;
  }

  memcpy(&server_ops, s_server_ops, sizeof(struct server_ops_t));
  server_ops_data = parent;

  LIBDEBUG("  return 0\n");
  return 1;
}

void do_close(void *unused_void) { LIBDEBUG("do_close()\n"); }

int64_t do_flush(void *unused_void, int bool_val, uint32_t index_val) {
  LIBDEBUG("do_flush()\n");

  for (unsigned int i = 0; i < tdi_data_count; i++) {
    uint32_t byte_sel = i / 8;
    uint32_t bit_sel = i % 8;

    // uint8_t tms = (tms_data[byte_sel] >> bit_sel) & 1;
    uint8_t tdi = (tdi_data[byte_sel] >> bit_sel) & 1;

    int tms_bit = (tms_data[byte_sel] >> bit_sel) & 1;
    int tdo_bit;
    mpsse_clock_tms_cs(ctx, tms_data, i, tdo_data, i, 1, tdi, JTAG_MODE);
    tdo_data_count++;
  }

  int rc = mpsse_flush(ctx);
  sock_debug.debug_write("[MPSSE] mpsse_flush = %d\n", rc);

  sock_debug.debug_write("[MPSSE]  tdo_data_count = %d\n", tdo_data_count);
  // sock_debug.mini_hexdump(tdo_data, (tdo_data_count + 7) / 8);

  server_ops.p_op_store_tdo(server_ops_data, (uint32_t *)tdo_data,
                            tdo_data_count);

  tdi_data_count = 0;
  tdo_data_count = 0;
  memset(tdi_data, 0, DATA_SIZE_BYTES);
  memset(tms_data, 0, DATA_SIZE_BYTES);
  memset(tdo_data, 0, DATA_SIZE_BYTES);

  server_ops.p_op_indicate_flush(server_ops_data);

  return 1;
}

void clock_raw(void *unused_void, uint32_t jtag_tms, uint32_t jtag_tdi,
               unsigned long len) {

  LIBDEBUG("clock_raw(tms = %d, tdi = %d, len = %d)\n", jtag_tms, jtag_tdi,
           len);

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
  LIBDEBUG("clock_multiple(tmp = %d, bits = %p, len = %d, ? = %d)\n", jtag_tms,
           p_bits, len, field_144_minus_len);
  // LIBDEBUG_HEXDUMP(p_bits, (len + 7) / 8 + 1);

  uint8_t *in_ptr = (uint8_t *)(p_bits);
  for (unsigned int i = 0; i < len; i++) {
    uint32_t in_byte_sel = i / 8;
    uint32_t in_bit_sel = i % 8;
    uint32_t byte_sel = tdi_data_count / 8;
    uint32_t bit_sel = tdi_data_count % 8;

    uint8_t in_bit = (in_ptr[in_byte_sel] >> in_bit_sel) & 0x1;

    tdi_data[byte_sel] |= (in_bit << bit_sel);
    tms_data[byte_sel] |= (jtag_tms << bit_sel);

    tdi_data_count += 1;
  }
  return 0;
}

uint64_t set_param(void *arg1, char *arg2, uint32_t arg3) {
  LIBDEBUG("set_param(param = %s)\n", arg2);
  return 0;
}

int64_t get_param(void *arg1, char *arg2, uint32_t *arg3) {
  LIBDEBUG("get_param(param = %s)\n", arg2);
  return 0;
}

int64_t direct_control(void *arg1, int32_t arg2, int32_t *arg3) {
  LIBDEBUG("direct_control(arg2 = %d)\n", arg2);
  return 0;
}

//==============================================================================

struct virtual_fns_st fns = {sizeof(struct virtual_fns_st),
                             "OTMA FT232H",
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
  LIBDEBUG("get_supported_hardware(hw_type = 0x%x)\n", hw_type);

  if (hw_type != 0) {
    return 0;
  }

  if (!tdo_data) {
    tdo_data = (uint8_t *)malloc(DATA_SIZE_BYTES);
  }
  if (!tms_data) {
    tms_data = (uint8_t *)malloc(DATA_SIZE_BYTES);
  }
  if (!tdi_data) {
    tdi_data = (uint8_t *)malloc(DATA_SIZE_BYTES);
  }

  LIBDEBUG("  tdo_data = %p\n", tdo_data);
  LIBDEBUG("  tms_data = %p\n", tms_data);
  LIBDEBUG("  tdi_data = %p\n", tdi_data);

  return &fns;
}
}
