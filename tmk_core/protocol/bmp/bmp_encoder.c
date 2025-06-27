// Copyright 2023 sekigon-gonnoc
/* SPDX-License-Identifier: GPL-2.0-or-later */

// QMK
#include "encoder.h"
#include "keyboard.h"
#include "print.h"

// BMP
#include "apidef.h"
#include "bmp.h"

#ifndef ENCODER_RESOLUTION
#    define ENCODER_RESOLUTION 4
#endif

#ifndef ENCODER_SCANTIME_MS
#    define ENCODER_SCANTIME_MS 2
#endif

#define ENCODER_CLOCKWISE true
#define ENCODER_COUNTER_CLOCKWISE false
#define NUM_ENCODERS_MAX 8

static int    encoder_count                   = 0;
static int8_t encoder_value[NUM_ENCODERS_MAX] = {0};

static void encoder_exec_mapping(uint8_t index, bool clockwise);

__attribute__((weak)) bool encoder_update_user(uint8_t index, bool clockwise) {
    return true;
}

__attribute__((weak)) bool encoder_update_kb(uint8_t index, bool clockwise) {
    bool res = encoder_update_user(index, clockwise);
    if (res) {
        encoder_exec_mapping(index, clockwise);
    }
    return res;
}

static void encoder_exec_mapping(uint8_t index, bool clockwise) {
    // The delays below cater for Windows and its wonderful requirements.
    action_exec(clockwise ? MAKE_ENCODER_CW_EVENT(index, true) : MAKE_ENCODER_CCW_EVENT(index, true));
#if ENCODER_MAP_KEY_DELAY > 0
    wait_ms(ENCODER_MAP_KEY_DELAY);
#endif // ENCODER_MAP_KEY_DELAY > 0

    action_exec(clockwise ? MAKE_ENCODER_CW_EVENT(index, false) : MAKE_ENCODER_CCW_EVENT(index, false));
#if ENCODER_MAP_KEY_DELAY > 0
    wait_ms(ENCODER_MAP_KEY_DELAY);
#endif // ENCODER_MAP_KEY_DELAY > 0
}

void encoder_init(void) {
    if (bmp_config->mode == SPLIT_SLAVE) {
        return;
    }

    for (int i = 0; i < NUM_ENCODERS_MAX; i++) {
        if (bmp_config->encoder.pin_a[i] == 0 || bmp_config->encoder.pin_b[i] == 0) {
            break;
        }
        encoder_count++;
    }

    if (encoder_count > 0) {
        BMPAPI->encoder.init(ENCODER_SCANTIME_MS, bmp_config->encoder.pin_a, bmp_config->encoder.pin_b, encoder_count);
    }
}

bool encoder_task(void) {
    bool changed = false;

    for (int index = 0; index < encoder_count; index++) {
        int32_t encoder_diff = BMPAPI->encoder.get_count(index);
        encoder_value[index] += encoder_diff;

        int8_t resolution = bmp_config->encoder.resolution[index];
        resolution        = resolution > 0 ? resolution : ENCODER_RESOLUTION;

        if (encoder_value[index] >= resolution) {
            encoder_update_kb(index, ENCODER_COUNTER_CLOCKWISE);
            changed = true;
        } else if (encoder_value[index] <= -resolution) {
            encoder_update_kb(index, ENCODER_CLOCKWISE);
            changed = true;
        }

        encoder_value[index] %= resolution;
    }

    return changed;
}
