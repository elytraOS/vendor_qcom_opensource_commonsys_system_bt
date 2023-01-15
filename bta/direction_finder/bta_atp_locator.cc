/******************************************************************************
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 *******************************************************************************/
#ifdef DIR_FINDING_FEATURE

#include "bt_target.h"
#include "bta_atp_locator_api.h"
#include "bta_gatt_api.h"
#include "btm_int.h"
#include "device/include/controller.h"
#include "gap_api.h"
#include "gatt_api.h"
#include "gattc_ops_queue.h"
#include "osi/include/properties.h"
#include "btm_ble_direction_finder_api.h"

#include <base/bind.h>
#include <base/callback.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <hardware/bt_atp_locator.h>
#include <vector>
#include "bta_sys.h"

using base::Bind;
using base::Closure;
using bluetooth::Uuid;
using bluetooth::directionfinder::GattOpsQueue;
using bluetooth::atp_locator::ConnectionState;

// Assigned Numbers for CTES
Uuid CTES_UUID          = Uuid::FromString("184A");
Uuid CTE_ENABLE_CHAR_UUID      = Uuid::FromString("2BAD");

void atp_locator_gattc_callback(tBTA_GATTC_EVT event, tBTA_GATTC* p_data);
void atp_locator_encryption_callback(const RawAddress*, tGATT_TRANSPORT, void*, tBTM_STATUS);
const char* atp_locator_gatt_callback_evt_str(uint8_t event);
const char* atp_locator_handle_ctes_evt_str(uint8_t event);

class AtpLocatorImpl;
static AtpLocatorImpl* instance;

class AoaDeviceManager {
 private:

 public:
  void Add(AoaDevice device) {
    if (FindByAddress(device.address) != nullptr) return;
    devices.push_back(device);
  }

  void Remove(const RawAddress& address) {
    for (auto it = devices.begin(); it != devices.end();) {
      if (it->address != address) {
        ++it;
        continue;
      }

      it = devices.erase(it);
      return;
    }
  }

  AoaDevice* FindByAddress(const RawAddress& address) {
    auto iter = std::find_if(devices.begin(), devices.end(),
                             [&address](const AoaDevice& device) {
                               return device.address == address;
                             });

    return (iter == devices.end()) ? nullptr : &(*iter);
  }

  AoaDevice* FindByConnId(uint16_t conn_id) {
    auto iter = std::find_if(devices.begin(), devices.end(),
                             [&conn_id](const AoaDevice& device) {
                               return device.conn_id == conn_id;
                             });

    return (iter == devices.end()) ? nullptr : &(*iter);
  }

  size_t size() { return (devices.size()); }

  std::vector<AoaDevice> devices;
  uint8_t switching_pattern_len;
  uint8_t switching_pattern[75];
};

class AtpLocatorImpl : public AtpLocator {
 private:
  uint8_t gatt_if;
  bluetooth::atp_locator::AtpLocatorCallbacks* callbacks;
  AoaDeviceManager aoaDeviceMgr;

 public:
  virtual ~AtpLocatorImpl() = default;

  AtpLocatorImpl(bluetooth::atp_locator::AtpLocatorCallbacks* callbacks)
      : gatt_if(0),
        callbacks(callbacks) {
    LOG(INFO) << "AtpLocatorImpl gattc app register";

    BTA_GATTC_AppRegister(
        atp_locator_gattc_callback,
        base::Bind(
            [](uint8_t client_id, uint8_t status) {
              if (status != GATT_SUCCESS) {
                LOG(ERROR) << "Can't start ATP profile - no gatt "
                              "clients left!";
                return;
              }
              if (instance != nullptr) {
                LOG(INFO) << "AtpLocator client_id: " << loghex(client_id);
                instance->gatt_if = client_id;
              }
            }), true);

    char value[PROPERTY_VALUE_MAX] = {'\0'};
    char val_switching_pattern[PROPERTY_VALUE_MAX] = {'\0'};
    char *endptr;
    uint8_t antenna_id;

    uint8_t* antenna_info = (uint8_t*) controller_get_interface()->get_antenna_info_ble()->as_array;
    aoaDeviceMgr.switching_pattern_len = 0;
    int ret = 0;
    ret = property_get("persist.vendor.service.bt.aoa.switching_pattern_len", value, "0");
    if (ret) {
      aoaDeviceMgr.switching_pattern_len = ((strcmp(value, "0") == 0) ?
          aoaDeviceMgr.switching_pattern_len : (strtol(value, &endptr, 10)));
    }

    LOG(INFO) << __func__ << ": switching_pattern_len:" << loghex(aoaDeviceMgr.switching_pattern_len);
    if (aoaDeviceMgr.switching_pattern_len == 0) {
      if (antenna_info) {
        LOG(INFO) << __func__ << ": antenna_info, num antennae=" << loghex(antenna_info[1])
                  << ": antenna_info, max_switching_pattern_len=" << loghex(antenna_info[2]);
        aoaDeviceMgr.switching_pattern_len = antenna_info[1];
        for(int i=0; i<antenna_info[1]; i++) {
          aoaDeviceMgr.switching_pattern[i] = i;
        }
      }
    } else {
      ret = property_get("persist.vendor.service.bt.aoa.switching_pattern", val_switching_pattern, "0");
      if (ret && strcmp(val_switching_pattern, "0")) {
        std::stringstream ss(val_switching_pattern);
        std::string antenna_id_str;
        int i=0;
        while (!ss.eof()) {
          getline(ss, antenna_id_str, ',');
          LOG(INFO) << __func__ << ": antenna_id_str:" << antenna_id_str;
          antenna_id = ((strcmp(antenna_id_str.c_str(), "0") == 0) ? 0 : (strtol(antenna_id_str.c_str(), &endptr, 10)));
          aoaDeviceMgr.switching_pattern[i++] = antenna_id;
        }
      }
    }
  }

  void Connect(const RawAddress& address, bool isDirect) override {
    LOG(INFO) << __func__ << " " << address << ", isDirect = " << logbool(isDirect);
    AoaDevice* aoaDev = aoaDeviceMgr.FindByAddress(address);

    if (aoaDev) {
      LOG(ERROR) << __func__ << "Device already in connected/connecting state" << address;
      return;
    }

    aoaDeviceMgr.Add(AoaDevice(address));
    aoaDev = aoaDeviceMgr.FindByAddress(address);
    if (!aoaDev) {
      LOG(ERROR) << __func__ << "Device address could not be found";
      return;
    }
    aoaDev->state = BTA_ATP_CONNECTING;
    callbacks->OnConnectionState(ConnectionState::CONNECTING, aoaDev->address);

    if (!isDirect) {
      aoaDev->bg_conn = true;
    }

    BTA_GATTC_Open(gatt_if, address, isDirect, GATT_TRANSPORT_LE, false);
  }

  void Disconnect(const RawAddress& address) override {
    AoaDevice* aoaDev = aoaDeviceMgr.FindByAddress(address);
    std::vector<uint8_t> vect_val;

    if (!aoaDev) {
      LOG(ERROR) << __func__ << "Device not connected to profile" << address;
      return;
    }

    LOG(INFO) << __func__ << " " << address;

    aoaDev->atp_locator_service.sampling_enable = 0;
    aoaDev->atp_locator_service.slot_durations = 0;

    aoaDev->atp_locator_service.cte_enable = 0;
    LOG(INFO) << __func__ << ": cte_enable=" << loghex(aoaDev->atp_locator_service.cte_enable);
    aoaDev->atp_locator_service.cte_req_int = 0;
    aoaDev->atp_locator_service.req_cte_len = 0;
    aoaDev->atp_locator_service.dir_finding_type = 0;
    aoaDev->atp_locator_service.is_disconnect_issued = true;
    vect_val.push_back(ATP_AOA_CTE_DISABLE);

    if (aoaDev->is_cte_enabled == true) {
      //disable
      do_in_bta_thread(FROM_HERE, Bind(&BTM_BleSetConnCTEReqEnable, aoaDev->address,
                       aoaDev->atp_locator_service.cte_enable,
                       aoaDev->atp_locator_service.cte_req_int,
                       aoaDev->atp_locator_service.req_cte_len,
                       aoaDev->atp_locator_service.dir_finding_type,
                       Bind(AtpLocatorImpl::OnConnCteReqEnableCompleteStatic),
                       Bind(AtpLocatorImpl::OnConnIqReportEventRcvdStatic)));
    } else {
      aoaDev->state = BTA_ATP_DISCONNECTING;
      callbacks->OnConnectionState(ConnectionState::DISCONNECTING, aoaDev->address);
      AtpGattClose(aoaDev);
    }
  }

  uint8_t GetSwitchingPatternLen() {
    LOG(INFO) << __func__ << " switching_pattern_len:"
              << loghex(aoaDeviceMgr.switching_pattern_len);
    return aoaDeviceMgr.switching_pattern_len;
  }

  uint8_t* GetSwitchingPattern() {
    return aoaDeviceMgr.switching_pattern;
  }

  AoaResultsCollector* FindOrCreateAoaResultsCollector(const RawAddress& address, AtpLocatorImpl *atpLocatorImpl) {
    AoaResultsCollector *resultCollector = nullptr;
    if (mAoaResultCollectors.find(address) != mAoaResultCollectors.end()) {
      resultCollector = mAoaResultCollectors[address];
    }
    if (resultCollector == nullptr) {
      resultCollector = new AoaResultsCollector(address, atpLocatorImpl);
      //Start new result collector
      if (resultCollector != nullptr) {
        BTIF_TRACE_DEBUG("Creating new result collector");
        mAoaResultCollectors[address] = resultCollector;
      } else {
        BTIF_TRACE_ERROR("result collector is null");
      }
    }
    return resultCollector;
  }

  void OnLeAoaResults(RawAddress& address, int status, double azimuth, uint8_t azimuth_unc,
      double elevation, uint8_t elevation_unc) {
    LOG(INFO) << __func__ << " address:" << address << " status:" << loghex(status) << " azimuth:" << azimuth
              << " elevation:" << elevation;
    callbacks->OnLeAoaResults(status, azimuth,  azimuth_unc, elevation, elevation_unc, address);
  }

  void OnConnIqReportEventRcvd(RawAddress address, uint8_t* p_data, uint16_t evt_len) {
    VLOG(1) << "OnConnIqReportEventRcvd";

    AoaDevice* aoaDev = aoaDeviceMgr.FindByAddress(address);
    if (!aoaDev) {
      LOG(ERROR) << __func__ << " Skipping unknown device" << address;
      return;
    }

    AoaResultsCollector* resc = FindOrCreateAoaResultsCollector(address, this);
    if (resc != nullptr) {
      resc->convertHciEvtResultToAlgoInput(p_data, evt_len);
    }
  }

  void OnConnCteReqEnableComplete(uint8_t* p_data, uint16_t evt_len) {
    uint8_t status;
    uint16_t conn_handle;
    std::vector<uint8_t> vect_val;

    STREAM_TO_UINT8(status, p_data);
    LOG(INFO) << __func__ << ": status=" << loghex(status);

    STREAM_TO_UINT16(conn_handle, p_data);

    tBTM_SEC_DEV_REC* p_dev_rec = btm_find_dev_by_handle(conn_handle);
    if (!p_dev_rec) {
      LOG(ERROR) << __func__ << ": dev rec is NULL";
      return;
    }
    AoaDevice* aoaDev = aoaDeviceMgr.FindByAddress(p_dev_rec->ble.pseudo_addr);

    if (!aoaDev) {
      LOG(ERROR) << __func__ << "Device not connected to profile" << p_dev_rec->ble.pseudo_addr;
      return;
    }

    if (aoaDev->conn_id == 0) {
      LOG(ERROR) << __func__ << ": GATT is not connected, skip enable AoA CTE";
      return;
    }

    if (aoaDev->state != BTA_ATP_CONNECTED) {
      LOG(ERROR) << __func__ << ": ATP is not connected, skip enable AoA CTE, state = "
                << loghex(aoaDev->state);
      return;
    }

    if (status == BT_STATUS_SUCCESS) {
      callbacks->OnEnableBleDirectionFinding(status, aoaDev->address);

      if (aoaDev->atp_locator_service.cte_enable == 1) {
        aoaDev->is_cte_enabled = true;
      } else if (aoaDev->atp_locator_service.cte_enable == 0) {
        vect_val.push_back(ATP_AOA_CTE_DISABLE);
        //send GATT write to CTE Disable CTE on char
        GattOpsQueue::WriteCharacteristic(gatt_if,
             aoaDev->conn_id, aoaDev->atp_locator_service.cte_enable_handle,
             vect_val, GATT_WRITE, AtpLocatorImpl::OnCteEnableWriteStatic,
             nullptr);
      }
    }
    else {
      LOG(ERROR) << __func__ << ": status failure";
      callbacks->OnEnableBleDirectionFinding(status, aoaDev->address);
    }
  }

  static void OnConnCteReqEnableCompleteStatic(uint8_t* p_data, uint16_t evt_len) {
    if (instance)
      instance->OnConnCteReqEnableComplete(p_data, evt_len);
  }

  static void OnConnIqReportEventRcvdStatic(RawAddress address, uint8_t* p_data, uint16_t evt_len) {
    if (instance)
      instance->OnConnIqReportEventRcvd(address, p_data, evt_len);
  }

  void OnConnCteRcvParamsComplete(uint8_t* p_data, uint16_t evt_len) {
    uint8_t status;
    uint16_t conn_handle;

    STREAM_TO_UINT8(status, p_data);
    LOG(INFO) << __func__ << ": status=" << loghex(status);

    STREAM_TO_UINT16(conn_handle, p_data);
    LOG(INFO) << __func__ << ": conn_handle=" << loghex(conn_handle);

    tBTM_SEC_DEV_REC* p_dev_rec = btm_find_dev_by_handle(conn_handle);
    if (!p_dev_rec) {
      LOG(ERROR) << __func__ << ": dev rec is NULL";
      return;
    }
    LOG(INFO) << __func__ << ": address=" << p_dev_rec->ble.pseudo_addr;
    AoaDevice* aoaDev = aoaDeviceMgr.FindByAddress(p_dev_rec->ble.pseudo_addr);

    if (!aoaDev) {
      LOG(ERROR) << __func__ << "Device not connected to profile" << p_dev_rec->ble.pseudo_addr;
      return;
    }

    if (aoaDev->conn_id == 0) {
      LOG(ERROR) << __func__ << ": GATT is not connected, skip enable AoA CTE";
      return;
    }

    if (aoaDev->state != BTA_ATP_CONNECTED) {
      LOG(ERROR) << __func__ << ": ATP is not connected, skip enable AoA CTE, state = "
                << loghex(aoaDev->state);
      return;
    }

    if (status == BT_STATUS_SUCCESS) {
      LOG(INFO) << __func__ << ": cte_enable=" << loghex(aoaDev->atp_locator_service.cte_enable)
                << ": cte_req_int=" << loghex(aoaDev->atp_locator_service.cte_req_int)
                << ": req_cte_len=" << loghex(aoaDev->atp_locator_service.req_cte_len)
                << ": dir_finding_type=" << loghex(aoaDev->atp_locator_service.dir_finding_type);

      do_in_bta_thread(FROM_HERE, Bind(&BTM_BleSetConnCTEReqEnable, aoaDev->address,
          aoaDev->atp_locator_service.cte_enable,
          aoaDev->atp_locator_service.cte_req_int,
          aoaDev->atp_locator_service.req_cte_len,
          aoaDev->atp_locator_service.dir_finding_type,
          Bind(AtpLocatorImpl::OnConnCteReqEnableCompleteStatic),
          Bind(AtpLocatorImpl::OnConnIqReportEventRcvdStatic)));
    }
    else {
      LOG(ERROR) << __func__ << ": status failure";
      callbacks->OnEnableBleDirectionFinding(status, aoaDev->address);
    }
  }

  static void OnConnCteRcvParamsCompleteStatic(uint8_t* p_data, uint16_t evt_len) {
    if (instance)
      instance->OnConnCteRcvParamsComplete(p_data, evt_len);
  }

  void EnableBleDirectionFinding(uint8_t sampling_enable, uint8_t slot_durations,
      uint8_t enable, uint16_t cte_req_int, uint8_t req_cte_len,
      uint8_t dir_finding_type, const RawAddress& address) {
    AoaDevice* aoaDev = aoaDeviceMgr.FindByAddress(address);
    std::vector<uint8_t> vect_val;

    if (!aoaDev) {
      LOG(ERROR) << __func__ << "Device not connected to profile" << address;
      return;
    }

    if (aoaDev->conn_id == 0) {
      LOG(ERROR) << __func__ << ": GATT is not connected, skip EnableBleDirectionFinding";
      return;
    }

    if (aoaDev->state != BTA_ATP_CONNECTED) {
      LOG(ERROR)
          << __func__ << ": ATP is not connected, skip EnableBleDirectionFinding, state = "
          << loghex(aoaDev->state);
      return;
    }

    LOG(INFO) << __func__ << ": EnableBleDirectionFinding, device=" << aoaDev->address
              << " enable:" << loghex(enable);

    if (enable) {
      vect_val.push_back(ATP_AOA_CTE_ENABLE);
      aoaDev->atp_locator_service.sampling_enable = sampling_enable;
      aoaDev->atp_locator_service.slot_durations = slot_durations;
      LOG(INFO) << __func__ << ": sampling_enable=" << loghex(sampling_enable)
                << ": slot_durations=" << loghex(slot_durations);
      aoaDev->atp_locator_service.cte_enable = enable;
      aoaDev->atp_locator_service.cte_req_int = cte_req_int;
      aoaDev->atp_locator_service.req_cte_len = req_cte_len;
      aoaDev->atp_locator_service.dir_finding_type = dir_finding_type;
      LOG(INFO) << __func__ << ": cte_enable=" << loghex(enable)
                << ": cte_req_int=" << loghex(cte_req_int)
                << ": req_cte_len=" << loghex(req_cte_len)
                << ": dir_finding_type=" << loghex(dir_finding_type);

      LOG(INFO) << __func__ << ": Enable AoA CTE, vect_val=" << loghex(vect_val[0]);
      //send GATT write to CTE Enable char
      GattOpsQueue::WriteCharacteristic(gatt_if,
          aoaDev->conn_id, aoaDev->atp_locator_service.cte_enable_handle,
          vect_val, GATT_WRITE, AtpLocatorImpl::OnCteEnableWriteStatic,
          nullptr);
    } else {
      vect_val.push_back(ATP_AOA_CTE_DISABLE);
      aoaDev->atp_locator_service.sampling_enable = 0;
      aoaDev->atp_locator_service.slot_durations = 0;
      aoaDev->atp_locator_service.cte_enable = 0;
      LOG(INFO) << __func__ << ": cte_enable=" << loghex(aoaDev->atp_locator_service.cte_enable);
      aoaDev->atp_locator_service.cte_req_int = 0;
      aoaDev->atp_locator_service.req_cte_len = 0;
      aoaDev->atp_locator_service.dir_finding_type = 0;
      aoaDev->atp_locator_service.is_disconnect_issued = false;

      //disable
      do_in_bta_thread(FROM_HERE, Bind(&BTM_BleSetConnCTEReqEnable, aoaDev->address,
                       aoaDev->atp_locator_service.cte_enable,
                       aoaDev->atp_locator_service.cte_req_int,
                       aoaDev->atp_locator_service.req_cte_len,
                       aoaDev->atp_locator_service.dir_finding_type,
                       Bind(AtpLocatorImpl::OnConnCteReqEnableCompleteStatic),
                       Bind(AtpLocatorImpl::OnConnIqReportEventRcvdStatic)));
    }
  }

  void AtpGattClose(AoaDevice* aoaDev) {
    LOG(INFO) << __func__ << ": address: " << aoaDev->address
                          << ", conn_id: " << loghex(aoaDev->conn_id)
                          << ", bg_conn: " << logbool(aoaDev->bg_conn);

    // Removes all registrations for connection.
    if (aoaDev->bg_conn) {
      BTA_GATTC_CancelOpen(gatt_if, aoaDev->address, false);
      aoaDev->bg_conn = false;
    }

    if (aoaDev->conn_id) {
      GattOpsQueue::Clean(aoaDev->conn_id);
      BTA_GATTC_Close(aoaDev->conn_id);
    } else {
       // cancel pending direct connect
      BTA_GATTC_CancelOpen(gatt_if, aoaDev->address, true);
      PostDisconnected(aoaDev);
    }
  }

  void OnGattConnected(tGATT_STATUS status, uint16_t conn_id,
                       tGATT_IF client_if, RawAddress address,
                       tBTA_TRANSPORT transport, uint16_t mtu) {
    AoaDevice* aoaDev = aoaDeviceMgr.FindByAddress(address);

    if (!aoaDev) {
      LOG(WARNING) << "Closing connection to ATP device, address="
                   << address;
      return;
    }

    LOG(INFO) << __func__ <<  ": address=" << address
                          << ", conn_id=" << conn_id
                          << ", bg_conn: " << logbool(aoaDev->bg_conn);

    if (status != GATT_SUCCESS) {
      if (aoaDev->bg_conn) {
        // whitelist connection failed, that's ok.
        LOG(ERROR) << __func__ << ": bg conn failed, return immediately";
        return;
      }

      LOG(INFO) << ": Failed to connect to ctes device";
      aoaDeviceMgr.Remove(address);
      callbacks->OnConnectionState(ConnectionState::DISCONNECTED, address);
      return;
    }

    if (aoaDev->bg_conn) {
        LOG(INFO) << __func__ <<  ": backgound connection from: address=" << address;
    }

    aoaDev->bg_conn = false;
    aoaDev->conn_id = conn_id;

    BTA_GATTC_ServiceSearchRequest(aoaDev->conn_id, &CTES_UUID);
  }

  void OnGattDisconnected(tGATT_STATUS status, uint16_t conn_id,
                          tGATT_IF client_if, RawAddress remote_bda,
                          tBTA_GATT_REASON reason) {
    AoaDevice* aoaDev = aoaDeviceMgr.FindByConnId(conn_id);

    if (!aoaDev) {
      LOG(WARNING) << "Skipping unknown device disconnect, conn_id="
              << loghex(conn_id);
      return;
    }
    LOG(INFO) << __func__ << ": conn_id=" << loghex(conn_id)
                          << ", reason=" << loghex(reason)
                          << ", remote_bda=" << remote_bda
                          << ", bg_conn: " << logbool(aoaDev->bg_conn);

    // Removes all registrations for connection.
    if (aoaDev->bg_conn) {
      BTA_GATTC_CancelOpen(gatt_if, remote_bda, false);
      aoaDev->bg_conn = false;
    }

    PostDisconnected(aoaDev);
  }

  void PostDisconnected(AoaDevice* aoaDev) {
    LOG(INFO) << __func__ << ": address: " << aoaDev->address
                          << ", conn_id: " << aoaDev->conn_id;
    aoaDev->state = BTA_ATP_DISCONNECTED;

    if (aoaDev->conn_id) {
      GattOpsQueue::Clean(aoaDev->conn_id);
      BTA_GATTC_Close(aoaDev->conn_id);
      aoaDev->conn_id = 0;
    }

    callbacks->OnConnectionState(ConnectionState::DISCONNECTED, aoaDev->address);
    aoaDeviceMgr.Remove(aoaDev->address);
  }

  void OnEncryptionComplete(const RawAddress& address, bool success) {
    AoaDevice* aoaDev = aoaDeviceMgr.FindByAddress(address);
    LOG(INFO) << __func__ << ": address: " << address;

    if (!aoaDev) {
      LOG(WARNING)  << ": Skipping unknown device: " << address;
      return;
    }

    if (!success) {
      LOG(ERROR) << ": encryption failed, for device: " << address
                 << ", bg_conn: " << logbool(aoaDev->bg_conn)
                 << ", conn_id: " << aoaDev->conn_id;

      // Removes all registrations for connection.
      AtpGattClose(aoaDev);
      return;
    }

     BTA_GATTC_ServiceSearchRequest(aoaDev->conn_id, &CTES_UUID);
  }

  void OnServiceSearchComplete(uint16_t conn_id, tGATT_STATUS status) {
    AoaDevice* aoaDev = aoaDeviceMgr.FindByConnId(conn_id);
    LOG(INFO) << __func__;

    if (!aoaDev) {
      LOG(WARNING)  << "Skipping unknown device, conn_id=" << loghex(conn_id);
      return;
    }

    if (status != GATT_SUCCESS) {
      /* close connection and report service discovery complete with error */
      LOG(ERROR) << "Service discovery failed";
      AtpGattClose(aoaDev);
      return;
    }

    const std::vector<gatt::Service>* services = BTA_GATTC_GetServices(conn_id);

    const gatt::Service* service = nullptr;
    if (services) {
      for (const gatt::Service& tmp : *services) {
        if (tmp.uuid == CTES_UUID) {
          LOG(INFO) << "Found Constant tone extension service, handle=" << loghex(tmp.handle);
          service = &tmp;
        }
      }
    } else {
      LOG(ERROR) << "no services found for conn_id: " << conn_id;
      return;
    }

    if (!service) {
      LOG(ERROR) << "No CTES found";
      AtpGattClose(aoaDev);
      return;
    }

    for (const gatt::Characteristic& charac : service->characteristics) {
      if (charac.uuid == CTE_ENABLE_CHAR_UUID) {
        aoaDev->atp_locator_service.cte_enable_handle = charac.value_handle;
        LOG(INFO) << __func__
                  << ": atp cte_enable_handle=" << loghex(charac.value_handle);
      } else {
        LOG(WARNING) << "Unknown characteristic found:" << charac.uuid;
      }
    }

    // Send conn complete after CTE enable char is found.
    if (aoaDev->state == BTA_ATP_CONNECTING) {
      LOG(INFO) << __func__ << ": ATP Connection Setup complete";
      if (aoaDev->atp_locator_service.cte_enable_handle != 0) {
        aoaDev->state = BTA_ATP_CONNECTED;
        callbacks->OnConnectionState(ConnectionState::CONNECTED, aoaDev->address);
        return;
      }
    }
  }

  void OnServiceChangeEvent(const RawAddress& address) {
    AoaDevice* aoaDev = aoaDeviceMgr.FindByAddress(address);
    if (!aoaDev) {
      LOG(ERROR) <<  __func__ << "Skipping unknown device" << address;
      return;
    }
    LOG(INFO) << __func__ << ": address=" << address;
    aoaDev->service_changed_rcvd = true;
    GattOpsQueue::Clean(aoaDev->conn_id);
  }

  void OnServiceDiscDoneEvent(const RawAddress& address) {
    AoaDevice* aoaDev = aoaDeviceMgr.FindByAddress(address);
    if (!aoaDev) {
      LOG(ERROR) << __func__ << "Skipping unknown device" << address;
      return;
    }
    if (aoaDev->service_changed_rcvd) {
      BTA_GATTC_ServiceSearchRequest(aoaDev->conn_id, &CTES_UUID);
    }
  }

  void OnCteEnableWrite(uint16_t client_id, uint16_t conn_id, tGATT_STATUS status,
                        uint16_t handle, void* data) {
    AoaDevice* aoaDev = aoaDeviceMgr.FindByConnId(conn_id);

    if (!aoaDev) {
      LOG(WARNING) << __func__ << "Skipping unknown read event, conn_id="
                   << loghex(conn_id);
      return;
    }

    LOG(INFO) << __func__ << " " << aoaDev->address << ", status: " << loghex(status)
              << ", ATP device state: " << loghex(aoaDev->state);

    if (status != GATT_SUCCESS) {
      LOG(ERROR) << "Error writing CTE Enable characteristic for device" << aoaDev->address;
    }

    HandleCtesEvent(aoaDev, ATP_CTE_ENABLE_WRITE_CMPL_EVT, status);
  }

  static void OnCteEnableWriteStatic(uint16_t client_id, uint16_t conn_id,
                                     tGATT_STATUS status,
                                     uint16_t handle, void* data) {
    if (instance)
      instance->OnCteEnableWrite(client_id, conn_id, status, handle, data);
  }

  void HandleCtesEvent(AoaDevice* aoaDev, uint32_t event, tGATT_STATUS status) {
    LOG(INFO) << __func__ << " event = " << atp_locator_handle_ctes_evt_str(event);

    if (status != GATT_SUCCESS && status != GATT_CONGESTED) {
      if (aoaDev->state == BTA_ATP_CONNECTING) {
        LOG(ERROR) << __func__ << ": Error status while ATP connecting, Close GATT for device: "
                               << aoaDev->address;
        AtpGattClose(aoaDev);
        return;
      } else if  (aoaDev->state == BTA_ATP_CONNECTED) {
        LOG(ERROR) << __func__ << ": Error status while ATP is connected for device: "
                               << aoaDev->address;
        return;
      } else {
        LOG(ERROR) << __func__ << ": Error status in disconnected or disconnecting "
                               << "Ignore handle CTES Event  for device: " << aoaDev->address;
        return;
      }
    }

    switch (event) {
      case ATP_CTE_ENABLE_WRITE_CMPL_EVT: {
        if (status == GATT_SUCCESS) {
          LOG(INFO) << __func__ << ": GATT write success, sampling_enable="
                    << loghex(aoaDev->atp_locator_service.sampling_enable)
                    << ": slot_durations=" << loghex(aoaDev->atp_locator_service.slot_durations)
                    << ": cte enable=" << loghex(aoaDev->atp_locator_service.cte_enable);

          if (aoaDev->atp_locator_service.cte_enable == 1) {
            std::vector<uint8_t> antenna_ids_vec(aoaDeviceMgr.switching_pattern,
                aoaDeviceMgr.switching_pattern + aoaDeviceMgr.switching_pattern_len);
            do_in_bta_thread(FROM_HERE, Bind(&BTM_BleSetConnCTERxParams, aoaDev->address,
                             aoaDev->atp_locator_service.sampling_enable,
                             aoaDev->atp_locator_service.slot_durations,
                             aoaDeviceMgr.switching_pattern_len,
                             std::move(antenna_ids_vec),
                             Bind(AtpLocatorImpl::OnConnCteRcvParamsCompleteStatic)));
          } else {
            //disable
            aoaDev->is_cte_enabled = false;
            if ((aoaDev->atp_locator_service.cte_enable == 0) &&
                (aoaDev->atp_locator_service.is_disconnect_issued == true)) {
              aoaDev->state = BTA_ATP_DISCONNECTING;
              callbacks->OnConnectionState(ConnectionState::DISCONNECTING, aoaDev->address);
              AtpGattClose(aoaDev);
            }
          }
        }
        break;
      }

      default:
        LOG(INFO) << __func__ << ": unexpected ATP event";
        break;
    }
  }

  void OnCongestionEvent(uint16_t conn_id, bool congested) {
    AoaDevice* device = aoaDeviceMgr.FindByConnId(conn_id);
    if (!device) {
      LOG(ERROR) << __func__
                << ": Skipping unknown device, conn_id=" << loghex(conn_id);
      return;
    }

    LOG(WARNING) << __func__ << ": conn_id:" << loghex(conn_id)
                             << ", congested: " << congested;
    GattOpsQueue::CongestionCallback(conn_id, congested);
  }

  void CleanUp() {
    LOG(INFO) << __func__;

    for (AoaDevice& device : aoaDeviceMgr.devices) {
      AoaResultsCollector* resc = FindOrCreateAoaResultsCollector(device.address, this);
      if (resc != nullptr) {
        resc->CleanupData();
      }
    }

    BTA_GATTC_AppDeregister(gatt_if);
    for (AoaDevice& device : aoaDeviceMgr.devices) {
      PostDisconnected(&device);
    }

    aoaDeviceMgr.devices.clear();
    aoaDeviceMgr.switching_pattern_len = 0;
  }
};

void atp_locator_gattc_callback(tBTA_GATTC_EVT event, tBTA_GATTC* p_data) {
  LOG(INFO) << __func__ << " event = " << atp_locator_gatt_callback_evt_str(event);

  if (p_data == nullptr) return;

  switch (event) {
    case BTA_GATTC_DEREG_EVT:
      break;

    case BTA_GATTC_OPEN_EVT: {
      if (!instance) return;
      tBTA_GATTC_OPEN& o = p_data->open;
      instance->OnGattConnected(o.status, o.conn_id, o.client_if, o.remote_bda,
                                o.transport, o.mtu);
      break;
    }

    case BTA_GATTC_CLOSE_EVT: {
      if (!instance) return;
      tBTA_GATTC_CLOSE& c = p_data->close;
      instance->OnGattDisconnected(c.status, c.conn_id, c.client_if,
                                   c.remote_bda, c.reason);
    } break;

    case BTA_GATTC_SEARCH_CMPL_EVT:
      if (!instance) return;
      instance->OnServiceSearchComplete(p_data->search_cmpl.conn_id,
                                        p_data->search_cmpl.status);
      break;

    case BTA_GATTC_ENC_CMPL_CB_EVT:
      if (!instance) return;
      instance->OnEncryptionComplete(p_data->enc_cmpl.remote_bda, true);
      break;

    case BTA_GATTC_SRVC_CHG_EVT:
      if (!instance) return;
      instance->OnServiceChangeEvent(p_data->remote_bda);
      break;

    case BTA_GATTC_SRVC_DISC_DONE_EVT:
      if (!instance) return;
      instance->OnServiceDiscDoneEvent(p_data->remote_bda);
      break;

    case BTA_GATTC_CONGEST_EVT:
      if (!instance) return;
      instance->OnCongestionEvent(p_data->congest.conn_id,
                                  p_data->congest.congested);
      break;

    case BTA_GATTC_SEARCH_RES_EVT:
    case BTA_GATTC_CANCEL_OPEN_EVT:
    case BTA_GATTC_CONN_UPDATE_EVT:

    default:
      break;
  }
}

void atp_locator_encryption_callback(const RawAddress* address,
                            UNUSED_ATTR tGATT_TRANSPORT transport,
                            UNUSED_ATTR void* data, tBTM_STATUS status) {
  if (instance) {
    instance->OnEncryptionComplete(*address,
                                   status == BTM_SUCCESS ? true : false);
  }
}

void AtpLocator::Initialize(
            bluetooth::atp_locator::AtpLocatorCallbacks* callbacks) {
  LOG(INFO) << __func__ ;

  if (instance) {
    LOG(ERROR) << "Already initialized!";
  }

  instance = new AtpLocatorImpl(callbacks);
}

bool AtpLocator::IsAtpLocatorRunning() { return instance; }

AtpLocator* AtpLocator::Get() {
  CHECK(instance);
  return instance;
};

int AtpLocator::GetDeviceCount() {
  if (!instance) {
    LOG(ERROR) << __func__ << ": Not initialized yet";
    return 0;
  }

  return (instance->GetDeviceCount());
}

void AtpLocator::CleanUp() {
  LOG(INFO) << __func__;
  AtpLocatorImpl* ptr = instance;
  instance = nullptr;

  ptr->CleanUp();
  delete ptr;
};

/*******************************************************************************
 *  Debugging functions
 ******************************************************************************/
#define CASE_RETURN_STR(const) \
  case const:                  \
    return #const;

const char* atp_locator_gatt_callback_evt_str(uint8_t event) {
  switch (event) {
    CASE_RETURN_STR(BTA_GATTC_DEREG_EVT)
    CASE_RETURN_STR(BTA_GATTC_OPEN_EVT)
    CASE_RETURN_STR(BTA_GATTC_CLOSE_EVT)
    CASE_RETURN_STR(BTA_GATTC_SEARCH_CMPL_EVT)
    CASE_RETURN_STR(BTA_GATTC_NOTIF_EVT)
    CASE_RETURN_STR(BTA_GATTC_ENC_CMPL_CB_EVT)
    CASE_RETURN_STR(BTA_GATTC_SEARCH_RES_EVT)
    CASE_RETURN_STR(BTA_GATTC_CANCEL_OPEN_EVT)
    CASE_RETURN_STR(BTA_GATTC_SRVC_CHG_EVT)
    CASE_RETURN_STR(BTA_GATTC_CONN_UPDATE_EVT)
    CASE_RETURN_STR(BTA_GATTC_SRVC_DISC_DONE_EVT)
    CASE_RETURN_STR(BTA_GATTC_CONGEST_EVT)
    default:
      return (char*)"Unknown GATT Callback Event";
  }
}

const char* atp_locator_handle_ctes_evt_str(uint8_t event) {
  switch (event) {
    CASE_RETURN_STR(ATP_CTE_ENABLE_READ_CMPL_EVT)
    CASE_RETURN_STR(ATP_CTE_ENABLE_WRITE_CMPL_EVT)
    default:
      return (char*)"Unknown handling ATP Event";
  }
}
#endif //DIR_FINDING_FEATURE