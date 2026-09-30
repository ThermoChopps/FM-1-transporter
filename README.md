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

1. Send JieLi USB hardware boot key `0x16EF`.
2. Detect the target acknowledgement.
3. Supply the ROM's 1 ms calibration pulses.
4. Release D+/D- completely.
5. Hand GP0/GP1 to a Pico-PIO-USB host controller.
6. Enumerate `WL82 UBOOT1.00` without moving the cable.
7. Print the USB descriptor to the XIAO's native USB console.

The working USB_KEY implementation is preserved first; PIO USB Host is added after that handoff boundary.

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
- PIO1: planned Pico-PIO-USB host
- GP0/GP1: shared target D+/D-, with explicit ownership handoff
- Target-side USB host: TinyUSB + Pico-PIO-USB
- Future UBOOT transport: JieLi MSC/SCSI command protocol

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Status

Experimental. The current firmware only performs the recovery entry sequence. Flash write support is deliberately not enabled yet.

## License

MIT
