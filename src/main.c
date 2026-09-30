// FM-1 Transporter - recovery baseline
// Seeed XIAO RP2040: GP0/D6 -> FM-1 D+, GP1/D7 -> FM-1 D-, GND -> GND.
// FM-1 VBUS is not connected in the current battery-powered prototype.
//
// This is the known-good USB_KEY + JieLi ROM calibration baseline.
// It deliberately stops with the target bus released. The next milestone
// replaces the manual cable swap with Pico-PIO-USB host enumeration.

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"

#include "usb_key.pio.h"

#define PIN_DP 0
#define PIN_DM 1
#define LED_R 17
#define LED_G 16
#define LED_B 25

#define USB_KEY_WORD 0x16EF
#define PIO_HZ 1000000
#define KEY_PACKET_US 340
#define GAP_PROBES 4
#define GAP_PROBE_SPACING_US 35
#define PACKETS_PER_POLARITY 40
#define ACK_PROBE_LOWS 2
#define DP_HIGH_TIMEOUT_MS 500
#define SOF_SAMPLE_US 100
#define SOF_DONE_LOW_MS 3
#define SOF_PHASE_TIMEOUT_MS 6000

enum { POL_A_DP_CLOCK, POL_B_DM_CLOCK };

static PIO pio = pio0;
static uint sm_key, sm_sof, off_key, off_sof;
static uint64_t t0;

static const char *pol_name(int pol) {
    return pol == POL_A_DP_CLOCK ? "A(D+clk,D-data)" : "B(D-clk,D+data)";
}

static void dlog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void dlog(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("[%9llu us] ", (unsigned long long)(time_us_64() - t0));
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

static void rgb(bool r, bool g, bool b) {
    gpio_put(LED_R, !r);
    gpio_put(LED_G, !g);
    gpio_put(LED_B, !b);
}

static void pad_setup(uint pin, enum gpio_drive_strength ds) {
    gpio_set_drive_strength(pin, ds);
    gpio_set_slew_rate(pin, GPIO_SLEW_RATE_SLOW);
    gpio_set_pulls(pin, false, true);
}

// This function is also the future recovery -> PIO USB Host handoff boundary.
static void target_bus_release(void) {
    pio_sm_set_enabled(pio, sm_key, false);
    pio_sm_set_enabled(pio, sm_sof, false);

    for (uint pin = PIN_DP; pin <= PIN_DM; pin++) {
        gpio_set_function(pin, GPIO_FUNC_SIO);
        gpio_set_dir(pin, GPIO_IN);
        gpio_put(pin, 0);
    }
}

static bool dp(void) { return gpio_get(PIN_DP); }
static bool dm(void) { return gpio_get(PIN_DM); }

static bool probe_high(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_put(pin, 1);
    gpio_set_dir(pin, GPIO_OUT);
    busy_wait_us_32(2);
    bool v = gpio_get(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_put(pin, 0);
    return v;
}

static void key_start(int pol) {
    uint clock_pin = pol == POL_A_DP_CLOCK ? PIN_DP : PIN_DM;
    uint data_pin = pol == POL_A_DP_CLOCK ? PIN_DM : PIN_DP;

    pio_sm_set_enabled(pio, sm_key, false);

    pio_sm_config c = usb_key_pp_program_get_default_config(off_key);
    sm_config_set_set_pins(&c, PIN_DP, 2);
    sm_config_set_out_pins(&c, data_pin, 1);
    sm_config_set_sideset_pins(&c, clock_pin);
    sm_config_set_out_shift(&c, false, false, 32);
    sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / PIO_HZ);

    pio_sm_init(pio, sm_key, off_key, &c);

    uint32_t mask = (1u << PIN_DP) | (1u << PIN_DM);
    pio_sm_set_pins_with_mask(pio, sm_key, 0, mask);
    pio_sm_set_pindirs_with_mask(pio, sm_key, 0, mask);
    pio_sm_clear_fifos(pio, sm_key);
    pio_sm_set_enabled(pio, sm_key, true);
}

static void key_send_packet(void) {
    pio_gpio_init(pio, PIN_DP);
    pio_gpio_init(pio, PIN_DM);
    pio_sm_put_blocking(pio, sm_key, (uint32_t)USB_KEY_WORD << 16);
    sleep_us(KEY_PACKET_US);
}

static int gap_sense(uint data_pin, bool *alive) {
    int lows = 0;
    int dp_highs = 0;

    for (int i = 0; i < GAP_PROBES; i++) {
        sleep_us(GAP_PROBE_SPACING_US);

        if (dp()) {
            if (++dp_highs >= 2) return 2;
        } else {
            dp_highs = 0;
        }

        if (probe_high(data_pin)) {
            *alive = true;
            lows = 0;
        } else if (*alive && ++lows >= ACK_PROBE_LOWS) {
            return 1;
        }
    }

    return 0;
}

static void sof_start(void) {
    gpio_set_drive_strength(PIN_DP, GPIO_DRIVE_STRENGTH_8MA);
    pio_gpio_init(pio, PIN_DP);

    pio_sm_config c = sof_pulse_program_get_default_config(off_sof);
    sm_config_set_set_pins(&c, PIN_DP, 1);
    sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / PIO_HZ);

    pio_sm_init(pio, sm_sof, off_sof, &c);
    pio_sm_set_pins_with_mask(pio, sm_sof, 0, 1u << PIN_DP);
    pio_sm_set_pindirs_with_mask(pio, sm_sof, 0, 1u << PIN_DP);
    pio_sm_set_enabled(pio, sm_sof, true);
}

static bool wait_dp_high(uint32_t timeout_ms) {
    uint64_t deadline = time_us_64() + (uint64_t)timeout_ms * 1000;
    int highs = 0;

    while (time_us_64() < deadline) {
        if (dp()) {
            if (++highs >= 5) return true;
        } else {
            highs = 0;
        }
        sleep_us(50);
    }

    return false;
}

static bool sof_phase(void) {
    dlog("SOF: D+ high (ROM pull-up), sending 4 us pulses every 1 ms");
    sof_start();

    uint64_t start = time_us_64();
    uint32_t low_run = 0;
    bool ok = false;

    while (time_us_64() - start < (uint64_t)SOF_PHASE_TIMEOUT_MS * 1000) {
        sleep_us(SOF_SAMPLE_US);

        if (!dp()) {
            if (++low_run * SOF_SAMPLE_US >= SOF_DONE_LOW_MS * 1000) {
                ok = true;
                break;
            }
        } else {
            low_run = 0;
        }
    }

    target_bus_release();
    gpio_set_drive_strength(PIN_DP, GPIO_DRIVE_STRENGTH_2MA);

    if (ok) {
        dlog("SOF: D+ released after %.1f ms - calibration done",
             (time_us_64() - start) / 1000.0);
    } else {
        dlog("SOF: timeout, D+ still %d (normal app attached instead of ROM?)", dp());
    }

    return ok;
}

typedef enum { R_DONE, R_RETRY } result_t;

static result_t run_once(void) {
    int pol = POL_A_DP_CLOCK;
    uint32_t packets = 0;
    uint32_t in_block = 0;
    bool alive = false;
    bool was_alive = false;
    uint64_t start = time_us_64();
    uint64_t last_status = start;
    uint64_t last_blink = start;
    bool blink = false;

    key_start(pol);
    dlog("KEY: sending 0x%04X, alternating polarity every %d packets. Switch the FM-1 ON now.",
         USB_KEY_WORD, PACKETS_PER_POLARITY);

    for (;;) {
        key_send_packet();
        packets++;
        in_block++;

        uint data_pin = pol == POL_A_DP_CLOCK ? PIN_DM : PIN_DP;
        int s = gap_sense(data_pin, &alive);

        if (alive && !was_alive) {
            dlog("target alive (data line shows a pull-down) during %s", pol_name(pol));
            was_alive = true;
        }

        if (s) {
            target_bus_release();
            dlog("%s during %s after %lu packets",
                 s == 1 ? "ACK (data line held low)" : "D+ high in gap (ACK missed?)",
                 pol_name(pol), (unsigned long)packets);
            rgb(true, true, false);

            if (s == 2 || wait_dp_high(DP_HIGH_TIMEOUT_MS)) {
                if (sof_phase()) return R_DONE;
                return R_RETRY;
            }

            dlog("no D+ pull-up within %d ms (D+=%d D-=%d) - false ACK, resuming key",
                 DP_HIGH_TIMEOUT_MS, dp(), dm());
            alive = false;
            was_alive = false;
            key_start(pol);
            continue;
        }

        if (in_block >= PACKETS_PER_POLARITY) {
            pol = pol == POL_A_DP_CLOCK ? POL_B_DM_CLOCK : POL_A_DP_CLOCK;
            in_block = 0;
            key_start(pol);
        }

        uint64_t now = time_us_64();

        if (now - last_blink > 250000) {
            blink = !blink;
            rgb(blink, false, false);
            last_blink = now;
        }

        if (now - last_status > 2000000) {
            dlog("KEY: %lu packets, alive=%d", (unsigned long)packets, alive);
            last_status = now;
        }
    }
}

int main(void) {
    // Pico-PIO-USB host timing wants a 120 MHz multiple. Use 120 MHz from the
    // beginning so the known-good recovery path is tested at the final clock.
    set_sys_clock_khz(120000, true);

    stdio_init_all();

    gpio_init(LED_R);
    gpio_set_dir(LED_R, GPIO_OUT);
    gpio_init(LED_G);
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_init(LED_B);
    gpio_set_dir(LED_B, GPIO_OUT);
    rgb(false, false, false);

    gpio_init(PIN_DP);
    gpio_init(PIN_DM);
    pad_setup(PIN_DP, GPIO_DRIVE_STRENGTH_2MA);
    pad_setup(PIN_DM, GPIO_DRIVE_STRENGTH_2MA);

    off_key = pio_add_program(pio, &usb_key_pp_program);
    off_sof = pio_add_program(pio, &sof_pulse_program);
    sm_key = pio_claim_unused_sm(pio, true);
    sm_sof = pio_claim_unused_sm(pio, true);
    target_bus_release();

    for (int i = 0; i < 100 && !stdio_usb_connected(); i++) {
        sleep_ms(100);
    }

    sleep_ms(200);
    t0 = time_us_64();

    printf("\nFM-1 Transporter recovery baseline\n");
    printf("XIAO RP2040: D+=GP0/D6 D-=GP1/D7\n");
    printf("Keep the FM-1 switched OFF until told, then switch it ON.\n");

    for (;;) {
        if (run_once() == R_DONE) {
            rgb(false, true, false);
            dlog("RECOVERY READY: target bus is released.");
            dlog("M0 next step: hand GP0/GP1 to Pico-PIO-USB and enumerate UBOOT.");
            for (;;) sleep_ms(1000);
        }

        rgb(false, false, true);
        dlog("FAILED - switch the FM-1 OFF; restarting the key in 3 s");
        sleep_ms(3000);
    }
}
