// Copyright 2023 sekigon-gonnoc
/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "print.h"
#include "via.h"
#include "vial.h"
#include "vial_generated_keyboard_definition.h"
#include "matrix.h"

#include "bmp.h"
#include "bmp_vial.h"
#include "bmp_flash.h"
#include "raw_hid.h"
#include "apidef.h"

#ifndef BMP_VIAL_MODE_DEFAULT
#    define BMP_VIAL_MODE_DEFAULT false
#endif

extern void raw_hid_receive_qmk(uint8_t *data, uint8_t length); // VIA implementation in original qmk

_Static_assert(sizeof(flash_vial_data_t) == BMP_USER_FLASH_PAGE_SIZE, "Invalid size");
_Static_assert(sizeof(bmp_api_config_t) == 192, "Invalid size");
flash_vial_data_t flash_vial_data;
bool              is_vial_enabled = BMP_VIAL_MODE_DEFAULT;

void bmp_vial_data_init(void) {
    const uint8_t lzma_header[] = {0xfd, 0x37, 0x7a, 0x58, 0x5a};

    BMPAPI->flash.read(FLASH_PAGE_ID_VIAL * BMP_USER_FLASH_PAGE_SIZE, (void *)&flash_vial_data, BMP_USER_FLASH_PAGE_SIZE);

    if (flash_vial_data.magic != BMP_VIAL_FLASH_PAGE_MAGIC         //
        || flash_vial_data.len > sizeof(flash_vial_data.vial_data) //
        || (memcmp(flash_vial_data.vial_data, lzma_header, sizeof(lzma_header)) != 0)) {
        // Load default data
        flash_vial_data.magic = BMP_VIAL_FLASH_PAGE_MAGIC;
        flash_vial_data.len = sizeof(keyboard_definition);
        memcpy(flash_vial_data.vial_data, keyboard_definition, sizeof(keyboard_definition));

        flash_vial_data.bmp_config = default_config;

        uint8_t keyboard_uid[] = VIAL_KEYBOARD_UID;
        memcpy(flash_vial_data.vial_uid, keyboard_uid, sizeof(flash_vial_data.vial_uid));
    }
}

const flash_vial_data_t *bmp_vial_get_config(void) {
    return &flash_vial_data;
}

void bmp_vial_save_config(void) {
    // Write to flash
    flash_erase_page(FLASH_PAGE_ID_VIAL);
    flash_write_page(FLASH_PAGE_ID_VIAL, (uint32_t *)&flash_vial_data);
}

static bool pre_raw_hid_receive(uint8_t *msg, uint8_t len) {
    bool _continue = true;

    // Override VIA protocol version
    if (msg[0] == id_get_protocol_version) {
        const uint16_t via_version = is_vial_enabled ? VIAL_SUPPORT_VIA_VERSION : VIA_PROTOCOL_VERSION;
        msg[1]                     = via_version >> 8;
        msg[2]                     = via_version & 0xFF;

        _continue = false;
    }
    // Override matrix test
    else if (msg[0] == id_get_keyboard_value && msg[1] == id_switch_matrix_state) {
        _continue = false;
        uint8_t i      = is_vial_enabled ? 2 : 3;
        uint8_t offset = is_vial_enabled ? 0 : msg[2];
        for (uint8_t row = 0; row < bmp_config->matrix.rows; row++) {
            matrix_row_t value = matrix_get_row(row + offset);

            if (bmp_config->matrix.cols > 24) {
                msg[i++] = (value >> 24) & 0xFF;
                if (i >= 32) break;
            }

            if (bmp_config->matrix.cols > 16) {
                msg[i++] = (value >> 16) & 0xFF;
                if (i >= 32) break;
            }

            if (bmp_config->matrix.cols > 8) {
                msg[i++] = (value >> 8) & 0xFF;
                if (i >= 32) break;
            }

            msg[i++] = value & 0xFF;
            if (i >= 32) break;
        }
    }
    // Override vial protocol
    else if (msg[0] == 0xfe) {
        switch (msg[1]) {
            case vial_get_keyboard_id: {
                _continue = false;

                is_vial_enabled = true;

                memset(msg, 0, len);
                msg[0] = VIAL_PROTOCOL_VERSION & 0xFF;
                msg[1] = (VIAL_PROTOCOL_VERSION >> 8) & 0xFF;
                msg[2] = (VIAL_PROTOCOL_VERSION >> 16) & 0xFF;
                msg[3] = (VIAL_PROTOCOL_VERSION >> 24) & 0xFF;
                memcpy(&msg[4], flash_vial_data.vial_uid, 8);
#ifdef VIALRGB_ENABLE
                msg[12] = 1; /* bit flag to indicate vialrgb is supported - so third-party apps don't have to query json */
#endif
                break;
            }

            case vial_get_size: {
                _continue   = false;
#ifdef BMP_USE_DEFAULT_VIAL_CONFIG
                uint32_t sz = sizeof(keyboard_definition);
#else
                uint32_t sz = flash_vial_data.len;
#endif
                msg[0]      = sz & 0xFF;
                msg[1]      = (sz >> 8) & 0xFF;
                msg[2]      = (sz >> 16) & 0xFF;
                msg[3]      = (sz >> 24) & 0xFF;
                break;
            }

            case vial_get_def: {
                _continue      = false;
                uint32_t page  = msg[2] + (msg[3] << 8);
                uint32_t start = page * len;
                uint32_t end   = start + len;
#ifdef BMP_USE_DEFAULT_VIAL_CONFIG
                if (end < start || start >= sizeof(keyboard_definition)) break;
                if (end > sizeof(keyboard_definition)) end = sizeof(keyboard_definition);
                memcpy(msg, &keyboard_definition[start], end - start);
#else
                if (end < start || start >= flash_vial_data.len) break;
                if (end > flash_vial_data.len) end = flash_vial_data.len;
                memcpy(msg, &flash_vial_data.vial_data[start], end - start);
#endif
                break;
            }
        }
    }

    if (!_continue) {
        raw_hid_send(msg, len);
    }

    return _continue;
}

void bmp_raw_hid_receive_common(const uint8_t *data, uint8_t len) {
    static uint8_t via_data[32];
    if (len > sizeof(via_data) + 1) {
        printf("<raw_hid>Too large packet");
        return;
    }

    memset(via_data, 0, sizeof(via_data));
    memcpy(via_data, data, len);

    if (pre_raw_hid_receive(via_data, len)) {
        if (is_vial_enabled) {
            raw_hid_receive(via_data, len);
        } else {
            raw_hid_receive_qmk(via_data, len);
        }
    }
}

void bmp_set_vial_enable_flag(bool flag) {
    is_vial_enabled = flag;
}