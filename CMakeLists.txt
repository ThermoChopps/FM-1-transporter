cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(fm1_transporter C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(fm1_transporter
    src/main.c
)

pico_generate_pio_header(fm1_transporter
    ${CMAKE_CURRENT_LIST_DIR}/pio/usb_key.pio
)

target_link_libraries(fm1_transporter
    pico_stdlib
    hardware_pio
    hardware_gpio
    hardware_clocks
)

pico_enable_stdio_usb(fm1_transporter 1)
pico_enable_stdio_uart(fm1_transporter 0)

pico_add_extra_outputs(fm1_transporter)
