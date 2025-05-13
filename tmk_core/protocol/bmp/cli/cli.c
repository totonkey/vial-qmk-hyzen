#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "microshell/core/microshell.h"
#include "microshell/core/msconf.h"
#include "microshell/util/msopt.h"
#include "microshell/util/mscmd.h"
#include "microshell/util/ntlibc.h"

// QMK headers
#include "printf.h"
#include "debug.h"
#include "wait.h"

// BMP headers
#include "cli.h"
#include "apidef.h"
#include "bmp.h"
#include "bmp_host_driver.h"
#include "bmp_file.h"
#include "eeprom_bmp.h"
#include "xmodem.h"
#include "bmp_vial.h"
#include "bmp_flash.h"

void              cli_puts(const char *str);
static MICROSHELL microshell;
static MSCMD      mscmd;
static cli_app_t  cli_app = {NULL, NULL};

static MSCMD_USER_RESULT usrcmd_help(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_version(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_reset(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_advertise(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_disconnect(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_select_connection(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_bonding_information(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_delete_bonding(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_config(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_bootloader(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_debug_enable(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_dump_memory(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_eeprom_default(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_start_xmodem(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_enable_vial(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_factory_reset(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_battery_check(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);
static MSCMD_USER_RESULT usrcmd_change_conn_mode(MSOPT *msopt, MSCMD_USER_OBJECT usrobj);

static const MSCMD_COMMAND_TABLE table[] = {{"help", usrcmd_help, "Show this message"},
                                            {"version", usrcmd_version, "Firmware version"},
                                            {"reset", usrcmd_reset, "Reset system"},
                                            {"adv", usrcmd_advertise, "Start advertising"},
                                            {"dis", usrcmd_disconnect, "Disconnect BLE"},
                                            {"sel", usrcmd_select_connection, "Select USB/BLE"},
                                            {"show", usrcmd_bonding_information, "Show bonded devices"},
                                            {"del", usrcmd_delete_bonding, "Delete bonding information"},
                                            {"config", usrcmd_config, "Show current config"},
                                            {"dfu", usrcmd_bootloader, "Jump to bootloader"},
                                            {"debug", usrcmd_debug_enable, "Debug print setting"},
                                            {"dump", usrcmd_dump_memory, "Dump memory"},
                                            {"default", usrcmd_eeprom_default, "Save/Load default eeprom data"},
                                            {"xmodem", usrcmd_start_xmodem, "Start XMODEM"},
                                            {"vial", usrcmd_enable_vial, "Enable/Disable vial"},
                                            {"factory_reset", usrcmd_factory_reset, "Factory reset"},
                                            {"battery", usrcmd_battery_check, "Battery check"},
                                            {"mode", usrcmd_change_conn_mode, "Change connection mode"},
#ifdef USER_DEFINED_MSCMD
                                            USER_DEFINED_MSCMD
#endif
};

void cli_puts(const char *str) {
    BMPAPI->usb.serial_puts((const uint8_t *)str, strlen(str));
}

void cli_init(void) {
    microshell_init(&microshell, BMPAPI->usb.serial_putc, BMPAPI->usb.serial_byte_to_read, BMPAPI->usb.serial_getc, NULL);
    mscmd_init(&mscmd, table, sizeof(table) / sizeof(table[0]), NULL);
}

void set_cli_app(cli_app_t *const p_cli_app) {
    cli_app.func = p_cli_app->func;
    cli_app.data = p_cli_app->data;
}

void cli_exec(void) {
    static char  buf[MSCONF_MAX_INPUT_LENGTH];
    static char *cur       = buf;
    static bool  show_info = false;

    if (cli_app.func != NULL) {
        cli_app.func(cli_app.data);
        return;
    }

    if (!show_info) {
        if (is_safe_mode()) {
            printf("bmp@safemode>");
        } else {
            printf("bmp@%s>", BMPAPI->app.get_config()->device_info.name);
        }
        show_info = true;
    }

    cur = microshell_getline(&microshell, buf, cur, MSCONF_MAX_INPUT_LENGTH - (cur - buf));
    if (cur == buf + MSCONF_MAX_INPUT_LENGTH) {
        MSCMD_USER_RESULT r;
        mscmd_execute(&mscmd, buf, &r);
        memset(buf, 0, MSCONF_MAX_INPUT_LENGTH);
        cur       = buf;
        show_info = false;
    }
}

static MSCMD_USER_RESULT usrcmd_help(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    for (int i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        cli_puts(table[i].argv0);
        microshell.uart_putc('\t');
        cli_puts(table[i].desc);
        cli_puts("\r\n");
    }
    return 0;
}

static MSCMD_USER_RESULT usrcmd_version(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    printf("%s\n\n", BMPAPI->get_bootloader_info());
    printf("%s\n", bmp_get_version_info());

    return 0;
}

static MSCMD_USER_RESULT usrcmd_reset(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    uint32_t safemode_flag = 0;
    char     arg[16];

    if (msopt->argc >= 2) {
        msopt_get_argv(msopt, 1, arg, sizeof(arg));
        if (strcmp(arg, "safe") == 0) {
            safemode_flag = 1;
        }
    }

    BMPAPI->app.reset(safemode_flag);
    return 0;
}

static MSCMD_USER_RESULT usrcmd_advertise(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    char arg[4];

    if (msopt->argc >= 2) {
        msopt_get_argv(msopt, 1, arg, sizeof(arg));
        uint8_t id = (uint8_t)atoi(arg);
        BMPAPI->ble.advertise(id);
    } else {
        BMPAPI->ble.advertise(255);
    }

    return 0;
}

static MSCMD_USER_RESULT usrcmd_disconnect(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    if (msopt->argc >= 2) {
        print("disconnect all\n");
        BMPAPI->ble.disconnect(1);
    } else {
        print("disconnect device\n");
        BMPAPI->ble.disconnect(0);
    }
    return 0;
}

static MSCMD_USER_RESULT usrcmd_select_connection(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    char arg[16];

    xprintf("Usage: select <usb/ble>\n");

    if (msopt->argc >= 2) {
        msopt_get_argv(msopt, 1, arg, sizeof(arg));
        if (strcmp(arg, "ble") == 0) {
            printf("Switch to BLE\n");
            select_ble();
        } else if (strcmp(arg, "usb") == 0) {
            printf("Switch to USB\n");
            select_usb();
        }
    }

    return 0;
}

static MSCMD_USER_RESULT usrcmd_bonding_information(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    bmp_api_bonding_info_t peers[8];
    uint32_t               peer_cnt     = sizeof(peers) / sizeof(peers[0]);
    const uint8_t          empty_name[] = "";

    BMPAPI->ble.get_bonding_info(peers, &peer_cnt);

    bmp_api_config_t const *const config = BMPAPI->app.get_config();

    cli_puts("{\"bonding\":[\n");

    for (int i = 0; i < peer_cnt; i++) {
        // print mac address
        printf("{\"id\":%2d, \"type\":\"%s\",\"name\":\"%s\",\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}", //
               peers[i].id,
               (peers[i].role == 2)            ? "Slave "
               : (config->mode == SPLIT_SLAVE) ? "Master"
                                               : "Device",                                  //
               ((peers[i].role != 2 && config->mode != SPLIT_SLAVE) ? peers[i].name : empty_name), //
               peers[i].addr[5], peers[i].addr[4], peers[i].addr[3], peers[i].addr[2], peers[i].addr[1], peers[i].addr[0]);

        if (i < peer_cnt - 1) {
            microshell.uart_putc(',');
        }

        microshell.uart_putc('\n');
    }
    cli_puts("]}");
    microshell.uart_putc('\0');
    return 0;
}

static MSCMD_USER_RESULT usrcmd_delete_bonding(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    char arg[4];
    if (msopt->argc >= 2) {
        msopt_get_argv(msopt, 1, arg, sizeof(arg));
        if (arg[0] >= '0' && arg[0] <= '9') {
            BMPAPI->ble.delete_bond(arg[0] - '0');
        }
    } else {
        BMPAPI->ble.delete_bond(255);
    }
    return 0;
}

static MSCMD_USER_RESULT usrcmd_config(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    const bmp_api_config_t *config = BMPAPI->app.get_config();
    printf("Mode: %d\n"
           "Is Left: %d\n"
           "Debounce: %d\n"
           "Auto sleep[min]: %d\n"
           "Interval(Peripheral)[ms]: %d\n"
           "Slave letency(Peripheral): %d\n"
           "Interval(Central)[ms]: %d\n",
           config->mode, //
           config->matrix.is_left_hand,
           config->matrix.debounce,                //
           config->reserved[2] * 10,               //
           config->param_peripheral.min_interval,  //
           config->param_peripheral.slave_latency, //
           config->param_central.min_interval);
    return 0;
}

static MSCMD_USER_RESULT usrcmd_bootloader(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    BMPAPI->bootloader_jump();
    return 0;
}

static MSCMD_USER_RESULT usrcmd_debug_enable(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    char arg[16];
    if (msopt->argc >= 2) {
        msopt_get_argv(msopt, 1, arg, sizeof(arg));
        if (strcmp(arg, "on") == 0) {
            debug_enable = true;
        } else if (strcmp(arg, "keyboard") == 0) {
            debug_enable   = true;
            debug_keyboard = true;
            debug_matrix   = true;
        } else if (strcmp(arg, "mouse") == 0) {
            debug_enable = true;
            debug_mouse  = true;
        } else if (strcmp(arg, "all") == 0) {
            debug_enable   = true;
            debug_keyboard = true;
            debug_mouse    = true;
        } else if (strcmp(arg, "off") == 0) {
            debug_enable   = false;
            debug_keyboard = false;
            debug_mouse    = false;
        }
    } else {
        printf("Set debug option [on|off]\r\n");
    }
    return 0;
}

static MSCMD_USER_RESULT usrcmd_dump_memory(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    char arg[16];
    if (msopt->argc >= 2) {
        msopt_get_argv(msopt, 1, arg, sizeof(arg));
        uint32_t addr = (uint32_t)strtol(arg, NULL, 16);
        uint8_t* p = (uint8_t*)addr;
        char* pstr = (char*)addr;
        if ((addr > 0x100000) && (addr < 0x20000000 || addr > 0x20040000))
        {
            printf("invalid addr\r\n");
            return 0;
        }
        for(int i=0; i<16; i++){
            for(int j=0; j<8; j++){
                printf("%02x ", *p++);
            }
            printf("  ");
            for(int j=0; j<8; j++){
                if (*pstr >= 0x20 && *pstr <= 0x7e) {
                    printf("%c", *pstr);
                } else {
                    printf(".");
                }
                pstr++;
            }
            printf("\r\n");
        }
    }
    return 0;
}

static MSCMD_USER_RESULT usrcmd_eeprom_default(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    char arg[16];
    if (msopt->argc >= 2) {
        msopt_get_argv(msopt, 1, arg, sizeof(arg));
        if (strcmp(arg, "save") == 0) {
            eeprom_bmp_save_default();
        } else if (strcmp(arg, "load") == 0) {
            eeprom_bmp_load_default();
        } else if (strcmp(arg, "erase") == 0) {
            eeprom_bmp_erase_default();
        }
    }

    return 0;
}

static void xmodem_complete_callback(void) {
    printf("xmodem complete\n");
    cli_app.func = NULL;
}

static void packet_receive_callback(xmodem_packet_t *packet) {
    static bmp_file_t xmodem_file_type;

    if (packet->block_no == 1) {
        xmodem_file_type = detect_file_type(packet->data, sizeof(packet->data));
    }

    bmp_file_res_t res = write_bmp_file(xmodem_file_type, packet->data,                //
                                        sizeof(packet->data) * (packet->block_no - 1), //
                                        sizeof(packet->data));
    if (res != BMP_FILE_CONTINUE) {
        xmodem_file_type = BMP_FILE_NONE;
    }
}

static MSCMD_USER_RESULT usrcmd_start_xmodem(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    xmodem_init(xmodem_complete_callback, packet_receive_callback);
    cli_app.func = xmodem_task;

    return 0;
}

static MSCMD_USER_RESULT usrcmd_enable_vial(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    char arg[16];
    if (msopt->argc >= 2) {
        msopt_get_argv(msopt, 1, arg, sizeof(arg));
        if (strcmp(arg, "on") == 0) {
            bmp_set_vial_enable_flag(true);
            printf("Enable vial protocol\n");
            return 0;
        } else if (strcmp(arg, "off") == 0) {
            bmp_set_vial_enable_flag(false);
            printf("Disable vial protocol (Support VIA protocol 0x0C)\n");
            return 0;
        }
    }
    printf("Usage: vial [on/off]\n");
    return 0;
}

static MSCMD_USER_RESULT usrcmd_factory_reset(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    printf("Erasing all setting files...\n");
    for (int page = 0; page < FLASH_PAGE_ID_END; page++) {
        int res = flash_erase_page(page);
        printf("Erase page%d: %s\n", page, res == 0 ? "OK" : "NG");
    }
    BMPAPI->ble.delete_bond(255);
    printf("Erase all pairing info\n");

    return 0;
}

static MSCMD_USER_RESULT usrcmd_battery_check(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    printf("%d mv\n", BMPAPI->app.get_vcc_mv(0));
    if (bmp_config->mode == SPLIT_MASTER) {
        printf("slave: %4dmV\n", BMPAPI->app.get_vcc_mv(1));
    }

    return 0;
}

static MSCMD_USER_RESULT usrcmd_change_conn_mode(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    char arg[16];
    if (msopt->argc >= 2) {
        msopt_get_argv(msopt, 1, arg, sizeof(arg));
        if (strcmp(arg, "web") == 0) {
            BMPAPI->ble.change_conn_mode(true);
            printf("Change to web bt mode\n");
            return 0;
        }
    }

    BMPAPI->ble.change_conn_mode(false);
    printf("Change to normal mode\n");
    return 0;
}