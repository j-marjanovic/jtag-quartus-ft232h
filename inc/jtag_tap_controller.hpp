
#pragma once

#include <unordered_map>

#include <stdint.h>

#include "shift_reg.hpp"
#include "sock_debug.hpp"

// #define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(...) sock_debug.debug_write("[jtag] " __VA_ARGS__)
#define DEBUG_HEXDUMP(addr, len) sock_debug.mini_hexdump(addr, len)
#else
#define DEBUG_PRINT(...)                                                       \
  do {                                                                         \
  } while (0)
#define DEBUG_HEXDUMP(...)                                                     \
  do {                                                                         \
  } while (0)
#endif

extern SockDebug sock_debug;

class JtagTapController {

  enum class TapState;
  enum class TapInstr;

  TapState _cur_state;
  TapInstr _cur_instr;
  ShiftReg<10> _ir;
  ShiftReg<32> _dr;
  ShiftReg<1> _bypassr;
  unsigned int _bit_cntr;

public:
  explicit JtagTapController()
      : _cur_state{TapState::TEST_LOGIC_RESET},
        _cur_instr{TapInstr::IDCODE}, _bit_cntr{0} {};

  /** advance TAP controller by one clock cycle */
  void step(int tms, int tdi, int *tdo) {
    DEBUG_PRINT("    cur_state = %s, tdi = %d, tms = %d, ",
                jtag_to_str(_cur_state), tdi, tms);

    TapState nxt_state = jtag_tap_next_state.at(_cur_state)[tms];

    int tdo_tmp = 1;
    switch (_cur_state) {
    case TapState::TEST_LOGIC_RESET:
      _cur_instr = TapInstr::IDCODE;
      break;
    case TapState::CAPTURE_DR:
      DEBUG_PRINT("  CAPTURE_DR (instr = %s), ", instr_to_str(_cur_instr));
      switch (_cur_instr) {
      case TapInstr::IDCODE:
        _dr.load(0x029070DD);
        break;
      case TapInstr::BYPASS:
        _bypassr.load(1);
        break;
      default:
        break;
      }
      break;
    case TapState::SHIFT_DR:
      switch (_cur_instr) {
      case TapInstr::IDCODE:
        tdo_tmp = _dr.shift(tdi);
        break;
      case TapInstr::BYPASS:
        tdo_tmp = _bypassr.shift(tdi);
        break;
      case TapInstr::UNKNOWN1:
        _bit_cntr++;
        if ((_bit_cntr % 1'000'000 == 0) || (_bit_cntr >= 213'798'880)) {
          sock_debug.debug_write("[jtag] received %d bits\n", _bit_cntr);
        }
        break;
      case TapInstr::UNKNOWN2:
        // This instructions looks like reading from a status register.
        // Return all 1s to satisfy the condition to have CONF_DONE high
        // at the end of the programming procedure.
        sock_debug.debug_write("[jtag] TapInstr::UNKNOWN2\n");
        tdo_tmp = 1;
        break;
      default:
        break;
      }
      break;
    case TapState::CAPTURE_IR:
      _ir.load(0b0101010101);
      break;
    case TapState::SHIFT_IR:
      tdo_tmp = _ir.shift(tdi);
      DEBUG_PRINT("SHIFT_IR = %x, ", _ir.val());
      break;
    case TapState::EXIT1_IR:
      // do nothing
      break;
    case TapState::PAUSE_IR:
      // do nothing
      break;
    case TapState::EXIT2_IR:
      // do nothing
      break;
    case TapState::UPDATE_IR:
      DEBUG_PRINT("UPDATE_IR = %x, ", _ir.val());
      _cur_instr = instr_from_bits(_ir.val());
      sock_debug.debug_write("[jtag] UPDATE_IR = %x (%s)\n", _ir.val(),
                             instr_to_str(_cur_instr));
      break;
    default:
      tdo_tmp = 1;
      break;
    }

    DEBUG_PRINT("tdo = %d", tdo_tmp);
    if (tdo) {
      *tdo = tdo_tmp;
    }

    DEBUG_PRINT("\n");
    _cur_state = nxt_state;
  }

private:
  enum class TapState {
    TEST_LOGIC_RESET,
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

  enum class TapInstr {
    SAMPLE_PRELOAD,
    EXTEST,
    BYPASS,
    USER_CODE,
    IDCODE,
    HIGHZ,
    CLAMP,
    PULSE_NCONFIG,
    CONFIG_IO,
    LOCK,
    UNLOCK,
    KEY_CLR_VREG,
    KEY_VERIFY,
    EXTEST_PULSE,
    EXTEST_TRAIN,
    UNKNOWN1,
    UNKNOWN2
  };

  TapInstr instr_from_bits(int val) {
    switch (val) {
    case 0b00'0000'0101:
      return TapInstr::SAMPLE_PRELOAD;
    case 0b00'0000'1111:
      return TapInstr::EXTEST;
    case 0b11'1111'1111:
      return TapInstr::BYPASS;
    case 0b00'0000'0111:
      return TapInstr::USER_CODE;
    case 0b00'0000'0110:
      return TapInstr::IDCODE;
    case 0b00'0000'1011:
      return TapInstr::HIGHZ;
    case 0b00'0000'1010:
      return TapInstr::CLAMP;
    case 0b00'0000'0001:
      return TapInstr::PULSE_NCONFIG;
    case 0b00'0000'1101:
      return TapInstr::CONFIG_IO;
    case 0b01'1111'0000:
      return TapInstr::LOCK;
    case 0b11'0011'0001:
      return TapInstr::UNLOCK;
    case 0b00'0010'1001:
      return TapInstr::KEY_CLR_VREG;
    case 0b00'0001'0011:
      return TapInstr::KEY_VERIFY;
    case 0b00'1000'1111:
      return TapInstr::EXTEST_PULSE;
    case 0b00'0100'1111:
      return TapInstr::EXTEST_TRAIN;
    case 0b00'0000'0010:
      return TapInstr::UNKNOWN1;
    case 0b00'0000'0100:
      return TapInstr::UNKNOWN2;
    }

    // not documented in Stratix V Device Handbook, but still used:
    //   0b00'0000'1110: USER1
    //   0b10'1000'0001: FACTORY
    //   0b00'0000'0010: download config bits
    //   0b00'0000'0100: last instr after programming

    sock_debug.debug_write("UNKNOWN INSTRUCTION (%x), ", val);
    DEBUG_PRINT("UNKNOWN INSTRUCTION (%x), ", val);

    return TapInstr::BYPASS; // just to make the compiler happy
  }

  const char *instr_to_str(TapInstr val) {
    switch (val) {
    case TapInstr::SAMPLE_PRELOAD:
      return "SAMPLE_PRELOAD";
    case TapInstr::EXTEST:
      return "EXTEST";
    case TapInstr::BYPASS:
      return "BYPASS";
    case TapInstr::USER_CODE:
      return "USER_CODE";
    case TapInstr::IDCODE:
      return "IDCODE";
    case TapInstr::HIGHZ:
      return "HIGHZ";
    case TapInstr::CLAMP:
      return "CLAMP";
    case TapInstr::PULSE_NCONFIG:
      return "PULSE_NCONFIG";
    case TapInstr::CONFIG_IO:
      return "CONFIG_IO";
    case TapInstr::LOCK:
      return "LOCK";
    case TapInstr::UNLOCK:
      return "UNLOCK";
    case TapInstr::KEY_CLR_VREG:
      return "KEY_CLR_VREG";
    case TapInstr::KEY_VERIFY:
      return "KEY_VERIFY";
    case TapInstr::EXTEST_PULSE:
      return "EXTEST_PULSE";
    case TapInstr::EXTEST_TRAIN:
      return "EXTEST_TRAIN";
    case TapInstr::UNKNOWN1:
      return "UNKNOWN1";
    case TapInstr::UNKNOWN2:
      return "UNKNOWN2";
    }

    // this should never be reached
    return "<UNKNOWN_INSTR>";
  }

  std::unordered_map<TapState, std::array<TapState, 2>> jtag_tap_next_state = {
      // clang-format off
    {TapState::TEST_LOGIC_RESET,  {TapState::RUN_TEST_IDLE,   TapState::TEST_LOGIC_RESET  }},
    {TapState::RUN_TEST_IDLE,     {TapState::RUN_TEST_IDLE,   TapState::SELECT_DR_SCAN    }},
    {TapState::SELECT_DR_SCAN,    {TapState::CAPTURE_DR,      TapState::SELECT_IR_SCAN    }},
    {TapState::CAPTURE_DR,        {TapState::SHIFT_DR,        TapState::EXIT1_DR          }},
    {TapState::SHIFT_DR,          {TapState::SHIFT_DR,        TapState::EXIT1_DR          }},
    {TapState::EXIT1_DR,          {TapState::PAUSE_DR,        TapState::UPDATE_DR         }},
    {TapState::PAUSE_DR,          {TapState::PAUSE_DR,        TapState::EXIT2_DR          }},
    {TapState::EXIT2_DR,          {TapState::SHIFT_DR,        TapState::UPDATE_DR         }},
    {TapState::UPDATE_DR,         {TapState::RUN_TEST_IDLE,   TapState::SELECT_DR_SCAN    }},
    {TapState::SELECT_IR_SCAN,    {TapState::CAPTURE_IR,      TapState::TEST_LOGIC_RESET  }},
    {TapState::CAPTURE_IR,        {TapState::SHIFT_IR,        TapState::EXIT1_IR          }},
    {TapState::SHIFT_IR,          {TapState::SHIFT_IR,        TapState::EXIT1_IR          }},
    {TapState::EXIT1_IR,          {TapState::PAUSE_IR,        TapState::UPDATE_IR         }},
    {TapState::PAUSE_IR,          {TapState::PAUSE_IR,        TapState::EXIT2_IR          }},
    {TapState::EXIT2_IR,          {TapState::SHIFT_IR,        TapState::UPDATE_IR         }},
    {TapState::UPDATE_IR,         {TapState::RUN_TEST_IDLE,   TapState::SELECT_DR_SCAN    }}
      // clang-format on
  };

  const char *jtag_to_str(TapState state) {
    switch (state) {
    case TapState::TEST_LOGIC_RESET:
      return "Test-Logic-Reset";
    case TapState::RUN_TEST_IDLE:
      return "Run-Test/Idle";
    case TapState::SELECT_DR_SCAN:
      return "Select-DR-Scan";
    case TapState::SELECT_IR_SCAN:
      return "Select-IR-Scan";
    case TapState::CAPTURE_DR:
      return "Capture-DR";
    case TapState::SHIFT_DR:
      return "Shift-DR";
    case TapState::EXIT1_DR:
      return "Exit1-DR";
    case TapState::PAUSE_DR:
      return "Pause-DR";
    case TapState::EXIT2_DR:
      return "Exit2-DR";
    case TapState::UPDATE_DR:
      return "Update-DR";
    case TapState::CAPTURE_IR:
      return "Capture-IR";
    case TapState::SHIFT_IR:
      return "Shift-IR";
    case TapState::EXIT1_IR:
      return "Exit1-IR";
    case TapState::PAUSE_IR:
      return "Pause-IR";
    case TapState::EXIT2_IR:
      return "Exit2-IR";
    case TapState::UPDATE_IR:
      return "Update-IR";
    default:
      return "<unknown state>";
    }
  };
};