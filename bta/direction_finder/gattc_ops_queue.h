/******************************************************************************
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 *******************************************************************************/
#ifdef DIR_FINDING_FEATURE

#pragma once

#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include "bta_gatt_api.h"

typedef void (*BTA_GATT_READ_OP_CB)(uint16_t client_id,uint16_t conn_id,
                                tGATT_STATUS status, uint16_t handle,
                                uint16_t len, uint8_t* value,
                                void* data);

typedef void (*BTA_GATT_WRITE_OP_CB)(uint16_t client_id, uint16_t conn_id,
                                     tGATT_STATUS status,
                                     uint16_t handle, void* data);

/* BTA GATTC implementation does not allow for multiple commands queuing. So one
 * client making calls to BTA_GATTC_ReadCharacteristic, BTA_GATTC_ReadCharDescr,
 * BTA_GATTC_WriteCharValue, BTA_GATTC_WriteCharDescr must wait for the callacks
 * before scheduling next operation.
 *
 * Methods below can be used as replacement to BTA_GATTC_* in BTA app. They do
 * queue the commands if another command is currently being executed.
 *
 * If you decide to use those methods in your app, make sure to not mix it with
 * existing BTA_GATTC_* API.
 */

namespace bluetooth {
namespace directionfinder {

class GattOpsQueue {
 public:
  static void Clean(uint16_t conn_id);
  static void ReadCharacteristic(uint16_t client_id,
                           uint16_t conn_id, uint16_t handle,
                           BTA_GATT_READ_OP_CB cb, void* cb_data);
  static void ReadDescriptor(uint16_t client_id,
                       uint16_t conn_id, uint16_t handle,
                       BTA_GATT_READ_OP_CB cb, void* cb_data);
  static void WriteCharacteristic(uint16_t client_id,
                            uint16_t conn_id, uint16_t handle,
                            std::vector<uint8_t> value,
                            tGATT_WRITE_TYPE write_type,
                            BTA_GATT_WRITE_OP_CB cb, void* cb_data);
  static void WriteDescriptor(uint16_t client_id,
                        uint16_t conn_id, uint16_t handle,
                        std::vector<uint8_t> value,
                        tGATT_WRITE_TYPE write_type, BTA_GATT_WRITE_OP_CB cb,
                        void* cb_data);
  static void ServiceSearch(uint16_t client_id,
                            uint16_t conn_id, Uuid* p_srvc_uuid);

  static uint16_t ServiceSearchComplete(uint16_t conn_id, tGATT_STATUS status);

  static void CongestionCallback(uint16_t conn_id, bool congested);


  /* Holds pending GATT operations */
  struct gatt_operation {
    uint8_t type;
    uint16_t client_id;
    uint16_t handle;
    BTA_GATT_READ_OP_CB read_cb;
    void* read_cb_data;
    BTA_GATT_WRITE_OP_CB write_cb;
    void* write_cb_data;

    /* write-specific fields */
    tGATT_WRITE_TYPE write_type;
    std::vector<uint8_t> value;

    /* discovery specific */
    Uuid* p_srvc_uuid;
  };

 private:
  static bool is_congested;

  static void mark_as_not_executing(uint16_t conn_id);
  static void gatt_execute_next_op(uint16_t conn_id);
  static void gatt_read_op_finished(uint16_t conn_id, tGATT_STATUS status,
                              uint16_t handle, uint16_t len,
                              uint8_t* value, void* data);
  static void gatt_write_op_finished(uint16_t conn_id, tGATT_STATUS status,
                                      uint16_t handle, uint16_t len,
                                      const uint8_t* value, void* data);

  // maps connection id to operations waiting for execution
  static std::unordered_map<uint16_t, std::list<gatt_operation>> gatt_op_queue;

  // maps connection id to congestion status of each device
  static std::unordered_map<uint16_t, bool> congestion_queue;

  // contain connection ids that currently execute operations
  static std::unordered_set<uint16_t> gatt_op_queue_executing;
};

}  // namespace directionfinder
}  // namespace bluetooth
#endif // DIR_FINDING_FEATURE