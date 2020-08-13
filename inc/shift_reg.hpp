
#pragma once

#include <algorithm>
#include <array>
#include <iostream>

/**
 *            +-----------------+
 * >--input-->| MSB   ...   LSB |--> output
 *            +-----------------+
 *                shift reg
 *
 *             arr[N-1] . arr[0]
 */

template <std::size_t N> class ShiftReg {

  std::array<int, N> arr;

public:
  explicit ShiftReg() { std::fill(std::begin(arr), std::end(arr), 0); }

  explicit ShiftReg(std::initializer_list<int> l) {
    if (l.size() != N) {
      throw std::runtime_error("initializer list missmatch");
    }

    std::copy(std::begin(l), std::end(l), std::begin(arr));
  }

  int shift(int in) {
    int out = arr[0];

    std::rotate(std::begin(arr), std::begin(arr) + 1, std::end(arr));

    arr[N - 1] = in;

    return out;
  }

  /** LSB is shifted out first */
  void load(uint64_t data) {
    for (std::size_t i = 0; i < N; i++) {
      arr[i] = (data >> i) & 0x1;
    }

    for (std::size_t i = 0; i < N; i++) {
      std::cout << arr[i] << ", ";
    }
    std::cout << "\n";
  }

  uint64_t val() {
    uint64_t tmp = 0;
    for (std::size_t i = 0; i < N; i++) {
      tmp <<= 1;
      tmp |= arr[N - 1 - i];
    }

    return tmp;
  }

  std::string to_str() const {
    std::string s;
    for (std::size_t i = 0; i < N; i++) {
      s += arr[N - 1 - i] ? "1" : "0";
    }

    return s;
  }
};
