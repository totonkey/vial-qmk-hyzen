

#include  QMK_KEYBOARD_H
#include "kqb.h"

#include <string.h>

#include "quantum.h"

#include "state_controller.h"
#include "bmp_matrix.h"
#include "apidef.h"
#include "process_packet.h"
#include "report_descriptor_parser.h"
#include "report_parser.h"

extern bool    ch559_update_mode;
extern bool    ch559_start;
extern int     reset_counter;
extern uint8_t qt_cmd_buf[3];
extern bool    qt_cmd_new;
#define BOOTPIN KQB_PIN_BOOT

matrix_row_t* matrix_dest;
bool          matrix_has_changed;
matrix_row_t* matrix_mouse_dest;

static uint8_t  spis_tx_buf[4] = {0xfe, 0xff, 0xff, 0xff};
static uint8_t  spis_rx_buf[1024];
static int16_t  spis_receive_len = -1;
static uint8_t  spis_status = 0;
static uint16_t led_on_count = 0;

static void qt_spis_start(void) {
    spis_receive_len = -1;

    int res = BMPAPI->spis.start(spis_tx_buf, sizeof(spis_tx_buf), spis_rx_buf,
                                 sizeof(spis_rx_buf));

    if (res != 0) {
        xprintf("failed to start spis");
        spis_status = 0;
    } else {
        spis_status = 1;
    }
}

void qt_spis_callback(uint16_t receive_len) {
    spis_receive_len = receive_len;
}

void qt_spis_receive(uint16_t receive_len) {
    int idx = 0;

    // discard heading bytes
    while (spis_rx_buf[idx] == 0xc0) {
        idx++;
    }

    for (; idx < receive_len; idx++) {
        uart_recv_callback(spis_rx_buf[idx]);
    }

    if (receive_len > 4) {
        led_on_count = 10;
    }

    // static int cnt;
    // static bool send_flag = false;
    // if (++cnt % 100 == 0 || send_flag == true) {
    // if (receive_len > 4) {
        // xprintf("SPIS Receive: %d bytes\n", receive_len);
        // for (int idx = 0; idx < receive_len; idx++) {
        //     xprintf("%02X ", spis_rx_buf[idx]);
        // }
        // xprintf("\n");
    // }

    if (qt_cmd_new) {
        memcpy(spis_tx_buf + 1, qt_cmd_buf, sizeof(qt_cmd_buf));
        memset(qt_cmd_buf, 0xff, sizeof(qt_cmd_buf));
        qt_cmd_new = false;
        // xprintf("SPIS Send:%d,%d,%d,%d\n", spis_tx_buf[0], spis_tx_buf[1], spis_tx_buf[2], spis_tx_buf[3]);
    } else {
        memset(spis_tx_buf + 1, 0xff, sizeof(spis_tx_buf) - 1);
    }

    qt_spis_start();
}
void qt_spis_init(void) {
    bmp_api_spis_config_t config;
    config.mosi = KQB_PIN_MOSI;
    config.miso = KQB_PIN_MISO;
    config.sck = KQB_PIN_SCK;
    config.cs = KQB_PIN_CS;
    config.default_data = 0xff;
    config.over_read_data = 0xff;
    config.cs_pullup = 0;
    config.mode = 3;

    config.complete_callback = qt_spis_callback;

    BMPAPI->spis.init(&config);
    qt_spis_start();
}

void qt_matrix_init(void) {

    qt_spis_init();

    gpio_set_pin_output_push_pull(KQB_PIN_CHRST);
    // Assert reset
    gpio_write_pin_high(KQB_PIN_CHRST);

    gpio_set_pin_output_push_pull(KQB_PIN_CHBOOT);
    gpio_write_pin_low(KQB_PIN_CHBOOT);

    gpio_set_pin_input_high(BOOTPIN);

    gpio_set_pin_input_high(KQB_PIN_SW0);
    gpio_set_pin_input_high(KQB_PIN_SW1);
    gpio_set_pin_input_high(KQB_PIN_SW2);
    gpio_set_pin_output_push_pull(KQB_PIN_LED0);
    gpio_set_pin_output_push_pull(KQB_PIN_LED1);
    gpio_set_pin_output_push_pull(KQB_PIN_LED2);

    // Deassrt reset
    gpio_write_pin_low(KQB_PIN_CHRST);
    ch559_start = false;
}

uint32_t qt_get_device_row(void) { return MATRIX_ROWS_DEFAULT; }
uint32_t qt_get_device_col(void) { return MATRIX_COLS_DEFAULT; }
uint32_t qt_matrix_scan(matrix_row_t *matrix_raw) {
    if (spis_receive_len > 0) {
        qt_spis_receive(spis_receive_len);
    } else if (spis_receive_len != -1) {
        // restart spi if errors
        qt_spis_start();
    }

    if (led_on_count) {
        led_on_count--;
        gpio_write_pin_high(KQB_PIN_LED0);
        gpio_write_pin_high(KQB_PIN_LED2);
    } else {
        gpio_write_pin_low(KQB_PIN_LED0);
        gpio_write_pin_low(KQB_PIN_LED2);
    }

    if (readPin(BOOTPIN) == 0 && reset_counter < 0) {
        reset_counter = 10;
    }

    if (ch559_update_mode) {
        bmp_set_enable_task_interval_stretch(false);
        return 0;
    }

    static uint32_t reset_timer = 0;
    if ((!ch559_start) && (!ch559_update_mode) &&
        timer_elapsed32(reset_timer) > 1000) {
        uart_buf_init();
        int res = send_reset_cmd();
        if (res == 0) {
            reset_timer = timer_read32();
        }
        // do not return to receive startup response
    }

    matrix_dest        = matrix_raw;
    matrix_has_changed = 0;
    matrix_has_changed |= process_packet(matrix_raw);

    // override matrix by physical switches on quantizer b2
    if (ch559_start) {
        matrix_row_t row1 = matrix_raw[1];
        matrix_row_t row1_1 = (row1 & ~0x000007) | (gpio_read_pin(KQB_PIN_SW0) ? 0 : (1 << 0)) | (gpio_read_pin(KQB_PIN_SW1) ? 0 : (1 << 1)) | (gpio_read_pin(KQB_PIN_SW2) ? 0 : (1 << 2));
        matrix_has_changed |= row1 != row1_1;
        matrix_raw[1] = row1_1;
    }
    // uart_flush_rx_buffer();

    return matrix_has_changed ? 1 : 0;
}
static const bmp_matrix_func_t matrix_func = {
    qt_matrix_init, qt_get_device_row, qt_get_device_col, qt_matrix_scan};

const bmp_matrix_func_t *get_matrix_func_user(void) { return &matrix_func; }

uint8_t matrix_scan_impl(matrix_row_t *_matrix) {
    return qt_matrix_scan(_matrix);
}

bool mouse_send_flag = false;

__attribute__((weak)) void keyboard_report_hook(keyboard_parse_result_t const* report) {
    if (debug_enable) {
        uprintf("Keyboard report\n");
        for (int idx = 0; idx < sizeof(report->bits); idx++) {
            uprintf("%02X ", report->bits[idx]);
        }
        uprintf("\n");
    }

    for (uint8_t rowIdx = 0; rowIdx < MATRIX_ROWS - 1; rowIdx++) {
        matrix_dest[rowIdx + 1] = report->bits[rowIdx];
    }

    // copy modifier bits
    matrix_dest[0] = report->bits[28];
    matrix_dest[29] = 0;
}

__attribute__((weak)) void mouse_report_hook(mouse_parse_result_t const* report) {
    if (debug_enable) {
        uprintf("Mouse report\n");
        uprintf("b:%d ", report->button);
        uprintf("x:%d ", report->x);
        uprintf("y:%d ", report->y);
        uprintf("v:%d ", report->v);
        uprintf("h:%d ", report->h);
        uprintf("undef:%u\n", report->undefined);
    }

    mouse_send_flag = true;

    report_mouse_t mouse = pointing_device_get_report();

    mouse.buttons = report->button;

    mouse.x += report->x;
    mouse.y += report->y;
    mouse.v += report->v;
    mouse.h += report->h;

    pointing_device_set_report(mouse);
}

bool pointing_device_task(void) {
    if (mouse_send_flag) {
        bool send_report = pointing_device_send();
        mouse_send_flag = false;
        return send_report;
    }

    return false;
}

void vendor_report_parser(uint16_t usage_id, hid_report_member_t const* member, uint8_t const* data, uint8_t len) {
    // For Lenovo thinkpad keyboard(17ef:6047)
    // TODO: restriction by VID:PID
    if (usage_id == 0xFFA1) {
        mouse_parse_result_t mouse = {0};
        mouse.h                    = (data[0] & 0x80 ? 0xFF00 : 0) | data[0];
        mouse_report_hook(&mouse);
    }
}

__attribute__((weak)) void system_report_hook(uint16_t report) {
    host_system_send(report);
    wait_ms(TAP_CODE_DELAY);
    host_system_send(0);
}

__attribute__((weak)) void consumer_report_hook(uint16_t report) {
    host_consumer_send(report);
    wait_ms(TAP_CODE_DELAY);
    host_consumer_send(0);
}
