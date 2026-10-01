#pragma once

#include <stdbool.h>
#include <stdint.h>

// JieLi WL82 UBOOT1.00 + wl82loader (LoaderV2), read-only subset.
// See docs/JIELI_UBOOT_PROTOCOL.md. Core 1 only.
//
// Deliberately absent: erase, flash write, memory write other than the
// loader upload, chip-key write.

#define JL_CHIP_KEY 0x980F
#define JL_FLASH_ID 0x856014
#define JL_FLASH_SIZE (1024u * 1024u)
#define JL_IO_SIZE 512

typedef struct {
    uint16_t chip_key;
    uint8_t dev_type;
    uint32_t flash_id;
} jl_info_t;

// Forget loader state (call on every new mount).
void jl_reset(void);

// True if this firmware was built with the loader blob embedded.
bool jl_have_loader(void);

// Uploads wl82loader to RAM and jumps to it, once per mount.
bool jl_ensure_loader(void);

bool jl_info(jl_info_t *info);

// Reads len (<= JL_IO_SIZE) bytes of SPI flash at addr into buf.
// buf must be word-aligned RAM.
bool jl_flash_read(uint32_t addr, uint16_t len, uint8_t *buf);

// Sends a harmless GET_ONLINE_DEVICE if the loader has been idle for 1 s.
// The loader resets the chip after ~3 s without a command.
void jl_keepalive(void);

uint16_t jl_crc16(const uint8_t *data, uint32_t len, uint16_t crc);
