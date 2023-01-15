/******************************************************************************
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 *******************************************************************************/
/******************************************************************************
 *
 *  This is the main implementation file for the BTA  CTE server.
 *
 ******************************************************************************/
#ifdef DIR_FINDING_FEATURE

#include "bta_api.h"
#include "btif/include/btif_util.h"
#include "bt_target.h"
#include "bta_ctes_api.h"
#include "gatts_ops_queue.h"
#include "btm_int.h"
#include "device/include/controller.h"
#include "btm_ble_direction_finder_api.h"

#include "osi/include/properties.h"
#include "osi/include/alarm.h"
#include "osi/include/allocator.h"
#include "osi/include/osi.h"
#include "bta_sys.h"

#include <vector>
#include <iostream>
#include <string.h>
#include <base/bind.h>
#include <base/callback.h>
#include <base/logging.h>
#include <base/location.h>
#include <hardware/bluetooth.h>
#include <hardware/bt_apm.h>

#include <base/strings/string_number_conversions.h>

#include <vector>
#include <string.h>
#include <algorithm>
#include <map>

#define CTE_RSP_DISABLE 0
#define CTE_RSP_ENABLE 1

using bluetooth::Uuid;
using bluetooth::directionfinder::GattsOpsQueue;
using base::Bind;
using base::Closure;

class CtesManagerImpl;
static CtesManagerImpl *cte_instance;

Uuid CONSTANT_TONE_EXT_SERVICE_UUID = Uuid::FromString("0000184A-0000-1000-8000-00805F9B34FB");

Uuid CONSTANT_TONE_EXT_ENABLE_CHAR_UUID = Uuid::FromString("00002BAD-0000-1000-8000-00805F9B34FB");

CteServiceInfo_t cteServiceInfo;

void BTCtesCback(tBTA_GATTS_EVT event, tBTA_GATTS* param);

typedef base::Callback<void(uint8_t status, int server_if,
                             std::vector<btgatt_db_element_t> service)>
                            OnCteServiceAdded;

static void OnCteServiceAddedCb(uint8_t status, int serverIf,
                              std::vector<btgatt_db_element_t> service);

const char* bta_cte_event_str(uint32_t event) {
  switch (event) {
      CASE_RETURN_STR(CTES_NONE_EVENT)
      CASE_RETURN_STR(CTES_INIT_EVENT)
      CASE_RETURN_STR(CTES_CLEANUP_EVENT)
      CASE_RETURN_STR(CTES_UPDATE)
      CASE_RETURN_STR(CTES_NOTIFY_ALL)
      CASE_RETURN_STR(CTES_WRITE_RSP)
      CASE_RETURN_STR(CTES_READ_RSP)
      CASE_RETURN_STR(CTES_DESCRIPTOR_WRITE_RSP)
      CASE_RETURN_STR(CTES_DESCRIPTOR_READ_RSP)
      CASE_RETURN_STR(CTES_CONNECTION)
      CASE_RETURN_STR(CTES_DISCONNECTION)
      CASE_RETURN_STR(CTES_CONNECTION_UPDATE)
      CASE_RETURN_STR(CTES_CONGESTION_UPDATE)
      CASE_RETURN_STR(CTES_PHY_UPDATE)
      CASE_RETURN_STR(CTES_MTU_UPDATE)
      CASE_RETURN_STR(CTES_CONNECTION_CLOSE_EVENT)
      CASE_RETURN_STR(CTES_BOND_STATE_CHANGE_EVENT)
    default:
      return (char*)"Unknown bta cte event";
  }
}

class CtesDevices {
 public:
  bool Add(CtesDeviceList device) {
    if (devices.size() == MAX_CTES_CONNECTION) {
      return false;
    }
    if (FindByAddress(device.peer_bda) != nullptr) return false;

    device.DeviceStateHandlerPointer[CTES_DISCONNECTED] = DeviceStateDisconnectedHandlerCtes;
    device.DeviceStateHandlerPointer[CTES_CONNECTED] = DeviceStateConnectionHandlerCtes;

    LOG(INFO) << __func__ <<"add device to list";
    devices.push_back(device);
    return true;
  }

  void Remove(RawAddress& address) {
    for (auto it = devices.begin(); it != devices.end();) {
      if (it->peer_bda != address) {
        ++it;
        continue;
      }
      if (it == devices.end()) {
          LOG(ERROR) << __func__ <<"no matching device";
        return;
      }

      it = devices.erase(it);

      return;
    }
  }

  void RemoveDevices() {
    for (auto it = devices.begin(); it != devices.end();) {
       it = devices.erase(it);
    }
    return;
  }

  CtesDeviceList* FindByAddress(const RawAddress& address) {
    auto iter = std::find_if(devices.begin(), devices.end(),
                             [&address](const CtesDeviceList& device) {
                               return device.peer_bda == address;
                             });

    return (iter == devices.end()) ? nullptr : &(*iter);
  }

  CtesDeviceList* FindByConnId(uint16_t conn_id) {
    auto iter = std::find_if(devices.begin(), devices.end(),
                             [&conn_id](const CtesDeviceList& device) {
                               return device.conn_id == conn_id;
                             });

    return (iter == devices.end()) ? nullptr : &(*iter);
  }

  size_t size() { return (devices.size()); }

  std::vector<CtesDeviceList> GetRemoteDevices() {
    return devices;
  }
  std::vector<CtesDeviceList> devices;
};


class CtesManagerImpl : public CtesManager {
  Uuid app_uuid;
  int max_clients;

  public:
     CtesDevices remoteDevices;
     virtual ~CtesManagerImpl() = default;


  CtesManagerImpl(Uuid uuid, int max_ctes_clients)
        :app_uuid(uuid),
         max_clients(max_ctes_clients) {
    LOG(INFO) << "max_clients " << max_clients;
    LOG(INFO) << "CtesManagerImpl gatts app register";
    BTA_GATTS_AppRegister(app_uuid, BTCtesCback, true);
  }

 void Disconnect(const RawAddress& bd_addr) {
    LOG(INFO) << __func__;
    tCTES_CONNECTION_CLOSE ConnectClosingOp;
    ConnectClosingOp.addr = bd_addr;
    HandleCtesEvent(CTES_CONNECTION_CLOSE_EVENT, &ConnectClosingOp);
 }

 void ConnectionStateCallback(uint8_t state,  const RawAddress&  address) {
    LOG(INFO) << __func__ << " state: " << state << " address: " << address;
 }
};

void CtesManager::CleanUp() {
  HandleCtesEvent(CTES_CLEANUP_EVENT, NULL);
  delete cte_instance;
  cte_instance = nullptr;
 }

CtesManager* CtesManager::Get() {
  CHECK(cte_instance);
  return cte_instance;
}

void CtesManager::Initialize(Uuid uuid, int max_ctes_clients) {
  if (cte_instance) {
  LOG(ERROR) << "Already initialized!";
  } else {
     cte_instance = new CtesManagerImpl(uuid, max_ctes_clients);
  }
}

bool CtesManager::IsCteServiceRunning() { return cte_instance; }

static std::vector<btgatt_db_element_t> CtesAddService(int server_if) {

  std::vector<btgatt_db_element_t> ctes_services;
  ctes_services.clear();
  //service
  btgatt_db_element_t service = {};
  service.uuid = CONSTANT_TONE_EXT_SERVICE_UUID;
  service.type = BTGATT_DB_PRIMARY_SERVICE;
  ctes_services.push_back(service);
  cteServiceInfo.ctes_service_uuid = service.uuid;

  btgatt_db_element_t cte_enable_char = {};
  cte_enable_char.uuid = CONSTANT_TONE_EXT_ENABLE_CHAR_UUID;
  cte_enable_char.type = BTGATT_DB_CHARACTERISTIC;
  cte_enable_char.properties = GATT_CHAR_PROP_BIT_WRITE;
  cte_enable_char.permissions = GATT_PERM_WRITE;
  ctes_services.push_back(cte_enable_char);
  cteServiceInfo.cte_enable_char_uuid = cte_enable_char.uuid;

  return ctes_services;
}

static void OnCteServiceAddedCb(uint8_t status, int serverIf,
                                std::vector<btgatt_db_element_t> service) {

  if (service[0].uuid == Uuid::From16Bit(UUID_SERVCLASS_GATT_SERVER) ||
      service[0].uuid == Uuid::From16Bit(UUID_SERVCLASS_GAP_SERVER)) {
    LOG(ERROR) << "%s: Attempt to register restricted service"<< __func__;
    return;
  }

  for(int i=0; i< (int)service.size(); i++) {
    if (service[i].uuid == CONSTANT_TONE_EXT_SERVICE_UUID) {
      LOG(INFO) << __func__ << " CTE service added attr handle: " << service[i].attribute_handle;
    } else if(service[i].uuid ==  CONSTANT_TONE_EXT_ENABLE_CHAR_UUID) {
      cteServiceInfo.cte_enable_char_handle = service[i++].attribute_handle;
      LOG(INFO) << __func__ << " cte_enable_char_attr: "
                << cteServiceInfo.cte_enable_char_handle;
    }
  }
}

void PrintCtesData(uint8_t data[], uint16_t len) {
  for (int i=0; i<len; i++) {
    LOG(INFO) << __func__ << " data[" << i << "] = " << std::hex << std::setfill('0') << std::setw(2) << data[i] << std::endl;
  }
}

void HandleCtesEvent(uint32_t event, void* param) {
  LOG(INFO) << __func__ << " event: " << bta_cte_event_str(event);
  tBTA_GATTS* p_data = NULL;
  tcte_resp_t *rsp = new tcte_resp_t();
  if (rsp == NULL) {
    LOG(ERROR) << __func__ << " ctes handle return rsp not allocated ";
    return;
  }
  std::vector<uint8_t> _data;
  _data.clear();
  uint8_t status = BT_STATUS_SUCCESS;
  rsp->event = CTES_NONE_EVENT;
  bool isCteOpUsed = false;
  switch (event) {

    case CTES_INIT_EVENT:
    {
      Uuid aap_uuid = bluetooth::Uuid::GetRandom();
      BTA_GATTS_AppRegister(aap_uuid, BTCtesCback, true);
      break;
    }

    case CTES_CLEANUP_EVENT:
    {
      BTA_GATTS_AppDeregister(cteServiceInfo.server_if);
      cte_instance->remoteDevices.RemoveDevices();
      break;
    }
    case BTA_GATTS_REG_EVT:
    {
       p_data = (tBTA_GATTS*)param;
       if (p_data->reg_oper.status == BT_STATUS_SUCCESS) {
           cteServiceInfo.server_if = p_data->reg_oper.server_if;
         std::vector<btgatt_db_element_t> service;
         service = CtesAddService(cteServiceInfo.server_if);
         if (service[0].uuid == Uuid::From16Bit(UUID_SERVCLASS_GATT_SERVER) ||
                 service[0].uuid == Uuid::From16Bit(UUID_SERVCLASS_GAP_SERVER)) {
           LOG(INFO) << __func__ << " service app register uuid is not valid";
           break;
         }
         LOG(INFO) << __func__ << " service app register";
         BTA_GATTS_AddService(cteServiceInfo.server_if, service, base::Bind(&OnCteServiceAddedCb));
       }
       break;
    }

    case BTA_GATTS_DEREG_EVT:
    {
      break;
    }

    case BTA_GATTS_CONF_EVT: {
      p_data = (tBTA_GATTS*)param;
      uint16_t conn_id = p_data->req_data.conn_id;
      uint8_t status = p_data->req_data.status;
      LOG(INFO) << __func__ << "Notify cb for conn_id :" << conn_id;
      if (status == GATT_SUCCESS || status == GATT_CONGESTED) {
        LOG(INFO) << __func__ << "Notification sent successfully ";
      } else {
        LOG(ERROR) << __func__ << "Notification failed with error :" << status;
      }
      GattsOpsQueue::NotificationCallback(conn_id, status);
      break;
    }

    case BTA_GATTS_CONGEST_EVT:
    {
      p_data = (tBTA_GATTS*)param;
      CtesDeviceList *remoteDevice;
      remoteDevice = cte_instance->remoteDevices.FindByConnId(p_data->congest.conn_id);
      if(remoteDevice == NULL) {
        LOG(ERROR) << __func__ << " connection entry not found conn_id: "
          << p_data->congest.conn_id;
        break;
      }
      rsp->remoteDevice = remoteDevice;
      rsp->oper.CongestionOp.congested = p_data->congest.congested;
      rsp->event = CTES_CONGESTION_UPDATE;
      break;
    }
    case BTA_GATTS_MTU_EVT: {
      p_data = (tBTA_GATTS*)param;

      CtesDeviceList *remoteDevice;
      remoteDevice = cte_instance->remoteDevices.FindByConnId(p_data->congest.conn_id);
      if(remoteDevice == NULL) {
        LOG(ERROR) << __func__ << " connection entry not found conn_id: "
          << p_data->congest.conn_id;
        break;
      }
      rsp->event = CTES_MTU_UPDATE;
      rsp->remoteDevice = remoteDevice;
      rsp->oper.MtuOp.mtu = p_data->req_data.p_data->mtu;
      break;
    }
    case BTA_GATTS_CONNECT_EVT: {
      p_data = (tBTA_GATTS*)param;
      LOG(INFO) << __func__ << " remote devices connected";
      CtesDeviceList remoteDevice;
      memset(&remoteDevice, 0, sizeof(remoteDevice));
      if(cte_instance->remoteDevices.FindByAddress(p_data->conn.remote_bda)) {
        LOG(ERROR) << __func__ << " remote devices already there is connected list";
        status = BT_STATUS_FAIL;
        return;
      }
      remoteDevice.peer_bda = p_data->conn.remote_bda;
      remoteDevice.conn_id = p_data->conn.conn_id;
      if(cte_instance->remoteDevices.Add(remoteDevice) == false) {
        LOG(ERROR) << __func__ << " remote device is not added : max connection reached";
        break;
      }
      remoteDevice.state = CTES_DISCONNECTED;

      LOG(INFO) << __func__ << " remote devices connected conn_id: "<< remoteDevice.conn_id <<
         "bd_addr " << remoteDevice.peer_bda;

      rsp->event = CTES_CONNECTION;
      rsp->remoteDevice = cte_instance->remoteDevices.FindByAddress(p_data->conn.remote_bda);
      if (rsp->remoteDevice == NULL) {
          LOG(ERROR)<<__func__ << " remote dev is null";
        break;
      }
      break;
    }

    case BTA_GATTS_DISCONNECT_EVT: {
      p_data = (tBTA_GATTS*)param;
      LOG(INFO) << __func__ << " remote devices disconnected, conn_id:"<<p_data->conn.conn_id;
      CtesDeviceList *remoteDevice;
      remoteDevice = cte_instance->remoteDevices.FindByConnId(p_data->conn.conn_id);
      if((!remoteDevice) ) {
        LOG(ERROR) << __func__ << " remote device not found ";
        status = BT_STATUS_FAIL;
        break;
      }

      rsp->event = CTES_DISCONNECTION;
      rsp->remoteDevice = remoteDevice;
      break;
    }

    case BTA_GATTS_STOP_EVT:
      //Do nothing
      break;

    case BTA_GATTS_DELELTE_EVT:
      //Do nothing
      break;

    case BTA_GATTS_WRITE_CHARACTERISTIC_EVT: {

      p_data = (tBTA_GATTS*)param;
      tGATT_WRITE_REQ req = p_data->req_data.p_data->write_req;
      LOG(INFO) << __func__ << " write characteristics len: " << req.len;
      PrintCtesData(req.value, req.len);
      CtesDeviceList *remoteDevice =
          cte_instance->remoteDevices.FindByConnId(p_data->req_data.conn_id);
      if(remoteDevice == NULL) {
        LOG(ERROR) << __func__ << " device not found ignore write";
        status = BT_STATUS_FAIL;
        break;
      }
      if ( status != BT_STATUS_FAIL) {
        rsp->oper.WriteOp.char_handle = req.handle;
        rsp->oper.WriteOp.trans_id = p_data->req_data.trans_id;
        rsp->oper.WriteOp.status = BT_STATUS_SUCCESS;
        rsp->remoteDevice = remoteDevice;
        rsp->oper.WriteOp.need_rsp = req.need_rsp;
        rsp->oper.WriteOp.offset = req.offset;
        rsp->oper.WriteOp.len = req.len;
        rsp->oper.WriteOp.data = (uint8_t*) malloc(sizeof(uint8_t)*req.len);
        memcpy(rsp->oper.WriteOp.data, req.value, req.len);
        rsp->event = CTES_WRITE_RSP;
      }
      break;
    }

    case BTA_GATTS_CLOSE_EVT: {
       //not required
       break;
    }

    case BTA_GATTS_PHY_UPDATE_EVT: {

      p_data = (tBTA_GATTS*)param;
      CtesDeviceList *remoteDevice =
        cte_instance->remoteDevices.FindByConnId(p_data->phy_update.conn_id);
      if(remoteDevice == NULL) {
        LOG(ERROR) << __func__ << " device not found ignore phy update "
             << p_data->phy_update.status;
        status = BT_STATUS_FAIL;
        break;
      }
      rsp->event = CTES_PHY_UPDATE;
      rsp->oper.PhyOp.rx_phy = p_data->phy_update.rx_phy;
      rsp->oper.PhyOp.tx_phy = p_data->phy_update.tx_phy;
      rsp->remoteDevice = remoteDevice;
      break;
    }

    case BTA_GATTS_CONN_UPDATE_EVT: {
      p_data = (tBTA_GATTS*)param;
      CtesDeviceList *remoteDevice =
        cte_instance->remoteDevices.FindByConnId(p_data->phy_update.conn_id);
      if(remoteDevice == NULL) {
        LOG(ERROR) << __func__ << " connection update device not found";
        break;
      }
      LOG(INFO) << __func__ << " connection update status " << p_data->phy_update.status;
      rsp->event = CTES_CONNECTION_UPDATE;
      rsp->oper.ConnectionUpdateOp.remoteDevice = remoteDevice;
      rsp->oper.ConnectionUpdateOp.remoteDevice->latency = p_data->conn_update.latency;
      rsp->oper.ConnectionUpdateOp.remoteDevice->timeout = p_data->conn_update.timeout;
      rsp->oper.ConnectionUpdateOp.remoteDevice->interval = p_data->conn_update.interval;
      rsp->oper.ConnectionUpdateOp.status = p_data->conn_update.status;
      rsp->remoteDevice = remoteDevice;
      break;
    }

    case CTES_CONNECTION_CLOSE_EVENT:
    {
     break;
    }

    default:
      LOG(INFO) << __func__ << " event not matched !!";
      break;
  }

  if(rsp->event != CTES_NONE_EVENT) {
    CtesHandler(rsp->event, rsp);
  }
  if(rsp) {
    LOG(INFO) << __func__ << "free rsp data";
    free(rsp);
  }
}

void BTCtesCback(tBTA_GATTS_EVT event, tBTA_GATTS* param) {
   HandleCtesEvent((uint32_t)event, param);
}

bool DeviceStateConnectionHandlerCtes(uint32_t event, void* param) {
  LOG(INFO) << __func__ << " device connected handle " << event;
  tcte_resp_t *p_data = (tcte_resp_t *) param;
  switch (event) {
    case CTES_WRITE_RSP: {
      bool need_rsp = p_data->oper.WriteOp.need_rsp;
      LOG(INFO) << __func__ << " device CTES_WRITE_RSP update " << event << " need_rsp: " <<need_rsp;
      if (need_rsp) {
         GattsOpsQueue::SendResponse(p_data->remoteDevice->conn_id, p_data->oper.WriteOp.trans_id,
             BT_STATUS_SUCCESS, &p_data->rsp_value);
      }
      break;
    }

    case CTES_CONNECTION_UPDATE: {
      LOG(INFO) << __func__ << " device cconnection update " << event;
      break;
    }

    case CTES_PHY_UPDATE: {
      LOG(INFO) << __func__ << " device CTES_PHY_UPDATE update " << event;
      p_data->remoteDevice->rx_phy = p_data->oper.PhyOp.rx_phy;
      p_data->remoteDevice->tx_phy = p_data->oper.PhyOp.tx_phy;
      break;
    }

    case CTES_MTU_UPDATE: {
      LOG(INFO) << __func__ << " device CTES_MTU_UPDATE update " << event;
      p_data->remoteDevice->mtu = p_data->oper.MtuOp.mtu;
      break;
    }

    case CTES_CONGESTION_UPDATE: {
      LOG(INFO) << __func__ << ": device CTES_CONGESTION_UPDATE update: " << event;
      CtesCongestionUpdate(p_data);
      break;
    }

    case CTES_DISCONNECTION: {
      LOG(INFO) << __func__ << " device CTES_DISCONNECTION remove " << event;
      cte_instance->remoteDevices.Remove(p_data->remoteDevice->peer_bda);
      cte_instance->ConnectionStateCallback(CTES_DISCONNECTED, p_data->remoteDevice->peer_bda);
      break;
    }

    case CTES_CONNECTION_CLOSE_EVENT: {
      LOG(INFO) << __func__ << " device connection closing";
        // Close active connection
      if (p_data->remoteDevice->conn_id != 0)
        BTA_GATTS_Close(p_data->remoteDevice->conn_id);
      else
        BTA_GATTS_CancelOpen(cteServiceInfo.server_if, p_data->remoteDevice->peer_bda, true);
        // Cancel pending background connections
        BTA_GATTS_CancelOpen(cteServiceInfo.server_if, p_data->remoteDevice->peer_bda, false);
      break;
    }

    default:
      LOG(INFO) << __func__ << " event not matched";
      break;
  }

  return BT_STATUS_SUCCESS;
}

bool DeviceStateDisconnectedHandlerCtes(uint32_t event, void* param) {
  LOG(INFO) << __func__ << " device disconnected handle " << event;
  tcte_resp_t *p_data = (tcte_resp_t *) param;
  switch (event) {
    case CTES_CONNECTION:
    {
      LOG(INFO) << __func__ << " connection processing " << event;
      p_data->remoteDevice->state = CTES_CONNECTED;
      p_data->remoteDevice->congested = false;

      p_data->remoteDevice->timeout = 0;
      p_data->remoteDevice->latency = 0;
      p_data->remoteDevice->interval = 0;
      p_data->remoteDevice->rx_phy = 0;
      p_data->remoteDevice->tx_phy = 0;
      cte_instance->ConnectionStateCallback(CTES_CONNECTED, p_data->remoteDevice->peer_bda);
      break;
    }

    case CTES_CONGESTION_UPDATE: {
      LOG(INFO) << __func__ << ": device CTES_CONGESTION_UPDATE update: " << event;
      CtesCongestionUpdate(p_data);
      break;
    }

    case CTES_MTU_UPDATE:
    case CTES_PHY_UPDATE:
    case CTES_DISCONNECTED:
    case CTES_READ_RSP:
    case CTES_DESCRIPTOR_READ_RSP:
    case CTES_WRITE_RSP:
    case CTES_DESCRIPTOR_WRITE_RSP:
    case CTES_NOTIFY_ALL:
    case CTES_DISCONNECTION:

    default:
      //ignore event
      LOG(INFO) << __func__ << " Ignore event " << event;
      break;
  }
  return BT_STATUS_SUCCESS;
}

void OnConnCteRspEnableComplete(uint8_t* p_data, uint16_t evt_len) {
  uint8_t status;

  STREAM_TO_UINT8(status, p_data);
  LOG(INFO) << __func__ << ": status=" << loghex(status);
}

void OnConnCteTxParamsComplete(uint8_t* p_data, uint16_t evt_len) {
  uint8_t status;
  uint16_t conn_handle;

  STREAM_TO_UINT8(status, p_data);
  LOG(INFO) << __func__ << ": status=" << loghex(status);

  if (status == BT_STATUS_SUCCESS) {
    STREAM_TO_UINT16(conn_handle, p_data);

    tBTM_SEC_DEV_REC* p_dev_rec = btm_find_dev_by_handle(conn_handle);
    if (!p_dev_rec) {
      LOG(ERROR) << __func__ << ": dev rec is NULL";
      return;
    }

    do_in_bta_thread(FROM_HERE, Bind(&BTM_BleSetConnCTERspEnable,
        p_dev_rec->ble.pseudo_addr,
        CTE_RSP_ENABLE, //Enable CTE  response
        Bind(&OnConnCteRspEnableComplete)));
  }
  else {
    LOG(ERROR) << __func__ << ": status failure";
  }
}

bool CtesHandler(uint32_t event, void* param) {
  tcte_resp_t *p_data = (tcte_resp_t *)param;

  CtesDeviceList *device = p_data->remoteDevice;
  LOG(INFO) << __func__ << " ctes handle event "<< bta_cte_event_str(event);
  switch(p_data->event) {
    case CTES_WRITE_RSP: {
      LOG(INFO) << __func__ << " Push Write response first: " << device->state;
      device = p_data->remoteDevice;
      device->DeviceStateHandlerPointer[device->state](p_data->event, p_data);
      //Process the values now
      uint8_t* data = p_data->oper.WriteOp.data;
      int len = p_data->oper.WriteOp.len;
      RawAddress peer_bda = p_data->remoteDevice->peer_bda;
      bool valid_index = false;
      uint8_t indices[10];
      int i;
      uint32_t handle = p_data->oper.WriteOp.char_handle;

      //send HCI cmds for Set CTE TX params followed by CTE rsp enable
      if ((p_data->oper.WriteOp.len > 0) && (p_data->oper.WriteOp.data[0] == 1)) {
        uint8_t cte_type = CTE_RSP_ENABLE; //Allow AoA CTE Response

        uint8_t* antenna_info = (uint8_t*) controller_get_interface()->get_antenna_info_ble()->as_array;
        uint8_t switching_pattern_len = 0;
        std::vector<uint8_t> antenna_ids;
        if (antenna_info) {
          switching_pattern_len = antenna_info[1];
          LOG(INFO) << __func__ << " switching_pattern_len: " << loghex(switching_pattern_len);
          for(int i=0; i<switching_pattern_len; i++) {
            antenna_ids.push_back(i);
          }
        }
        do_in_bta_thread(FROM_HERE, Bind(&BTM_BleSetConnCTETxParams, peer_bda,
                         cte_type, switching_pattern_len, std::move(antenna_ids),
                         Bind(&OnConnCteTxParamsComplete)));
      }
      else {
        //disable send CTE response cmd
        do_in_bta_thread(FROM_HERE, Bind(&BTM_BleSetConnCTERspEnable,
            peer_bda, CTE_RSP_DISABLE /*Disable CTE response*/,
            Bind(&OnConnCteRspEnableComplete)));
      }

      break;
    }

    case CTES_CONGESTION_UPDATE: {
      LOG(INFO) << __func__ << ": device CTES_CONGESTION_UPDATE update: " << event;
      CtesCongestionUpdate(p_data);
      break;
    }

    case CTES_CONNECTION_UPDATE:
    case CTES_PHY_UPDATE:
    case CTES_CONNECTION:
    case CTES_DISCONNECTION:
      LOG(INFO) << __func__ << " calling device state " << device->state;
      device = p_data->remoteDevice;
      device->DeviceStateHandlerPointer[device->state](p_data->event, p_data);
      break;

      default:
      LOG(INFO) << __func__ << " event is not in list";
      break;
    }

  return BT_STATUS_SUCCESS;
}

void CtesCongestionUpdate(tcte_resp_t* p_data) {
  p_data->remoteDevice->congested = p_data->oper.CongestionOp.congested;
  LOG(INFO) << __func__ << ": conn_id: " << p_data->remoteDevice->conn_id
                        << ", congested: " << p_data->remoteDevice->congested;

  GattsOpsQueue::CongestionCallback(p_data->remoteDevice->conn_id,
                                    p_data->remoteDevice->congested);
}
#endif //DIR_FINDING_FEATURE