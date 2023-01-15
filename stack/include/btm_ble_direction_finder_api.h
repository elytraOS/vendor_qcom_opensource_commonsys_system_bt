/******************************************************************************
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 *******************************************************************************/
#ifdef DIR_FINDING_FEATURE

#ifndef BTM_BLE_DIRECTION_FINDER_API_H
#define BTM_BLE_DIRECTION_FINDER_API_H

#include "stack/include/btm_ble_direction_finder_api_types.h"

/*******************************************************************************
 *
 * Function         BTM_BleSetConnCTERxParams
 *
 * Description      This function is used to set BLE connection CTE Receive params
 *                  for a given BLE connection
 *
 *
 *
 * Returns          None
 *
 ******************************************************************************/
extern void BTM_BleSetConnCTERxParams(const RawAddress& bd_addr,
                                      uint8_t sampling_enable,
                                      uint8_t slot_durations,
                                      uint8_t switching_pattern_len,
                                      std::vector<uint8_t> antenna_ids,
                                      base::Callback<void(uint8_t*, uint16_t)> cb);

/*******************************************************************************
 *
 * Function         BTM_BleSetConnCTETxParams
 *
 * Description      This functin is used to set BLE connection CTE Transmit params
 *                  for a specified BLE connection
 *
 *
 * Returns          None
 *
 ******************************************************************************/
extern void BTM_BleSetConnCTETxParams(const RawAddress& bd_addr,
                                      uint8_t cte_types,
                                      uint8_t switching_pattern_len,
                                      std::vector<uint8_t> antenna_ids,
                                      base::Callback<void(uint8_t*, uint16_t)> cb);

/*******************************************************************************
 *
 * Function         BTM_BleSetConnCTEReqEnable
 *
 * Description      This function is used to set BLE Connection CTE Request Enable
 *                  for a specified BLE connection
 *
 *
 * Returns          None
 *
 ******************************************************************************/
extern void BTM_BleSetConnCTEReqEnable(const RawAddress& bd_addr, uint8_t enable,
                                       uint16_t cte_req_int, uint8_t req_cte_len,
                                       uint8_t req_cte_type,
                                       base::Callback<void(uint8_t*, uint16_t)> cb,
                                       base::Callback<void(RawAddress, uint8_t*, uint16_t)> aoa_conn_iq_cb);

/*******************************************************************************
 *
 * Function         BTM_BleSetConnCTERspEnable
 *
 * Description      This function is used to set BLE connection CTE Response Enable
 *                  for a specified BLE connection
 *
 *
 * Returns          None
 *
 ******************************************************************************/
extern void BTM_BleSetConnCTERspEnable(const RawAddress& bd_addr, uint8_t enable,
                                       base::Callback<void(uint8_t*, uint16_t)> cb);

/*******************************************************************************
 *
 * Function         BTM_BleReadAntennaInfo
 *
 * Description      This function is used to read BLE Antenna information
 *                  from local controller
 *
 *
 * Returns          None.
 *
 ******************************************************************************/
extern void BTM_BleReadAntennaInfo();

/*******************************************************************************
 *
 * Function         btm_ble_aoa_conn_iq_rpt_evt
 *
 * Description      This function is called when BT stack receives HCI LE
 *                  Connection IQ Report event.
 *                  This message is received from the HCI.
 *
 * Returns          void
 *
 ******************************************************************************/
extern void btm_ble_aoa_conn_iq_rpt_evt(uint16_t conn_handle, uint8_t* p_data, uint16_t evt_len);

#endif//BTM_BLE_DIRECTION_FINDER_API_H
#endif //DIR_FINDING_FEATURE