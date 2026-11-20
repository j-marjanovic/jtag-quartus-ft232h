
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <mutex>

#include <libusb.h>

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
//
// MPSSE initialisation
//
// Quartus' jtagd only opens the MPSSE and starts clocking, so the pin
// directions and the TCK divider have to be programmed into the FT232H
// beforehand.

#define OTMA_PIN_OUTPUT_INIT 0xfff8u
#define OTMA_PIN_DIR_INIT 0xfffbu

#define OTMA_VID 0x0403
#define OTMA_PID 0x6014

// TCK rate.
//
// Quartus' Programmer configures the cable through the set_param()/get_param()
// callbacks below. The TCK rate is the "JtagClock" parameter (the "Clock
// frequency" box in the Programmer's Hardware Settings dialog; Quartus stores
// it under the PGMWQ_TCK_FREQ key) and arrives here in Hz. The FT232H only
// needs a divider programmed, so honouring the parameter is a couple of MPSSE
// commands. Until the Programmer asks for something else we use 2 MHz as a safe
// default.

#define OTMA_DEFAULT_TCK_HZ 2000000u
#define OTMA_PARAM_TCK "JtagClock"
#define OTMA_PARAM_KERNEL_UPDATE "KernelUpdateRequired"

static uint32_t g_tck_hz = OTMA_DEFAULT_TCK_HZ;

// Program the MPSSE clock divider. Returns the rate the FT232H will actually
// run at, or a negative value on failure.
static int otma_apply_tck(struct mpsse_ctx *c, uint32_t hz) {
  if (!c) {
    return -1;
  }
  if (hz == 0) {
    hz = OTMA_DEFAULT_TCK_HZ;
  }

  int actual = mpsse_set_frequency(c, (int)hz);
  int rc = mpsse_flush(c);

  sock_debug.debug_write(
      "[MPSSE] TCK: requested %u Hz, actual %d Hz, flush = %d\n", hz, actual,
      rc);

  return (rc == 0) ? actual : -1;
}

static int otma_init_mpsse(struct mpsse_ctx *c) {
  if (!c) {
    return -1;
  }

  mpsse_set_data_bits_low_byte(c, OTMA_PIN_OUTPUT_INIT & 0xff,
                               OTMA_PIN_DIR_INIT & 0xff);
  mpsse_set_data_bits_high_byte(c, (OTMA_PIN_OUTPUT_INIT >> 8) & 0xff,
                                (OTMA_PIN_DIR_INIT >> 8) & 0xff);
  mpsse_loopback_config(c, false);

  return otma_apply_tck(c, g_tck_hz);
}

//==============================================================================
//
// Adapter discovery
//
// Every FT232H on the system is a separate JTAG cable. The chips have no
// serial number (iSerialNumber is 0 in the on-board EEPROM), so the only
// stable identity is the USB bus/port path, e.g. "1:4" for bus 1 port 4. That
// is also the string mpsse_open() accepts as its `location` argument.

#define OTMA_MAX_DEVS 8
#define DATA_SIZE_BYTES (32 * 1024 * 1024)

struct otma_dev {
  char desc[32];     // cable description shown by Quartus, e.g. "otma-1:4"
  char location[24]; // USB bus:port path for mpsse_open(), e.g. "1:4"
  struct mpsse_ctx *ctx;
  struct server_ops_t server_ops;
  void *server_ops_data;
  uint8_t *tdo_data;
  uint8_t *tms_data;
  uint8_t *tdi_data;
  uint32_t tdo_data_count;
  uint32_t tdi_data_count;
  uint32_t tck_hz; // last rate programmed into this adapter
  bool opened;
};

static otma_dev g_devs[OTMA_MAX_DEVS];
static int g_dev_count = -1; // -1 until enumerated
static otma_dev *g_last_dev = NULL;
static std::mutex g_enum_mutex;

static bool otma_build_location(libusb_device *device, char *out,
                                size_t out_len) {
  uint8_t path[8];
  int bus = libusb_get_bus_number(device);
  int path_len = libusb_get_port_numbers(device, path, sizeof(path));

  if (path_len <= 0 || path_len == LIBUSB_ERROR_OVERFLOW) {
    return false;
  }

  int off = snprintf(out, out_len, "%d:", bus);
  for (int i = 0; i < path_len; i++) {
    if (off < 0 || (size_t)off >= out_len) {
      return false;
    }
    off += snprintf(out + off, out_len - off, i ? ",%d" : "%d", path[i]);
  }
  return true;
}

// Enumerate the FTDI adapters. Must be called with g_enum_mutex held.
static void otma_enumerate_locked(void) {
  if (g_dev_count >= 0) {
    return;
  }

  g_dev_count = 0;

  libusb_context *usb_ctx = NULL;
  if (libusb_init(&usb_ctx) != LIBUSB_SUCCESS) {
    sock_debug.debug_write("[dev] libusb_init failed\n");
    return;
  }

  libusb_device **list = NULL;
  ssize_t cnt = libusb_get_device_list(usb_ctx, &list);
  if (cnt < 0) {
    sock_debug.debug_write("[dev] libusb_get_device_list failed\n");
    libusb_exit(usb_ctx);
    return;
  }

  for (ssize_t i = 0; i < cnt && g_dev_count < OTMA_MAX_DEVS; i++) {
    struct libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(list[i], &desc) != LIBUSB_SUCCESS) {
      continue;
    }
    if (desc.idVendor != OTMA_VID || desc.idProduct != OTMA_PID) {
      continue;
    }

    otma_dev &dev = g_devs[g_dev_count];
    memset(&dev, 0, sizeof(dev));
    if (!otma_build_location(list[i], dev.location, sizeof(dev.location))) {
      continue;
    }
    snprintf(dev.desc, sizeof(dev.desc), "otma-%s", dev.location);
    g_dev_count++;
  }

  libusb_free_device_list(list, 1);
  libusb_exit(usb_ctx);

  std::sort(g_devs, g_devs + g_dev_count,
            [](const otma_dev &a, const otma_dev &b) {
              return strcmp(a.location, b.location) < 0;
            });

  sock_debug.debug_write("[dev] found %d adapter(s)\n", g_dev_count);
  for (int i = 0; i < g_dev_count; i++) {
    sock_debug.debug_write("[dev]   %d: %s\n", i, g_devs[i].desc);
  }
}

static int otma_enumerate(void) {
  std::lock_guard<std::mutex> lock(g_enum_mutex);
  otma_enumerate_locked();
  return g_dev_count;
}

static otma_dev *otma_find_by_desc(const char *desc) {
  int count = otma_enumerate();
  for (int i = 0; i < count; i++) {
    if (!strcmp(g_devs[i].desc, desc)) {
      return &g_devs[i];
    }
  }
  return NULL;
}

// The Quartus ABI passes the pointer that init_dev() stored through *arg1 back
// to every transfer callback. Fall back to the most recently opened adapter if
// it passes something else.
static otma_dev *otma_dev_from(void *handle) {
  if (handle) {
    for (int i = 0; i < g_dev_count; i++) {
      if (handle == (void *)&g_devs[i]) {
        return &g_devs[i];
      }
    }
  }
  return g_last_dev;
}

//==============================================================================

char find_devs(uint32_t dev_index, char *out_desc, uint32_t api_ver) {
  LIBDEBUG("find_devs(dev_index = %d, api_ver = %d)\n", dev_index, api_ver);

  if (api_ver < 4) {
    LIBDEBUG("  return 0\n");
    return 0;
  }

  int count = otma_enumerate();
  if (dev_index >= (uint32_t)count) {
    LIBDEBUG("  return 0\n");
    return 0;
  }

  // out_desc is a 32 byte field in struct virtual_fns_st
  strncpy(out_desc, g_devs[dev_index].desc, 31);
  out_desc[31] = '\0';

  LIBDEBUG("  desc = %s\n", out_desc);
  LIBDEBUG("  return 1\n");
  return 1;
}

char find_descriptions(const char *description) {
  LIBDEBUG("find_descriptions(%s)\n", description);
  return otma_find_by_desc(description) != NULL;
}

int64_t init_dev(void **arg1, const char *desc,
                 struct server_ops_t *s_server_ops, void *parent) {
  LIBDEBUG("init_dev(desc = %s, s_server_ops = %p, parent = %p)\n", desc,
           s_server_ops, parent);

  if (!parent) {
    LIBDEBUG("  return 1\n");
    return 1;
  }

  otma_dev *dev = otma_find_by_desc(desc);
  if (!dev) {
    sock_debug.debug_write("[dev] unknown adapter '%s'\n", desc);
    return 1;
  }

  if (!dev->ctx) {
    const uint16_t vid = OTMA_VID;
    const uint16_t pid = OTMA_PID;
    dev->ctx = mpsse_open(&vid, &pid, NULL, NULL, dev->location, 0);
    sock_debug.debug_write("[MPSSE] %s: mpsse_open = %p\n", dev->desc,
                           dev->ctx);
    if (!dev->ctx) {
      return 1;
    }
    int tck = otma_init_mpsse(dev->ctx);
    dev->tck_hz = (tck > 0) ? (uint32_t)tck : g_tck_hz;
  }

  if (!dev->tdo_data) {
    dev->tdo_data = (uint8_t *)malloc(DATA_SIZE_BYTES);
    dev->tms_data = (uint8_t *)malloc(DATA_SIZE_BYTES);
    dev->tdi_data = (uint8_t *)malloc(DATA_SIZE_BYTES);
    if (!dev->tdo_data || !dev->tms_data || !dev->tdi_data) {
      sock_debug.debug_write("[dev] %s: buffer allocation failed\n",
                             dev->desc);
      return 1;
    }
  }

  memcpy(&dev->server_ops, s_server_ops, sizeof(struct server_ops_t));
  dev->server_ops_data = parent;
  dev->opened = true;
  g_last_dev = dev;

  if (arg1) {
    *arg1 = dev;
  }

  // A fresh session starts with an empty command queue.
  dev->tdi_data_count = 0;
  dev->tdo_data_count = 0;
  memset(dev->tdi_data, 0, DATA_SIZE_BYTES);
  memset(dev->tms_data, 0, DATA_SIZE_BYTES);
  memset(dev->tdo_data, 0, DATA_SIZE_BYTES);

  LIBDEBUG("  return 0\n");
  return 1;
}

void do_close(void *arg) {
  otma_dev *dev = otma_dev_from(arg);
  LIBDEBUG("do_close(%s)\n", dev ? dev->desc : "?");
}

int64_t do_flush(void *arg, int bool_val, uint32_t index_val) {
  otma_dev *dev = otma_dev_from(arg);
  if (!dev || !dev->ctx) {
    return 0;
  }

  LIBDEBUG("do_flush(%s)\n", dev->desc);

  for (unsigned int i = 0; i < dev->tdi_data_count; i++) {
    uint32_t byte_sel = i / 8;
    uint32_t bit_sel = i % 8;

    uint8_t tdi = (dev->tdi_data[byte_sel] >> bit_sel) & 1;

    mpsse_clock_tms_cs(dev->ctx, dev->tms_data, i, dev->tdo_data, i, 1, tdi,
                       JTAG_MODE);
    dev->tdo_data_count++;
  }

  int rc = mpsse_flush(dev->ctx);
  sock_debug.debug_write("[MPSSE] %s: mpsse_flush = %d\n", dev->desc, rc);

  LIBDEBUG("  tdo_data_count = %d\n", dev->tdo_data_count);

  dev->server_ops.p_op_store_tdo(dev->server_ops_data,
                                 (uint32_t *)dev->tdo_data, dev->tdo_data_count);

  dev->tdi_data_count = 0;
  dev->tdo_data_count = 0;
  memset(dev->tdi_data, 0, DATA_SIZE_BYTES);
  memset(dev->tms_data, 0, DATA_SIZE_BYTES);
  memset(dev->tdo_data, 0, DATA_SIZE_BYTES);

  dev->server_ops.p_op_indicate_flush(dev->server_ops_data);

  return 1;
}

void clock_raw(void *arg, uint32_t jtag_tms, uint32_t jtag_tdi,
               unsigned long len) {
  otma_dev *dev = otma_dev_from(arg);
  if (!dev) {
    return;
  }

  LIBDEBUG("clock_raw(%s, tms = %d, tdi = %d, len = %d)\n", dev->desc, jtag_tms,
           jtag_tdi, len);

  for (unsigned int i = 0; i < len; i++) {
    uint32_t byte_sel = dev->tdi_data_count / 8;
    uint32_t bit_sel = dev->tdi_data_count % 8;

    dev->tdi_data[byte_sel] |= (jtag_tdi << bit_sel);
    dev->tms_data[byte_sel] |= (jtag_tms << bit_sel);

    dev->tdi_data_count += 1;
  }
}

int64_t clock_multiple(void *arg, unsigned jtag_tms, unsigned long *p_bits,
                       unsigned long len,
                       unsigned long field_144_minus_len) {
  otma_dev *dev = otma_dev_from(arg);
  if (!dev) {
    return 0;
  }

  LIBDEBUG("clock_multiple(%s, tms = %d, bits = %p, len = %d, ? = %d)\n",
           dev->desc, jtag_tms, p_bits, len, field_144_minus_len);

  uint8_t *in_ptr = (uint8_t *)(p_bits);
  for (unsigned int i = 0; i < len; i++) {
    uint32_t in_byte_sel = i / 8;
    uint32_t in_bit_sel = i % 8;
    uint32_t byte_sel = dev->tdi_data_count / 8;
    uint32_t bit_sel = dev->tdi_data_count % 8;

    uint8_t in_bit = (in_ptr[in_byte_sel] >> in_bit_sel) & 0x1;

    dev->tdi_data[byte_sel] |= (in_bit << bit_sel);
    dev->tms_data[byte_sel] |= (jtag_tms << bit_sel);

    dev->tdi_data_count += 1;
  }
  return 0;
}

// Quartus' Programmer asks for the TCK rate before it starts and sets it when
// the user changes the hardware settings.
uint64_t set_param(void *arg, char *name, uint32_t value) {
  otma_dev *dev = otma_dev_from(arg);

  LIBDEBUG("set_param(%s, %s = %u)\n", dev ? dev->desc : "?",
           name ? name : "?", value);

  if (!name) {
    return 0;
  }

  if (!strcmp(name, OTMA_PARAM_TCK)) {
    g_tck_hz = value ? value : OTMA_DEFAULT_TCK_HZ;
    if (dev && dev->ctx) {
      int actual = otma_apply_tck(dev->ctx, g_tck_hz);
      dev->tck_hz = (actual > 0) ? (uint32_t)actual : g_tck_hz;
    }
  }

  return 0;
}

int64_t get_param(void *arg, char *name, uint32_t *out) {
  otma_dev *dev = otma_dev_from(arg);

  LIBDEBUG("get_param(%s, %s)\n", dev ? dev->desc : "?",
           name ? name : "?");

  if (!name || !out) {
    return 0;
  }

  if (!strcmp(name, OTMA_PARAM_TCK)) {
    *out = (dev && dev->tck_hz) ? dev->tck_hz : g_tck_hz;
  } else if (!strcmp(name, OTMA_PARAM_KERNEL_UPDATE)) {
    *out = 0; // we drive the FT232H from userspace, no kernel driver involved
  }

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

  // Per-adapter buffers are allocated when the adapter is opened.
  return &fns;
}
}
