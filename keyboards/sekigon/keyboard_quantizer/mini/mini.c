// Copyright 2023 sekigon-gonnoc
// SPDX-License-Identifier: GPL-2.0-or-later

#include "keyboard.h"
#include "pico/stdlib.h"
#include "bootloader.h"
#include "debug.h"
#include "c1.h"

void kqm_set_suspend_led(bool suspended);

static bool c1_suspended;

void keyboard_pre_init_kb(void) {
    set_sys_clock_khz(120000, true);
    keyboard_pre_init_user();
}

__attribute__((weak)) void virtser_recv(uint8_t c) {
    if (c == 'b') {
        bootloader_jump();
    } else if (c == 'd') {
        debug_enable = !debug_enable;
        uprintf("Debug %s\n", debug_enable ? "enabled" : "disabled");
    }
}

bool backing_store_lock(void) {
    c1_after_flash_operation();
    return true;
}

bool backing_store_unlock(void) {
    c1_before_flash_operation();
    return true;
}

void suspend_power_down_kb(void) {
    // QMK can call this repeatedly while USB is suspended.
    if (!c1_suspended) {
        c1_before_flash_operation();
        c1_suspended = true;
        kqm_set_suspend_led(true);
    }
}

void suspend_wakeup_init_kb(void) {
    if (c1_suspended) {
        c1_after_flash_operation();
        c1_suspended = false;
        kqm_set_suspend_led(false);
    }
}