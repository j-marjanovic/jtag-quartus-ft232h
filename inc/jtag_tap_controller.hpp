
#pragma once

#include <unordered_map>

#include <stdint.h>

#include "sock_debug.h"

enum class JtagTapState {
  TEST_LOGIC_RESET = 0,
  RUN_TEST_IDLE,
  SELECT_DR_SCAN,
  SELECT_IR_SCAN,
  CAPTURE_DR,
  SHIFT_DR,
  EXIT1_DR,
  PAUSE_DR,
  EXIT2_DR,
  UPDATE_DR,
  CAPTURE_IR,
  SHIFT_IR,
  EXIT1_IR,
  PAUSE_IR,
  EXIT2_IR,
  UPDATE_IR,
};

const char *jtag_to_str(JtagTapState state) {
  switch (state) {
  case JtagTapState::TEST_LOGIC_RESET:
    return "Test-Logic-Reset";
  case JtagTapState::RUN_TEST_IDLE:
    return "Run-Test/Idle";
  case JtagTapState::SELECT_DR_SCAN:
    return "Select-DR-Scan";
  case JtagTapState::SELECT_IR_SCAN:
    return "Select-IR-Scan";
  case JtagTapState::CAPTURE_DR:
    return "Capture-DR";
  case JtagTapState::SHIFT_DR:
    return "Shift-DR";
  case JtagTapState::EXIT1_DR:
    return "Exit1-DR";
  case JtagTapState::PAUSE_DR:
    return "Pause-DR";
  case JtagTapState::EXIT2_DR:
    return "Exit2-DR";
  case JtagTapState::UPDATE_DR:
    return "Update-DR";
  case JtagTapState::CAPTURE_IR:
    return "Capture-IR";
  case JtagTapState::SHIFT_IR:
    return "Shift-IR";
  case JtagTapState::EXIT1_IR:
    return "Exit1-IR";
  case JtagTapState::PAUSE_IR:
    return "Pause-IR";
  case JtagTapState::EXIT2_IR:
    return "Exit2-IR";
  case JtagTapState::UPDATE_IR:
    return "Update-IR";
  default:
    return "<unknown state>";
  }
}

std::unordered_map<JtagTapState, std::array<JtagTapState, 2>>
    jtag_tap_next_state = {
        // clang-format off
    {JtagTapState::TEST_LOGIC_RESET,  {JtagTapState::RUN_TEST_IDLE,   JtagTapState::TEST_LOGIC_RESET  }},
    {JtagTapState::RUN_TEST_IDLE,     {JtagTapState::RUN_TEST_IDLE,   JtagTapState::SELECT_DR_SCAN    }},
    {JtagTapState::SELECT_DR_SCAN,    {JtagTapState::CAPTURE_DR,      JtagTapState::SELECT_IR_SCAN    }},
    {JtagTapState::CAPTURE_DR,        {JtagTapState::SHIFT_DR,        JtagTapState::EXIT1_DR          }},
    {JtagTapState::SHIFT_DR,          {JtagTapState::SHIFT_DR,        JtagTapState::EXIT1_DR          }},
    {JtagTapState::EXIT1_DR,          {JtagTapState::PAUSE_DR,        JtagTapState::UPDATE_DR         }},
    {JtagTapState::PAUSE_DR,          {JtagTapState::PAUSE_DR,        JtagTapState::EXIT2_DR          }},
    {JtagTapState::EXIT2_DR,          {JtagTapState::SHIFT_DR,        JtagTapState::UPDATE_DR         }},
    {JtagTapState::UPDATE_DR,         {JtagTapState::RUN_TEST_IDLE,   JtagTapState::SELECT_DR_SCAN    }},
    {JtagTapState::SELECT_IR_SCAN,    {JtagTapState::CAPTURE_IR,      JtagTapState::TEST_LOGIC_RESET  }},
    {JtagTapState::CAPTURE_IR,        {JtagTapState::SHIFT_IR,        JtagTapState::EXIT1_IR          }},
    {JtagTapState::SHIFT_IR,          {JtagTapState::SHIFT_IR,        JtagTapState::EXIT1_IR          }},
    {JtagTapState::EXIT1_IR,          {JtagTapState::PAUSE_IR,        JtagTapState::UPDATE_IR         }},
    {JtagTapState::PAUSE_IR,          {JtagTapState::PAUSE_IR,        JtagTapState::EXIT2_IR          }},
    {JtagTapState::EXIT2_IR,          {JtagTapState::SHIFT_IR,        JtagTapState::UPDATE_IR         }},
    {JtagTapState::UPDATE_IR,         {JtagTapState::RUN_TEST_IDLE,   JtagTapState::SELECT_DR_SCAN    }}
        // clang-format on
};

struct jtag_tap_state {
  JtagTapState cur_state;
  uint64_t dr;
};

void jtag_tap_controller(struct jtag_tap_state *state, int tms, int tdi,
                         int *tdo) {
  debug_write(&_debug_data, "    cur_state = %s, tdi = %d, ",
              jtag_to_str(state->cur_state), tdi);

  JtagTapState nxt_state = jtag_tap_next_state.at(state->cur_state)[tms];

  int tdo_tmp = 0;

  switch (state->cur_state) {
  case JtagTapState::TEST_LOGIC_RESET:
    state->dr = 0x1234; // IDCODE;
    break;
  case JtagTapState::SHIFT_DR:
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
