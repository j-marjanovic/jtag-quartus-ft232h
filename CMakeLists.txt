cmake_minimum_required(VERSION 3.9)

project(jtag_hw_dummy VERSION 0.1 DESCRIPTION "Dummy JTAG device")

include_directories(inc)
add_library(jtag_hw_dummy SHARED src/jtag_hw_dummy.c)
set_target_properties(jtag_hw_dummy PROPERTIES COMPILE_FLAGS "-g -Wall -Wextra -Wno-unused-parameter")
