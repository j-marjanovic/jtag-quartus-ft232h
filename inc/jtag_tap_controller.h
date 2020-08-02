
#pragma once

#include <stdint.h>

#include "sock_debug.h"

typedef enum {
  TAP_STATE_TEST_LOGIC_RESET = 0,
  TAP_STATE_RUN_TEST_IDLE,
  TAP_STATE_SELECT_DR_SCAN,
  TAP_STATE_SELECT_IR_SCAN,
  TAP_STATE_CAPTURE_DR,
  TAP_STATE_SHIFT_DR,
  TAP_STATE_EXIT1_DR,
  TAP_STATE_PAUSE_DR,
  TAP_STATE_EXIT2_DR,
  TAP_STATE_UPDATE_DR,
  TAP_STATE_CAPTURE_IR,
  TAP_STATE_SHIFT_IR,
  TAP_STATE_EXIT1_IR,
  TAP_STATE_PAUSE_IR,
  TAP_STATE_EXIT2_IR,
  TAP_STATE_UPDATE_IR,
  TAP_STATE_NR_OF_ELS // not a real state, just to mark the number of els
} t_jtag_tap_state;

const char *jtag_tap_state_to_str(t_jtag_tap_state state) {
  switch (state) {
  case TAP_STATE_TEST_LOGIC_RESET:
    return "Test-Logic-Reset";
  case TAP_STATE_RUN_TEST_IDLE:
    return "Run-Test/Idle";
  case TAP_STATE_SELECT_DR_SCAN:
    return "Select-DR-Scan";
  case TAP_STATE_SELECT_IR_SCAN:
    return "Select-IR-Scan";
  case TAP_STATE_CAPTURE_DR:
    return "Capture-DR";
  case TAP_STATE_SHIFT_DR:
    return "Shift-DR";
  case TAP_STATE_EXIT1_DR:
    return "Exit1-DR";
  case TAP_STATE_PAUSE_DR:
    return "Pause-DR";
  case TAP_STATE_EXIT2_DR:
    return "Exit2-DR";
  case TAP_STATE_UPDATE_DR:
    return "Update-DR";
  case TAP_STATE_CAPTURE_IR:
    return "Capture-IR";
  case TAP_STATE_SHIFT_IR:
    return "Shift-IR";
  case TAP_STATE_EXIT1_IR:
    return "Exit1-IR";
  case TAP_STATE_PAUSE_IR:
    return "Pause-IR";
  case TAP_STATE_EXIT2_IR:
    return "Exit2-IR";
  case TAP_STATE_UPDATE_IR:
    return "Update-IR";
  default:
    return "<unknown state>";
  }
}

// second value -
t_jtag_tap_state jtag_tap_next_state[TAP_STATE_NR_OF_ELS][2] = {
    [TAP_STATE_TEST_LOGIC_RESET] = {TAP_STATE_RUN_TEST_IDLE,
                                    TAP_STATE_TEST_LOGIC_RESET},
    [TAP_STATE_RUN_TEST_IDLE] = {TAP_STATE_RUN_TEST_IDLE,
                                 TAP_STATE_SELECT_DR_SCAN},
    [TAP_STATE_SELECT_DR_SCAN] = {TAP_STATE_CAPTURE_DR,
                                  TAP_STATE_SELECT_IR_SCAN},
    [TAP_STATE_CAPTURE_DR] = {TAP_STATE_SHIFT_DR, TAP_STATE_EXIT1_DR},
    [TAP_STATE_SHIFT_DR] = {TAP_STATE_SHIFT_DR, TAP_STATE_EXIT1_DR},
    [TAP_STATE_EXIT1_DR] = {TAP_STATE_PAUSE_DR, TAP_STATE_UPDATE_DR},
    [TAP_STATE_PAUSE_DR] = {TAP_STATE_PAUSE_DR, TAP_STATE_EXIT2_DR},
    [TAP_STATE_EXIT2_DR] = {TAP_STATE_SHIFT_DR, TAP_STATE_UPDATE_DR},
    [TAP_STATE_UPDATE_DR] = {TAP_STATE_RUN_TEST_IDLE, TAP_STATE_SELECT_DR_SCAN},
    // TODO...
};

struct jtag_tap_state {
  t_jtag_tap_state cur_state;
  uint64_t dr;
};

void jtag_tap_controller(struct jtag_tap_state *state, int tms, int tdi,
                         int *tdo) {
  debug_write(&_debug_data, "    cur_state = %s, tdi = %d, ",
              jtag_tap_state_to_str(state->cur_state), tdi);

  t_jtag_tap_state nxt_state = jtag_tap_next_state[state->cur_state][tms];

  int tdo_tmp = 0;

  switch (state->cur_state) {
  case TAP_STATE_TEST_LOGIC_RESET:
    state->dr = 0x1234; // IDCODE;
    break;
  case TAP_STATE_SHIFT_DR:
    tdo_tmp = tdi; // state->dr & 0x1;
    state->dr >>= 1;
    break;
  default:
    tdo_tmp = 1;
    break;
  }

  debug_write(&_debug_data, "tdo = %d", tdo_tmp);
  if (tdo) {
    *tdo = tdo_tmp;
  }

  debug_write(&_debug_data, "\n");
  state->cur_state = nxt_state;
}
