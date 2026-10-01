# FM-1 Transporter

An RP2040-based recovery, transport, and programming bridge for the M-VAVE FM-1 (JieLi AC791N/WL82 family).

The first hardware target is the **Seeed XIAO RP2040**.

## Goal

FM-1 Transporter is intended to remove the manual USB cable swap from the FM-1 recovery workflow:

```
Mac / Agent
    |
    | native USB (CDC/control)
    v
XIAO RP2040
    |
    | GP0/GP1
    | USB_KEY -> ROM calibration -> PIO USB Host
    v
M-VAVE FM-1
    |
    v
JieLi UBOOT / SPI flash
```

The long-term command surface is intentionally small:

```
fm1t probe
fm1t boot
fm1t info
fm1t dump stock.bin
fm1t flash app.bin
fm1t verify app.bin
fm1t reset
```

## Current milestone

**M0: automatic UBOOT enumeration**

1. Send JieLi USB hardware boot key `0x16EF` (polarity A).
2. Detect the target acknowledgement.
3. Supply 1 ms pulses while the ROM holds D+ high.
4. Release D+/D- and hand GP0/GP1 to a Pico-PIO-USB host controller immediately.
5. Enumerate `WL80UBOOT1.00` (`4C4A:8057`) without moving the cable.
6. Print the USB descriptors and a read-only SCSI INQUIRY to the XIAO's native USB console.

See [docs/M0_BRINGUP.md](docs/M0_BRINGUP.md) and [docs/JIELI_UBOOT_PROTOCOL.md](docs/JIELI_UBOOT_PROTOCOL.md).

## Prototype wiring

| XIAO RP2040 | FM-1 |
|---|---|
| D6 / GP0 | D+ |
| D7 / GP1 | D- |
| GND | GND |

The current prototype does not connect FM-1 VBUS; the FM-1 runs from its own battery.

For a permanent programmer, add approximately 22-27 ohm series resistors on D+ and D-.

## Architecture

- RP2040 native USB: Mac-facing device/control channel
- PIO0: JieLi USB_KEY and ROM calibration pulse generator
- PIO1: Pico-PIO-USB host
- GP0/GP1: shared target D+/D-, with explicit ownership handoff
- Target-side USB host: TinyUSB + Pico-PIO-USB
- Future UBOOT transport: JieLi MSC/SCSI command protocol

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Status

Experimental. M0 firmware is implemented but not yet tested on hardware. It performs the recovery entry, the PIO USB host handoff and read-only descriptor/INQUIRY probing. Memory and flash commands are deliberately not implemented yet.

## License

MIT
