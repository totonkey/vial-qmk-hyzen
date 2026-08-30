// Copyright 2023 sekigon-gonnoc
// SPDX-License-Identifier: GPL-2.0-or-later

#include "c1.h"
#include "tusb.h"
#include "pio_usb.h"
#include "pio_usb_ll.h"
#include "hardware/sync.h"
#include "host/hcd.h"

// dummy implementation
void alarm_pool_add_repeating_timer_us(void) {}
void alarm_pool_create(void) {}

static volatile bool reconnect_requested;

void c1_request_usb_host_reconnect(void) {
    reconnect_requested = true;
}

static void c1_soft_reconnect_usb_host(void) {
    root_port_t *root = PIO_USB_ROOT_PORT(0);

    if (!root->connected) {
        return;
    }

    root->connected = false;
    root->suspended = true;
    hcd_event_device_remove(1, false);
}

static bool c1_usb_host_has_mounted_device(void) {
    for (uint8_t dev_addr = 1; dev_addr <= CFG_TUH_DEVICE_MAX; dev_addr++) {
        if (tuh_mounted(dev_addr)) {
            return true;
        }
    }

    return false;
}

static void c1_usb_host_reconnect_task(void) {
    if (!reconnect_requested) {
        return;
    }

    // Consume the request once. Do not carry a startup wake request forward
    // until a keyboard is connected later.
    reconnect_requested = false;

    // QMK can call the wake hook during startup. Reconnect only when TinyUSB
    // has already completed mounting a downstream USB device.
    if (!c1_usb_host_has_mounted_device()) {
        return;
    }

    c1_soft_reconnect_usb_host();
}

// Initialize USB host stack on core1
void c1_usbh(void) {
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp                  = 4;
    pio_cfg.extra_error_retry_count = 10;
    pio_cfg.skip_alarm_pool         = true;
    tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

    tuh_init(1);
    c1_start_timer();
}

// USB host stack main task
void c1_main_task(void) {
    c1_usb_host_reconnect_task();
    tuh_task();
    kqm_hid_receive_task();
}