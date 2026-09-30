#pragma once

#include <stdbool.h>

// Starts the FM-1-facing PIO USB host after the recovery engine has released
// GP0/GP1. This module is intentionally separate from recovery so the known-good
// USB_KEY timing remains untouched.
//
// M0 implementation target:
//   - Pico-PIO-USB on TinyUSB rhport 1
//   - GP0 = D+, GP1 = D-
//   - enumerate exactly one target
//   - read/log descriptors
//   - no erase/write commands
bool fm1_pio_host_start(void);
bool fm1_pio_host_mounted(void);
