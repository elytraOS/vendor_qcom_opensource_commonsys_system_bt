/******************************************************************************
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 *******************************************************************************/
#ifdef DIR_FINDING_FEATURE

#pragma once

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bta_gatt_api.h"

using bluetooth::Uuid;
#define MAX_RESPONSE_DATA_LEN 255
#define MAX_CTES_CONNECTION   5

typedef enum {
  CTES_NONE_EVENT = 120,
  CTES_INIT_EVENT,
  CTES_CLEANUP_EVENT,
  CTES_UPDATE,
  CTES_NOTIFY_ALL,
  CTES_WRITE_RSP,
  CTES_READ_RSP,
  CTES_DESCRIPTOR_WRITE_RSP,
  CTES_DESCRIPTOR_READ_RSP,
  CTES_CONNECTION,
  CTES_DISCONNECTION,
  CTES_CONNECTION_UPDATE,
  CTES_CONGESTION_UPDATE,
  CTES_PHY_UPDATE,
  CTES_MTU_UPDATE,
  CTES_SET_ACTIVE_DEVICE,
  CTES_CONNECTION_CLOSE_EVENT,
  CTES_BOND_STATE_CHANGE_EVENT,
}cte_event_t;

typedef enum {
  CTES_STATUS_SUCCESS = 0x00,
  CTES_OPCODE_NOT_SUPPORTED,
  CTES_OPCODE_UNSUCCESSFUL,
}cte_error_t;

typedef enum {
  CTES_DISCONNECTED = 0x00,
  CTES_CONNECTED,
  CTES_MAX_DEVICE_STATE
} ctes_connect_state_t;

 //connection state machine
 bool DeviceStateConnectionHandlerCtes(uint32_t event, void* param);
 bool DeviceStateDisconnectingHandlerCtes(uint32_t event, void* param);
 bool DeviceStateDisconnectedHandlerCtes(uint32_t event, void* param);

typedef struct {
  int server_if;
  Uuid ctes_service_uuid;
  Uuid cte_enable_char_uuid;

  //handle for characteristics
  uint16_t cte_enable_char_handle;
}CteServiceInfo_t;

typedef struct {
  ctes_connect_state_t state;
  bool congested;
  int conn_id;
  int trans_id;
  int timeout;
  int latency;
  int interval;
  int rx_phy;
  int tx_phy;
  int mtu;

  RawAddress peer_bda;
  bool (*DeviceStateHandlerPointer[2])(uint32_t event, void* param);
}CtesDeviceList;

typedef struct {
  RawAddress addr;
}tCTES_CONNECTION_CLOSE;

//Union ops
struct tCTES_CHAR_DESC_WRITE {
  tCTES_CHAR_DESC_WRITE() {};
  ~tCTES_CHAR_DESC_WRITE() {};
  std::vector<uint8_t> value;
  uint8_t status;
  uint16_t notification;
  uint32_t trans_id;
  uint32_t desc_handle;
  uint32_t char_handle;
  bool need_rsp;
  bool prep_rsp;
};

struct tCTES_CHAR_DESC_READ {
  tCTES_CHAR_DESC_READ() {};
  ~tCTES_CHAR_DESC_READ() {};
  uint8_t status;
  uint32_t trans_id;
  uint32_t desc_handle;
  uint32_t char_handle;
};

struct tCTES_CHAR_GATT_READ {
  tCTES_CHAR_GATT_READ() {};
  ~tCTES_CHAR_GATT_READ() {};
  uint8_t status;
  uint32_t trans_id;
  uint32_t char_handle;
};

struct tCTES_CHAR_WRITE {
  tCTES_CHAR_WRITE() {};
  ~tCTES_CHAR_WRITE() {};
  uint8_t status;
  bool need_rsp;
  bool prep_rsp;
  uint16_t offset;
  uint16_t trans_id;
  uint32_t char_handle;
  int len;
  std::vector<uint8_t> value;
  uint8_t *data;
};

struct tCTES_CONNECTION {
  uint8_t status;
  CtesDeviceList remoteDevice;
};

struct tCTES_CONN_UPDATE {
  tCTES_CONN_UPDATE() {};
  ~tCTES_CONN_UPDATE() {};
  uint8_t status;
  CtesDeviceList *remoteDevice;
};

struct tCTES_DISCONNECTION {
  tCTES_DISCONNECTION() {};
  ~tCTES_DISCONNECTION() {};
  uint8_t status;
  CtesDeviceList *remoteDevice;
};

struct tCTES_CONGESTION {
  tCTES_CONGESTION() {};
  ~tCTES_CONGESTION() {};
  bool congested;
  CtesDeviceList *remoteDevice;
};

struct tCTES_PHY{
  tCTES_PHY();
  ~tCTES_PHY();
  uint8_t status;
  uint8_t tx_phy;
  uint8_t rx_phy;
  CtesDeviceList *remoteDevice;
};

struct tCTES_MTU {
  tCTES_MTU() {};
  ~tCTES_MTU() {};
  uint8_t status;
  uint16_t mtu;
  CtesDeviceList *remoteDevice;
};

union CTES_OPERATION{
  CTES_OPERATION() {};
  ~CTES_OPERATION() {};
  tCTES_CHAR_DESC_WRITE WriteDescOp;
  tCTES_CHAR_DESC_READ ReadDescOp;
  tCTES_CHAR_WRITE WriteOp;
  tCTES_CHAR_GATT_READ ReadOp;
  tCTES_CONNECTION ConnectionOp;
  tCTES_CONN_UPDATE ConnectionUpdateOp;
  tCTES_DISCONNECTION DisconnectionOp;
  tCTES_CONGESTION CongestionOp;
  tCTES_MTU MtuOp;
  tCTES_PHY PhyOp;
};

typedef union CTES_OPERATION tCTES_OPERATION;

struct tcte_resp_t {
  tcte_resp_t()    {};
  ~tcte_resp_t()    {};
  uint32_t event = 0;
  uint16_t handle = 0;
  uint16_t status = 0;
  bool force = false;
  CtesDeviceList *remoteDevice = nullptr;
  tGATTS_RSP rsp_value;
  tCTES_OPERATION oper;
 };

class CtesManager {

  public:
  virtual ~CtesManager() = default;
  static void Initialize(Uuid app_id, int max_ctes_clients);
  static void CleanUp();
  static CtesManager* Get();
  static bool IsCteServiceRunning();

  virtual void Disconnect(const RawAddress& bd_add) = 0;
};

void HandleCtesEvent(uint32_t event, void* param);
bool CtesHandler(uint32_t event, void* param);
void CtesCongestionUpdate(tcte_resp_t * p_data);
#endif // DIR_FINDING_FEATURE
