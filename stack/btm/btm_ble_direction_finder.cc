/******************************************************************************
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 *******************************************************************************/
#ifdef DIR_FINDING_FEATURE

#include <base/callback_forward.h>
#include <hardware/bt_common_types.h>
#include <vector>
#include <map>

#define LOG_TAG "bt_btm_ble_directionfinder"

#include "bt_target.h"

#include <base/bind.h>
#include <string.h>

#include "bt_types.h"
#include "bt_utils.h"
#include "btm_ble_direction_finder_api_types.h"
#include "btm_int.h"
#include "btu.h"
#include "device/include/controller.h"
#include "gap_api.h"
#include "gatt_api.h"
#include "hcimsgs.h"
#include "log/log.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "stack/gatt/connection_manager.h"
#include "btif/include/btif_config.h"

//HCI Command or Event callbacks
tBTM_BLE_AOA_HCI_CMD_CB hci_aoa_cmd_cmpl;


/*******************************************************************************
 *
 * Function         BTM_BleSetConnCTERxParams
 *
 * Description      To set BLE connection CTE Receive params for a given
 *                  BLE connection
 *
 *
 * Returns          None
 *
 ******************************************************************************/
void BTM_BleSetConnCTERxParams(const RawAddress& bd_addr, uint8_t sampling_enable,
        uint8_t slot_durations, uint8_t switching_pattern_len,
        std::vector<uint8_t> antenna_ids, base::Callback<void(uint8_t*, uint16_t)> cb) {
  tACL_CONN* p_acl = btm_bda_to_acl(bd_addr, BT_TRANSPORT_LE);

  if (!controller_get_interface()->supports_ble_aoa()) {
    BTM_TRACE_ERROR("%s failed, request not supported in local controller!", __func__);
    return;
  }

  if (p_acl == NULL) {
    BTM_TRACE_ERROR("%s: Wrong mode: no LE link exist or LE not supported",
                    __func__);
    return;
  }

  BTM_TRACE_DEBUG(
        "%s: sampling_enable = %d, slot_durations = %d, switching_pattern_len = %d",
        __func__, sampling_enable, slot_durations, switching_pattern_len);

  uint16_t conn_handle = p_acl->hci_handle;

  if (!HCI_LE_CONN_CTE_REQ_SUPPORTED(p_acl->peer_le_features) ||
      !HCI_LE_CONN_CTE_RSP_SUPPORTED(p_acl->peer_le_features) ||
      !HCI_LE_CONN_ANTENNA_SWITCH_AOA_SUPPORTED(p_acl->peer_le_features) ||
      !HCI_LE_CONN_RX_CTE_SUPPORTED(p_acl->peer_le_features)) {
    BTM_TRACE_ERROR("%s failed, peer does not support request", __func__);
    return;
  }

  btsnd_hcic_ble_set_conn_cte_rx_params(conn_handle, sampling_enable,
                                        slot_durations, switching_pattern_len,
                                        antenna_ids, cb);
}

/*******************************************************************************
 *
 * Function         BTM_BleSetConnCTETxParams
 *
 * Description      To set BLE connection CTE Transmit params for a specified
 *                  BLE connection
 *
 *
 * Returns          None
 *
 ******************************************************************************/
void BTM_BleSetConnCTETxParams(const RawAddress& bd_addr, uint8_t cte_types,
    uint8_t switching_pattern_len, std::vector<uint8_t> antenna_ids,
    base::Callback<void(uint8_t*, uint16_t)> cb) {
  tACL_CONN* p_acl = btm_bda_to_acl(bd_addr, BT_TRANSPORT_LE);

  if (!controller_get_interface()->supports_ble_aoa()) {
     BTM_TRACE_ERROR("%s failed, request not supported in local controller!", __func__);
     return;
  }

  if (p_acl == NULL) {
    BTM_TRACE_ERROR("%s: Wrong mode: no LE link exist or LE not supported",
                    __func__);
    return;
  }

  BTM_TRACE_DEBUG(
      "%s: cte_types = %d, switching_pattern_len = %d,",
      __func__, cte_types, switching_pattern_len);

  uint16_t conn_handle = p_acl->hci_handle;

  if (!HCI_LE_CONN_CTE_REQ_SUPPORTED(p_acl->peer_le_features) ||
      !HCI_LE_CONN_CTE_RSP_SUPPORTED(p_acl->peer_le_features) ||
      !HCI_LE_CONN_ANTENNA_SWITCH_AOA_SUPPORTED(p_acl->peer_le_features) ||
      !HCI_LE_CONN_RX_CTE_SUPPORTED(p_acl->peer_le_features)) {
    BTM_TRACE_ERROR("%s failed, peer does not support request", __func__);
    return;
  }

  btsnd_hcic_ble_set_conn_cte_tx_params(conn_handle, cte_types,
                                        switching_pattern_len,
                                        antenna_ids, cb);
}

/*******************************************************************************
 *
 * Function         BTM_BleSetConnCTEReqEnable
 *
 * Description      To set BLE Connection CTE Request Enable for a specified
 *                  BLE connection
 *
 *
 * Returns          None
 *
 ******************************************************************************/
void BTM_BleSetConnCTEReqEnable(const RawAddress& bd_addr, uint8_t enable,
    uint16_t cte_req_int, uint8_t req_cte_len, uint8_t req_cte_type,
    base::Callback<void(uint8_t*, uint16_t)> cb,
    base::Callback<void(RawAddress, uint8_t*, uint16_t)> aoa_conn_iq_cb) {
  tACL_CONN* p_acl = btm_bda_to_acl(bd_addr, BT_TRANSPORT_LE);

  if (!controller_get_interface()->supports_ble_aoa()) {
    BTM_TRACE_ERROR("%s failed, request not supported in local controller!", __func__);
    return;
  }

  if (p_acl == NULL) {
    BTM_TRACE_ERROR("%s: Wrong mode: no LE link exist or LE not supported",
                    __func__);
    return;
  }

  BTM_TRACE_DEBUG(
      "%s: enable = %d, cte_req_int = %d, req_cte_len = %d, req_cte_type "
      "= %d",
      __func__, enable, cte_req_int, req_cte_len, req_cte_type);

  uint16_t conn_handle = p_acl->hci_handle;

  if (!HCI_LE_CONN_CTE_REQ_SUPPORTED(p_acl->peer_le_features) ||
      !HCI_LE_CONN_CTE_RSP_SUPPORTED(p_acl->peer_le_features) ||
      !HCI_LE_CONN_ANTENNA_SWITCH_AOA_SUPPORTED(p_acl->peer_le_features) ||
      !HCI_LE_CONN_RX_CTE_SUPPORTED(p_acl->peer_le_features)) {
    BTM_TRACE_ERROR("%s failed, peer does not support request", __func__);
    return;
  }

  hci_aoa_cmd_cmpl.aoa_conn_iq_rpt_evt_cb = aoa_conn_iq_cb;

  btsnd_hcic_ble_set_conn_cte_req_enable(conn_handle, enable,
                                         cte_req_int, req_cte_len,
                                         req_cte_type, cb);
}

/*******************************************************************************
 *
 * Function         BTM_BleSetConnCTERspEnable
 *
 * Description      To set BLE connection CTE Response Enable for a specified
 *                  BLE connection
 *
 *
 * Returns          None
 *
 ******************************************************************************/
void BTM_BleSetConnCTERspEnable(const RawAddress& bd_addr, uint8_t enable,
                                base::Callback<void(uint8_t*, uint16_t)> cb) {
  tACL_CONN* p_acl = btm_bda_to_acl(bd_addr, BT_TRANSPORT_LE);

  if (!controller_get_interface()->supports_ble_aoa()) {
    BTM_TRACE_ERROR("%s failed, request not supported in local controller!", __func__);
    return;
  }

  if (p_acl == NULL) {
    BTM_TRACE_ERROR("%s: Wrong mode: no LE link exist or LE not supported",
                    __func__);
    return;
  }

  BTM_TRACE_DEBUG("%s: enable = %d", __func__, enable);

  uint16_t conn_handle = p_acl->hci_handle;

  if (!HCI_LE_CONN_CTE_REQ_SUPPORTED(p_acl->peer_le_features) ||
      !HCI_LE_CONN_CTE_RSP_SUPPORTED(p_acl->peer_le_features) ||
      !HCI_LE_CONN_ANTENNA_SWITCH_AOA_SUPPORTED(p_acl->peer_le_features) ||
      !HCI_LE_CONN_RX_CTE_SUPPORTED(p_acl->peer_le_features)) {
    BTM_TRACE_ERROR("%s failed, peer does not support request", __func__);
    return;
  }

  btsnd_hcic_ble_set_conn_cte_rsp_enable(conn_handle, enable, cb);
}

/*******************************************************************************
 *
 * Function         BTM_BleReadAntennaInfo
 *
 * Description      To read BLE Antenna information from local controller
 *
 *
 * Returns          None.
 *
 ******************************************************************************/
void BTM_BleReadAntennaInfo() {
  BTM_TRACE_DEBUG("%s:",__func__);

  if (!controller_get_interface()->supports_ble_aoa()) {
     BTM_TRACE_ERROR("%s failed, request not supported in local controller!", __func__);
     return;
  }

  btsnd_hcic_ble_read_antenna_info();
}

/*******************************************************************************
 *
 * Function         btm_ble_read_antenna_info_complete
 *
 * Description      This function is the callback function for HCI LE Read
 *                  Antenna Information command.
 *                  This message is received from the HCI.
 *
 * Returns          void
 *
 ******************************************************************************/
void btm_ble_read_antenna_info_complete(uint8_t* p, uint16_t evt_len) {
  STREAM_TO_UINT8(btm_cb.ble_antenna_info.status, p);
  BTM_TRACE_DEBUG("btm_ble_read_antenna_info_complete, status = %d", btm_cb.ble_antenna_info.status);

  if (btm_cb.ble_antenna_info.status == HCI_SUCCESS) {
    STREAM_TO_UINT8(btm_cb.ble_antenna_info.supp_switching_sampling_rates, p);
    STREAM_TO_UINT8(btm_cb.ble_antenna_info.num_antennae, p);
    STREAM_TO_UINT8(btm_cb.ble_antenna_info.max_switching_pattern_len, p);
    STREAM_TO_UINT8(btm_cb.ble_antenna_info.max_cte_len, p);
  }
}

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
void btm_ble_aoa_conn_iq_rpt_evt(uint16_t conn_handle, uint8_t* p_data, uint16_t evt_len) {
  tBTM_SEC_DEV_REC* p_dev_rec = btm_find_dev_by_handle(conn_handle);
  BTM_TRACE_DEBUG("%s:", __func__);

  if (!p_dev_rec) {
    BTM_TRACE_WARNING("%s: No Device Found!", __func__);
    return;
  }

  if (hci_aoa_cmd_cmpl.aoa_conn_iq_rpt_evt_cb) {
    hci_aoa_cmd_cmpl.aoa_conn_iq_rpt_evt_cb.Run(p_dev_rec->ble.pseudo_addr, p_data, evt_len);
  }
}
#endif //DIR_FINDING_FEATURE

