#pragma once

#include <stdbool.h>

// FM-1-facing PIO USB host (TinyUSB rhport 1, Pico-PIO-USB on PIO1,
// GP0 = D+, GP1 = D-). Runs entirely on core 1, after recovery.
//
// M0 scope: enumerate the JieLi ROM UBOOT, log its descriptors and run a
// read-only SCSI INQUIRY over Bulk-Only Transport. No memory or flash
// commands.

// Takes GP0/GP1. Call immediately after recovery_release_bus(); the gap
// between the last ROM pulse and this call should be as short as possible.
void fm1_pio_host_start(void);

// Services the host stack. Call continuously on core 1.
void fm1_pio_host_task(void);

bool fm1_pio_host_mounted(void);

// Queues a one-letter console command for core 1 (safe to call from core 0).
void fm1_pio_host_command(char c);
