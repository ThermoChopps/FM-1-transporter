cmake_minimum_required(VERSION 3.13)

# Pico-PIO-USB is vendored as a submodule, pinned to the release TinyUSB in
# the SDK expects. It must be set before the SDK pulls in TinyUSB.
set(PICO_PIO_USB_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/Pico-PIO-USB)

# TinyUSB debug level (CFG_TUSB_DEBUG). 2 logs host enumeration during M0.
set(LOG 2)

include(pico_sdk_import.cmake)

project(fm1_transporter C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

function(fm1t_add_firmware TARGET)
    add_executable(${TARGET}
        src/main.c
        src/recovery.c
        src/pio_host.c
        src/usb_device.c
        src/log.c
    )

    pico_generate_pio_header(${TARGET}
        ${CMAKE_CURRENT_LIST_DIR}/pio/usb_key.pio
    )

    # tusb_config.h
    target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

    target_link_libraries(${TARGET}
        pico_stdlib
        pico_multicore
        pico_unique_id
        pico_usb_reset_interface_headers
        hardware_pio
        hardware_gpio
        hardware_clocks
        hardware_dma
        tinyusb_device
        tinyusb_host
        tinyusb_pico_pio_usb
    )

    # The console is our own TinyUSB CDC device (see src/usb_device.c,
    # src/log.c); pico_stdio_usb cannot coexist with the TinyUSB host stack.
    pico_enable_stdio_usb(${TARGET} 0)
    pico_enable_stdio_uart(${TARGET} 0)

    pico_add_extra_outputs(${TARGET})
endfunction()

# M0 firmware: USB_KEY recovery -> PIO USB host.
fm1t_add_firmware(fm1_transporter)

# Host bring-up firmware: PIO USB host only, no USB_KEY.
fm1t_add_firmware(fm1_transporter_hostonly)
target_compile_definitions(fm1_transporter_hostonly PRIVATE FM1T_HOST_ONLY=1)
