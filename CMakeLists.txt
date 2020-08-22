cmake_minimum_required(VERSION 3.9)

project(jtag_hw_dummy VERSION 0.1 DESCRIPTION "Dummy JTAG device")

include_directories(inc)
add_library(jtag_hw_dummy SHARED src/jtag_hw_dummy.cpp)
set_target_properties(jtag_hw_dummy PROPERTIES COMPILE_FLAGS "-Os -Wall -Wextra -Wno-unused-parameter -std=c++17")

add_executable(test_mpsse src/test_mpsse.c
    openocd/src/helper/binarybuffer.c
    openocd/src/jtag/drivers/mpsse.c
    openocd/src/jtag/drivers/ftdi.c
)
target_include_directories(test_mpsse PRIVATE openocd/src /usr/include/libusb-1.0)
set_target_properties(test_mpsse PROPERTIES COMPILE_FLAGS "-DHAVE_STDINT_H -DHAVE_STDBOOL_H")
target_link_libraries(test_mpsse usb-1.0)
