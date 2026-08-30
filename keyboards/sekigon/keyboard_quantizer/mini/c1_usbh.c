// Copyright 2023 sekigon-gonnoc
// SPDX-License-Identifier: GPL-2.0-or-later

#include "c1.h"
#include "tusb.h"
#include "pio_usb.h"
#include "pio_usb_ll.h"
#include "hardware/sync.h"
#include "host/hcd.h"

extern uint64_t time_us_64(void);

// dummy implementation
void alarm_pool_add_repeating_timer_us(void) {}
void alarm_pool_create(void) {}

// Temporary validation: perform one software-only downstream USB
// re-enumeration two seconds after the first device mount.
#define KQM_USB_HOST_RECONNECT_TEST_DELAY_US 2000000ULL

static bool     reconnect_test_armed;
static bool     reconnect_test_done;
static uint64_t reconnect_test_started_at;

static bool c1_usb_host_has_mounted_device(void) {
    for (uint8_t dev_addr = 1; dev_addr <= CFG_TUH_DEVICE_MAX; dev_addr++) {
        if (tuh_mounted(dev_addr)) {
            return true;
        }
    }

    return false;
}

static void c1_soft_reconnect_usb_host(void) {
    root_port_t *root = PIO_USB_ROOT_PORT(0);

    if (!root->connected) {
        return;
    }

    // Queue a normal TinyUSB removal event, then let the next USB frame
    // detect the still-connected physical device and enumerate it again.
    root->connected = false;
    root->suspended = true;
    hcd_event_device_remove(1, false);
}

static void c1_usb_host_reconnect_test_task(void) {
    if (reconnect_test_done) {
        return;
    }

    if (!c1_usb_host_has_mounted_device()) {
        reconnect_test_armed = false;
        return;
    }

    if (!reconnect_test_armed) {
        reconnect_test_armed      = true;
        reconnect_test_started_at = time_us_64();
        return;
    }

    if (time_us_64() - reconnect_test_started_at <
        KQM_USB_HOST_RECONNECT_TEST_DELAY_US) {
        return;
    }

    reconnect_test_done = true;
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
    tuh_task();
    kqm_hid_receive_task();
    c1_usb_host_reconnect_test_task();
}