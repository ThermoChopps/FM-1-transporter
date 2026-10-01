// FM-1 Transporter - M0: automatic UBOOT enumeration
// Seeed XIAO RP2040: GP0/D6 -> FM-1 D+, GP1/D7 -> FM-1 D-, GND -> GND.
// FM-1 VBUS is not connected in the current battery-powered prototype.
//
// core 0: native USB device (Mac-facing CDC console) and log output
// core 1: USB_KEY recovery on PIO0 -> handoff -> Pico-PIO-USB host on PIO1
//
// GP0/GP1 have exactly one owner at a time, and both owners live on core 1:
// recovery hands the bus to the host in one call sequence, with the ROM
// pulses running until the moment the host takes the pins.

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "tusb.h"

#include "log.h"
#include "pio_host.h"
#include "recovery.h"

#define CONSOLE_WAIT_MS 10000

static volatile bool console_ready;

static void core1_main(void) {
    for (int i = 0; i < CONSOLE_WAIT_MS / 100 && !console_ready; i++) {
        sleep_ms(100);
    }
    sleep_ms(200);

    printf("\nFM-1 Transporter M0\n");
    printf("XIAO RP2040: D+=GP0/D6 D-=GP1/D7, PIO host on PIO1\n");
    printf("Keep the FM-1 switched OFF until told, then switch it ON.\n");

    recovery_init();
#if FM1T_HOST_ONLY
    // Host bring-up without the key path: attach any full-speed device, or an
    // FM-1 already in UBOOT (e.g. via the V15 USB-MIDI soft key).
    dlog("HOST-ONLY build: skipping USB_KEY recovery");
#else
    recovery_run();
#endif

    // The ROM is holding D+ up and our pulses are still running. Swap owners
    // with the smallest possible gap.
    uint64_t t_release = time_us_64();
    recovery_release_bus();
    fm1_pio_host_start();
    uint64_t t_host = time_us_64();

    rgb(true, true, true);
    dlog("HANDOFF: pulses stopped, PIO host started in %llu us",
         (unsigned long long)(t_host - t_release));

    for (;;) {
        fm1_pio_host_task();
    }
}

void tud_cdc_rx_cb(uint8_t itf) {
    (void)itf;
    char buf[16];
    uint32_t n = tud_cdc_read(buf, sizeof(buf));
    for (uint32_t i = 0; i < n; i++) {
        if (buf[i] > ' ') fm1_pio_host_command(buf[i]);
    }
}

int main(void) {
    // Pico-PIO-USB needs clk_sys to be a multiple of 12 MHz. The recovery
    // PIO dividers are derived from clk_sys, so they follow automatically.
    set_sys_clock_khz(120000, true);

    log_init();
    tud_init(0);

    multicore_launch_core1(core1_main);

    for (;;) {
        tud_task();
        console_ready = tud_cdc_connected();
        log_drain();
    }
}
