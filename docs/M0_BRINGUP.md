# M0 bring-up checklist

The objective is one automatic transition:

```
USB_KEY -> JieLi ROM calibration -> PIO USB Host -> UBOOT descriptor
```

No flash writes in M0.

## 1. Baseline

- [ ] Build `fm1_transporter.uf2`.
- [ ] Flash XIAO RP2040.
- [ ] Verify USB console appears.
- [ ] Verify existing three-wire recovery still reaches `RECOVERY READY` at 120 MHz.

If this fails, revert only the 120 MHz clock change first. The PIO key timing divider is calculated from `clk_sys`, but the regression must still be measured on hardware.

## 2. Replace stdio USB ownership

Do not simply add `tuh_init(1)` beside Pico SDK USB stdio.

Move the Mac-facing port to an explicit TinyUSB device CDC configuration:

- rhport 0: RP2040 native USB device / CDC
- rhport 1: Pico-PIO-USB host
- core 0: device/control task
- core 1: host task after handoff

This follows Pico-PIO-USB's dual-controller example and avoids two independent owners of the TinyUSB stack.

## 3. Delayed host start

Core 1 waits on a start flag. It must not initialize or drive the PIO USB host while recovery owns GP0/GP1.

After `sof_phase()` succeeds:

1. `target_bus_release()`
2. verify both recovery state machines are disabled
3. leave GP0/GP1 as inputs
4. short guard delay
5. signal core 1
6. core 1 configures PIO USB with `pin_dp = 0`
7. `tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &cfg)`
8. `tuh_init(1)`
9. repeatedly call `tuh_task()`

## 4. Enumeration callback

First success criterion:

```
RECOVERY READY
PIO HOST START
DEVICE ATTACHED addr=1
VID=....
PID=....
DEVICE DESCRIPTOR OK
```

Read device and configuration descriptors. Record interface class/subclass/protocol and endpoints before choosing the UBOOT transport implementation.

## 5. Only then enable MSC

The JieLi UBOOT work should be implemented read-only first:

- probe
- chip/flash info
- dump

Do not expose erase/write until dump + verification are reliable.
