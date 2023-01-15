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
namespace bluetooth {
namespace directionfinder {

class GattsOpsQueue {
 public:
  static void Clean(uint16_t conn_id);
  static void SendNotification(uint16_t conn_id, uint16_t handle,
                               std::vector<uint8_t> value, bool need_confirm);
  static void SendResponse(uint16_t conn_id, uint32_t trans_id,
                               uint8_t status, tGATTS_RSP* rsp_value);
  static void NotificationCallback(uint16_t conn_id, uint8_t status);
  static void CongestionCallback(uint16_t conn_id, bool congested);

  /* Holds pending GATT operations */
  struct gatts_operation {
        uint8_t type;
        /* For enqueue Notification/Indication */
        uint16_t attr_id;
        std::vector<uint8_t> value;
        bool need_confirm;
        /* For enqueue read responses OR write responses */
        uint32_t trans_id;
        uint8_t status;
        tGATTS_RSP* rsp_value;
  };

 private:
  static bool is_congested;
  static void mark_as_not_executing(uint16_t conn_id);
  static void gatts_execute_next_op(uint16_t conn_id);

  // maps connection id to operations waiting for execution
  static std::unordered_map<uint16_t, std::list<gatts_operation>> gatts_op_queue;

  // maps connection id to congestion status of each device
  static std::unordered_map<uint16_t, bool> congestion_queue;

  // contain connection ids that currently execute operations
  static std::unordered_set<uint16_t> gatts_op_queue_executing;

}; // Class GattsOpsQueue ends

}
} // namespace ends
#endif // DIR_FINDING_FEATURE