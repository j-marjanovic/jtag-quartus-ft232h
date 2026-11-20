cmake_minimum_required(VERSION 3.9)

project(jtag_hw_dummy VERSION 0.1 DESCRIPTION "Dummy JTAG device")

include_directories(inc)
add_library(jtag_hw_dummy SHARED src/jtag_hw_dummy.cpp)
set_target_properties(jtag_hw_dummy PROPERTIES COMPILE_FLAGS "-g -Wall -Wextra -Wno-unused-parameter -std=c++17")

add_library(jtag_hw_otma SHARED src/jtag_hw_otma.cpp
    openocd/src/helper/binarybuffer.c
    openocd/src/jtag/drivers/mpsse.c
    openocd/src/jtag/drivers/ftdi.c
)
target_compile_features(jtag_hw_otma PRIVATE cxx_std_17)
target_compile_options(jtag_hw_otma PRIVATE -g -Wall -Wextra -Wno-unused-parameter)

# mpsse.c only implements location-based device selection when this is set;
# it is normally defined by OpenOCD's config.h, which we do not have.
target_compile_definitions(jtag_hw_otma PRIVATE
    HAVE_STDINT_H
    HAVE_STDBOOL_H
    HAVE_LIBUSB_GET_PORT_NUMBERS=1
)

target_include_directories(jtag_hw_otma PRIVATE openocd/src /usr/include/libusb-1.0)
target_link_libraries(jtag_hw_otma usb-1.0)

add_executable(test_mpsse src/test_mpsse.c
    openocd/src/helper/binarybuffer.c
    openocd/src/jtag/drivers/mpsse.c
    openocd/src/jtag/drivers/ftdi.c
)
target_include_directories(test_mpsse PRIVATE openocd/src /usr/include/libusb-1.0)
set_target_properties(test_mpsse PROPERTIES COMPILE_FLAGS "-DHAVE_STDINT_H -DHAVE_STDBOOL_H")
target_link_libraries(test_mpsse usb-1.0)
