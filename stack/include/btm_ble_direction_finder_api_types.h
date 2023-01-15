/******************************************************************************
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 *******************************************************************************/

#ifndef BTM_BLE_DIRECTION_FINDER_API_TYPES_H
#define BTM_BLE_DIRECTION_FINDER_API_TYPES_H

#include "osi/include/alarm.h"
#include <base/callback_forward.h>
#include <base/callback.h>
#include <hardware/bt_common_types.h>

using tBTM_BLE_AOA_CONN_IQ_RPT_EVT_CB = base::Callback<void(RawAddress, uint8_t*, uint16_t)>;

/* LE Read Antenna Info */
typedef struct {
  uint8_t status;
  uint8_t supp_switching_sampling_rates;
  uint8_t num_antennae;
  uint8_t max_switching_pattern_len;
  uint8_t max_cte_len;
} tBTM_BLE_READ_ANTENNA_INFO;

typedef struct {
  tBTM_BLE_AOA_CONN_IQ_RPT_EVT_CB aoa_conn_iq_rpt_evt_cb;
} tBTM_BLE_AOA_HCI_CMD_CB;

#endif //BTM_BLE_DIRECTION_FINDER_API_TYPES_H
