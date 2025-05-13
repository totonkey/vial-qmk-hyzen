// Copyright 2023 sekigon-gonnoc
/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "quantum_keycodes.h"

enum bmp_custom_keycodes {
    SEL_BLE = QK_KB_0, /* Enable BLE and disable USB           */
    SEL_USB,          /* Enable USB and disable BLE           */
    ADV_ID0,          /* Start advertising to PeerID 0        */
    ADV_ID1,          /* Start advertising to PeerID 1        */
    ADV_ID2,          /* Start advertising to PeerID 2        */
    ADV_ID3,          /* Start advertising to PeerID 3        */
    ADV_ID4,          /* Start advertising to PeerID 4        */
    ADV_ID5,          /* Start advertising to PeerID 5        */
    ADV_ID6,          /* Start advertising to PeerID 6        */
    ADV_ID7,          /* Start advertising to PeerID 7        */
    AD_WO_L,          /* Start advertising without whitelist  */
    DEL_ID0,          /* Delete bonding of PeerID 0           */
    DEL_ID1,          /* Delete bonding of PeerID 1           */
    DEL_ID2,          /* Delete bonding of PeerID 2           */
    DEL_ID3,          /* Delete bonding of PeerID 3           */
    DEL_ID4,          /* Delete bonding of PeerID 4           */
    DEL_ID5,          /* Delete bonding of PeerID 5           */
    DEL_ID6,          /* Delete bonding of PeerID 6           */
    DEL_ID7,          /* Delete bonding of PeerID 7           */
    DELBNDS,          /* Delete all bonding                   */
    BATT_LV,          /* Display battery level in milli volts */
    ENT_SLP,          /* Enter deep sleep mode */
    DISABLE_KEY_OS_OVERRIDE, /* Disable key override for JP/US OS */
    ENABLE_US_KEY_ON_JP_OS_OVERRIDE, /* Enable US keyboard on JP OS */
    ENABLE_JP_KEY_ON_US_OS_OVERRIDE, /* Enable JP keyboard on US OS */
    EN_WEBBT,         /* Enable Web bluetooth connection */
    DIS_WEBBT,        /* Enable Web bluetooth connection */
    BMP_SAFE_RANGE
};
