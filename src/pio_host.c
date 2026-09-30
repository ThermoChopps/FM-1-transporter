#include "pio_host.h"

// M0 host integration notes:
//
// Pico-PIO-USB's current dual-controller example configures the PIO controller
// with:
//
//   pio_usb_configuration_t cfg = PIO_USB_DEFAULT_CONFIG;
//   cfg.pin_dp = 0;
//   tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &cfg);
//   tuh_init(1);
//
// and services it continuously with tuh_task().
//
// We do NOT enable that code in the recovery baseline yet. Native USB stdio in
// the baseline also owns TinyUSB device plumbing; the M0 host commit will switch
// to an explicit TinyUSB dual-role configuration (native device on rhport 0,
// PIO host on rhport 1) rather than mixing two independent TinyUSB owners.
//
// This stub makes the architectural boundary concrete without risking the
// already-working USB_KEY recovery path.

bool fm1_pio_host_start(void) {
    return false;
}

bool fm1_pio_host_mounted(void) {
    return false;
}
