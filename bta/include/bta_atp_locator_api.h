/******************************************************************************
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 *******************************************************************************/
#ifdef DIR_FINDING_FEATURE

#pragma once

#include <base/callback_forward.h>
#include "hardware/bt_atp_locator.h"
#include "bta_atp_result_collector.h"
#include <deque>
#include <future>
#include <vector>

#define ATP_AOA_CTE_ENABLE 0x01
#define ATP_AOA_CTE_DISABLE 0x0

using bluetooth::atplocator::AoaResultsCollector;
class AtpLocatorImpl;

enum {
  BTA_ATP_DISCONNECTED = 0x0,
  BTA_ATP_CONNECTING,
  BTA_ATP_CONNECTED,
  BTA_ATP_DISCONNECTING,
};

enum {
  ATP_CTE_ENABLE_READ_CMPL_EVT = 0x0,
  ATP_CTE_ENABLE_WRITE_CMPL_EVT,
};

struct AtpLocatorService {
  uint16_t cte_enable_handle;

  //Set CTE Receive params
  uint8_t sampling_enable;
  uint8_t slot_durations;

  //CTE Request enable params
  uint8_t cte_enable;
  uint16_t cte_req_int;
  uint8_t req_cte_len;
  uint8_t dir_finding_type;

  bool is_disconnect_issued;

  AtpLocatorService()
      : cte_enable_handle(0),
        sampling_enable(0),
        slot_durations(0),
        cte_enable(0),
        cte_req_int(0),
        req_cte_len(0),
        dir_finding_type(0),
        is_disconnect_issued(false){}
};

struct AoaDevice {
  RawAddress address;
  uint16_t conn_id;
  uint8_t state;
  bool is_cte_enabled;
  bool bg_conn;
  bool service_changed_rcvd;
  AtpLocatorService atp_locator_service;

  AoaDevice(const RawAddress& address)
      : address(address),
        conn_id(0),
        state(BTA_ATP_DISCONNECTED),
        is_cte_enabled(false),
        bg_conn(false),
        service_changed_rcvd(false),
        atp_locator_service() {}

  AoaDevice() : AoaDevice(RawAddress::kEmpty) {}
};

class AtpLocator {
  public:
  virtual ~AtpLocator() = default;

  static void Initialize(bluetooth::atp_locator::AtpLocatorCallbacks* callbacks);
  static void CleanUp();
  static AtpLocator* Get();
  static bool IsAtpLocatorRunning();
  static int GetDeviceCount();

  virtual void Connect(const RawAddress& address, bool isDirect) = 0;
  virtual void Disconnect(const RawAddress& address) = 0;
  virtual void EnableBleDirectionFinding(uint8_t sampling_enable,
          uint8_t slot_durations, uint8_t enable, uint16_t cte_req_int,
          uint8_t req_cte_len, uint8_t dir_finding_type, const RawAddress& address) = 0;
  virtual AoaResultsCollector* FindOrCreateAoaResultsCollector(const RawAddress& address,
          AtpLocatorImpl *atpLocatorImpl) = 0;
  virtual void OnLeAoaResults(RawAddress& address, int status, double azimuth, uint8_t azimuth_unc,
          double elevation, uint8_t elevation_unc) = 0;
  virtual uint8_t GetSwitchingPatternLen() = 0;
  virtual uint8_t* GetSwitchingPattern() = 0;

  std::map<RawAddress, AoaResultsCollector*> mAoaResultCollectors;
};
#endif // DIR_FINDING_FEATURE