# FM-1 Transporter architecture

## Design rule

Do not change the already-working JieLi USB_KEY sequence while bringing up the USB host. The first host milestone begins only after the ROM calibration phase has succeeded.

## Target state machine

```
WAIT_TARGET_POWER
      |
      v
SEND_USB_KEY
      |
      v
WAIT_ACK
      |
      v
WAIT_DP_PULLUP
      |
      v
ROM_SOF_CALIBRATION
      |
      v
RELEASE_TARGET_BUS
      |
      v
PIO_USB_HOST_START
      |
      v
USB_ENUMERATION
      |
      v
UBOOT_READY
```

On any failure before `UBOOT_READY`, the target pins must return to high impedance before retrying.

## Pin ownership

### Recovery phase

PIO0 owns GP0/GP1 only while a USB_KEY packet or calibration pulse is actively being generated.

### Handoff

The handoff must be explicit:

1. Disable the USB_KEY state machine.
2. Disable the calibration/SOF state machine.
3. Return GP0/GP1 to SIO.
4. Set both pins to input/high-impedance.
5. Wait for a short guard interval.
6. Initialize Pico-PIO-USB on PIO1.
7. Start TinyUSB host on rhport 1.

Never allow the recovery PIO and USB host PIO to drive GP0/GP1 simultaneously.

## USB roles

```
RP2040 native USB controller
  role: USB device
  peer: Mac
  purpose: control / logs / future fm1t protocol

RP2040 PIO USB controller
  role: USB host
  peer: FM-1 JieLi ROM/UBOOT
  purpose: enumeration and UBOOT transport
```

Pico-PIO-USB requires one PIO block, three state machines, two adjacent GPIO pins, and a 1 ms repeating timer in host mode. GP0=D+ and GP1=D- satisfy its adjacent-pin convention.

The RP2040 system clock must be selected to satisfy Pico-PIO-USB timing; 120 MHz is the initial target.

## Bring-up gates

### Gate A - recovery regression

The imported USB_KEY implementation must still reach ROM calibration complete on the existing three-wire prototype.

### Gate B - electrical handoff

After calibration, both GP0/GP1 must remain released until the PIO host takes ownership.

### Gate C - host attach

TinyUSB must call `tuh_mount_cb()` for the FM-1.

### Gate D - descriptor

Read and log the device descriptor and configuration descriptor. Do not implement writes yet.

### Gate E - MSC/SCSI

After the actual UBOOT interface is confirmed, implement only the required MSC/SCSI transport used by the JieLi UBOOT protocol.

### Gate F - read-only UBOOT

Implement `info` and flash dump before erase/write.

### Gate G - guarded writes

Add erase/write/verify only after readback is reliable. Preserve the stock SPL/UBOOT region by policy; initial CFW work targets the application area only.

## Planned source split

```
src/
  main.c              state machine / console
  recovery.c          USB_KEY + calibration
  target_bus.c        GP0/GP1 ownership and handoff
  pio_host.c          Pico-PIO-USB / TinyUSB host
  jieli_uboot.c       UBOOT protocol
  transporter_proto.c Mac-facing command protocol

pio/
  usb_key.pio
```

The first commit intentionally keeps the known-good recovery code together. Split it only after the baseline is reproduced from this repository.
