// Copyright 2023 sekigon-gonnoc
/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>

#include "action.h"
#include "action_util.h"
#include "bmp.h"
#include "send_string.h"
#include "print.h"
#include "vial.h"
#include "wait.h"
#include "qmk_settings.h"

#include "apidef.h"
#include "bmp_custom_keycodes.h"
#include "bmp_host_driver.h"
#include "bmp_settings.h"
#include "state_controller.h"

extern uint8_t extract_mod_bits(uint16_t code);

#define DEFFERED_KEY_RECORD_LEN 8
static keyrecord_t deferred_key_record[DEFFERED_KEY_RECORD_LEN];
static uint8_t     dkr_widx, dkr_ridx;

static uint8_t  encoder_modifier            = 0;
static uint16_t encoder_modifier_pressed_ms = 0;

#ifndef ENCODER_MODIFIER_TIMEOUT_MS
#    define ENCODER_MODIFIER_TIMEOUT_MS 500
#endif

static int push_deferred_key_record(uint16_t keycode, keyevent_t *event) {
    uint8_t widx = dkr_widx;
    uint8_t next_idx = dkr_widx+1;
    if (next_idx>= DEFFERED_KEY_RECORD_LEN) {
        next_idx = 0;
    }

    if (dkr_ridx == next_idx) {
        // fifo full
        return -1;
    }

    keyrecord_t record        = {.event = *event, .keycode = keycode};
    deferred_key_record[widx] = record;
    dkr_widx                  = next_idx;

    return 0;
}

bool process_record_bmp(uint16_t keycode, keyrecord_t *record) {
    bool is_encoder_action = record->event.type == ENCODER_CCW_EVENT || record->event.type == ENCODER_CW_EVENT;
    if (encoder_modifier != 0 && !is_encoder_action) {
        unregister_mods(encoder_modifier);
        encoder_modifier = 0;
    }

    if (is_encoder_action && (keycode >= QK_MODS && keycode <= QK_MODS_MAX)) {
        if (record->event.pressed) {
            uint8_t current_mods        = keycode >> 8;
            encoder_modifier_pressed_ms = timer_read();
            if (current_mods != encoder_modifier) {
                del_mods(encoder_modifier);
                encoder_modifier = current_mods;
                add_mods(encoder_modifier);
            }
            register_code(keycode & 0xff);
        } else {
            unregister_code(keycode & 0xff);
        }
        return false;
    }

    if ((dkr_ridx != dkr_widx) && record->event.key.row != VIAL_MATRIX_MAGIC) {
        keyevent_t deferred_key_event = (keyevent_t){.type = KEY_EVENT, .key = (keypos_t){.row = VIAL_MATRIX_MAGIC, .col = VIAL_MATRIX_MAGIC}, .pressed = record->event.pressed, .time = record->event.time};
        return push_deferred_key_record(keycode, &deferred_key_event);
    }

    // To apply key overrides to keycodes combined shift modifier, separate to two actions
    if (keycode >= QK_MODS && keycode <= QK_MODS_MAX) {
        if (record->event.pressed) {
            keyevent_t deferred_key_event = (keyevent_t){.type = KEY_EVENT, .key = (keypos_t){.row = VIAL_MATRIX_MAGIC, .col = VIAL_MATRIX_MAGIC}, .pressed = record->event.pressed, .time = record->event.time};
            register_weak_mods(extract_mod_bits(keycode));
            wait_ms(QS_tap_code_delay);
            return push_deferred_key_record(keycode, &deferred_key_event);
        } else {
            keyevent_t deferred_key_event = ((keyevent_t){.type = KEY_EVENT, .key = (keypos_t){.row = VIAL_MATRIX_MAGIC, .col = VIAL_MATRIX_MAGIC}, .pressed = record->event.pressed, .time = record->event.time});
            unregister_weak_mods(extract_mod_bits(keycode));
            wait_ms(QS_tap_code_delay);
            return push_deferred_key_record(keycode, &deferred_key_event);
        }
    }

    if (record->event.pressed) {
        switch (keycode) {
            case SEL_BLE:
                select_ble();
                return false;
            case SEL_USB:
                select_usb();
                return false;
            case ADV_ID0 ... ADV_ID7:
                BMPAPI->ble.advertise(keycode - ADV_ID0);
                return false;
            case AD_WO_L:
                BMPAPI->ble.advertise(255);
                return false;
            case DEL_ID0 ... DEL_ID7:
                BMPAPI->ble.delete_bond(keycode - DEL_ID0);
                return false;
            case DELBNDS:
                BMPAPI->ble.delete_bond(255);
                return false;
            case BATT_LV: {
                char str[16];
                snprintf(str, sizeof(str), "%4dmV", BMPAPI->app.get_vcc_mv(0));
                if (bmp_config->mode == SPLIT_MASTER) {
                    snprintf(str + 6, sizeof(str) - 6, " %4dmV", BMPAPI->app.get_vcc_mv(1));
                }
                send_string(str);
                return false;
            }
            case DISABLE_KEY_OS_OVERRIDE: {
                println("Disable key os overrides");
                bmp_set_key_os_override(BMP_KEY_OS_OVERRIDE_DISABLE);
                return false;
            }
            case ENABLE_US_KEY_ON_JP_OS_OVERRIDE: {
                println(
                    "Perform as an US keyboard on the OS configured for JP");
                bmp_set_key_os_override(BMP_US_KEY_JP_OS_OVERRIDE);
                return false;
            }
            case ENABLE_JP_KEY_ON_US_OS_OVERRIDE: {
                println("Perform as a JP keyboard on the OS configured for US");
                bmp_set_key_os_override(BMP_JP_KEY_US_OS_OVERRIDE);
                return false;
            }
            case EN_WEBBT: {
                BMPAPI->ble.change_conn_mode(true);
                return false;
            }
            case DIS_WEBBT: {
                BMPAPI->ble.change_conn_mode(false);
                return false;
            }
        }
    }
    else {
        switch (keycode) {
            case ENT_SLP: {
                sleep_enter_counter = 1;
                return false;
            }
        }
    }

    return true;
}

void bmp_post_keyboard_task(void) {
    while (dkr_ridx != dkr_widx) {
        uint8_t ridx = dkr_ridx;
        dkr_ridx++;
        if (dkr_ridx >= DEFFERED_KEY_RECORD_LEN) {
            dkr_ridx = 0;
        }

        if (deferred_key_record[ridx].keycode != KC_NO) {
            if (deferred_key_record[ridx].keycode >= QK_MODS && deferred_key_record[ridx].keycode <= QK_MODS_MAX) {
                uint16_t kc                       = deferred_key_record[ridx].keycode;
                uint8_t weak_mods                 = get_weak_mods();
                uint8_t mods                      = get_mods();
                uint8_t kc_mods                   = extract_mod_bits(kc);

                if (deferred_key_record[ridx].event.pressed) {
                    if (((weak_mods | mods) & kc_mods) != kc_mods) {
                        register_mods(kc_mods);
                        wait_ms(QS_tap_code_delay);
                    } else {
                        add_mods(kc_mods);
                    }
                } else {
                    unregister_weak_mods(kc_mods);
                }

                g_vial_magic_keycode_override     = QK_MODS_GET_BASIC_KEYCODE(kc);
                deferred_key_record[ridx].keycode = KC_NO;
                action_exec(deferred_key_record[ridx].event);
                set_mods(mods);
                set_weak_mods(weak_mods);
            } else {
                g_vial_magic_keycode_override     = deferred_key_record[ridx].keycode;
                deferred_key_record[ridx].keycode = KC_NO;
                action_exec(deferred_key_record[ridx].event);
            }
        }
    }
}

void protocol_post_task_bmp(void) {
    if (encoder_modifier != 0 && timer_elapsed(encoder_modifier_pressed_ms) > ENCODER_MODIFIER_TIMEOUT_MS) {
        unregister_mods(encoder_modifier);
        encoder_modifier = 0;
    }
}