
#include <stdint.h>

typedef void (*fn_op_store_tdo)(void *arg1, uint32_t *buf, uint32_t count);
typedef void (*fn_op_store_unknown_tdo)(void *param_1, uint32_t param_2);
typedef void (*fn_op_tdo_needed)(void *param_1, int32_t param_2);
typedef void (*fn_op_indicate_flush)(void *param_1);

struct server_ops_t {
  void *rsvd0;
  fn_op_store_tdo p_op_store_tdo;
  fn_op_store_unknown_tdo p_op_store_unknown_tdo;
  void *rsvd1;
  fn_op_tdo_needed p_op_tdo_needed;
  fn_op_indicate_flush p_op_indicate_flush;
} __attribute__((packed));

typedef char (*fn_find_devs)(uint32_t dev_index, char *out_desc,
                             uint32_t api_ver);
typedef char (*fn_find_descriptions)(const char *description);
typedef int64_t (*fn_init_dev)(void **arg1, const char *desc,
                               struct server_ops_t *s_server_ops, void *parent);
typedef void (*fn_close)(void *unused_void);
typedef uint64_t (*fn_set_param)(void *arg1, char *arg2, uint32_t arg3);
typedef int64_t (*fn_get_param)(void *arg1, char *arg2, uint32_t *arg3);
typedef int64_t (*fn_direct_control)(void *arg1, int32_t arg2, int32_t *arg3);
typedef void (*fn_clock_raw)(void *unused_void, uint32_t jtag_tms, uint32_t v,
                             unsigned long len);
typedef int64_t (*fn_clock_multiple)(void *unused_void, unsigned jtag_tms,
                                     unsigned long *p_bits, unsigned long len,
                                     unsigned long field_144_minus_len);
typedef int64_t (*fn_do_flush)(void *unused_void, int bool_val,
                                uint32_t index_val);

struct virtual_fns_st { /* ABI to pass data in and out of driver */
  uint32_t st_size;     /* to check ABI compatibility */
  char dev_description[32];
  uint32_t st_flags;
  void *reserved01[2];
  fn_find_devs p_find_devs;                 // aka scan_port
  fn_find_descriptions p_find_descriptions; // aka verify_port
  fn_init_dev p_init_dev;                   // aka open_hw
  fn_close p_close;
  fn_set_param p_set_param;
  fn_get_param p_get_param;
  fn_direct_control p_direct_control;
  fn_clock_raw p_clock_raw;
  fn_clock_multiple p_clock_multiple;
  void *reserved03[1];
  fn_do_flush p_do_flush;
  void *reserved04[5];
} __attribute__((packed));
