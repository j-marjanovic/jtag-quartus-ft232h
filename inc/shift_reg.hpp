
#pragma once

#include <algorithm>
#include <array>
#include <iostream>

/**
 * >-- input     sr     --> output
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
    int out = arr[N - 1];

    std::rotate(std::begin(arr), std::begin(arr) + (N - 1), std::end(arr));

    arr[0] = in;

    return out;
  }

  /** LSB is shifted out first */
  void load(uint64_t data) {
    for (std::size_t i = 0; i < N - 1; i++) {
      arr[N - 1 - i] = (data >> i) & 0x1;
    }
  }

  std::string to_str() const {
    std::string s;
    for (auto &el : arr) {
      s += el;
    }

    return s;
  }
};
