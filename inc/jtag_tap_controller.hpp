
#pragma once

#include <unordered_map>

#include <stdint.h>

#include "shift_reg.hpp"
#include "sock_debug.hpp"

extern SockDebug sock_debug;

class JtagTapController {

  enum class JtagTapState;

  JtagTapState _cur_state;
  ShiftReg<10> _ir;
  ShiftReg<32> _dr;

public:
  explicit JtagTapController() : _cur_state{JtagTapState::TEST_LOGIC_RESET} {};

  /** advance TAP controller by one clock cycle */
  void step(int tms, int tdi, int *tdo) {
    sock_debug.debug_write("    cur_state = %s, tdi = %d, tms = %d, ",
                           jtag_to_str(_cur_state), tdi, tms);

    JtagTapState nxt_state = jtag_tap_next_state.at(_cur_state)[tms];

    int tdo_tmp = 0;
    switch (_cur_state) {
    case JtagTapState::TEST_LOGIC_RESET:
      break;
    case JtagTapState::CAPTURE_DR:
      _dr.load(0x12341);
      // sock_debug.debug_write("DR = %s, ", _dr.to_str());
      break;
    case JtagTapState::SHIFT_DR:
      tdo_tmp = _dr.shift(tdi);
      break;
    case JtagTapState::CAPTURE_IR:
      _ir.load(0b01);
      break;
    case JtagTapState::SHIFT_IR:
      tdo_tmp = _ir.shift(tdi);
      break;
    default:
      tdo_tmp = 1;
      break;
    }

    sock_debug.debug_write("tdo = %d", tdo_tmp);
    if (tdo) {
      *tdo = tdo_tmp;
    }

    sock_debug.debug_write("\n");
    _cur_state = nxt_state;
  }

private:
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
  };
};