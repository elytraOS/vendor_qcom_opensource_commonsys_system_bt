/******************************************************************************
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 *******************************************************************************/

/* Asset Tracking Profile Interface */
#ifdef DIR_FINDING_FEATURE

#include "bt_target.h"
#include "bta_closure_api.h"
#include "bta_atp_locator_api.h"
#include "btif_api.h"
#include "btif_common.h"
#include "btif_storage.h"

#include <base/bind.h>
#include <base/callback.h>
#include <base/location.h>
#include <base/logging.h>
#include <bluetooth/uuid.h>
#include <hardware/bluetooth.h>
#include <hardware/bt_atp_locator.h>
#include "bta_ctes_api.h"

using base::Bind;
using base::Unretained;
using bluetooth::Uuid;
using bluetooth::atp_locator::ConnectionState;
using bluetooth::atp_locator::AtpLocatorCallbacks;
using bluetooth::atp_locator::AtpLocatorInterface;

namespace {
class AtpLocatorInterfaceImpl;
std::unique_ptr<AtpLocatorInterface> AtpLocatorInstance;

class AtpLocatorInterfaceImpl
    : public AtpLocatorInterface, public AtpLocatorCallbacks {
  ~AtpLocatorInterfaceImpl() = default;

  void Init(AtpLocatorCallbacks* callbacks) override {
    LOG(INFO) << __func__ ;
    this->callbacks = callbacks;

    do_in_bta_thread(
        FROM_HERE,
        Bind(&AtpLocator::Initialize, this));

    //Initialize CTES server
    do_in_bta_thread(
        FROM_HERE,
        Bind(&CtesManager::Initialize, Uuid::FromString("0000184A-0000-1000-8000-00805F9B34FB"), 10));
  }

  void OnConnectionState(ConnectionState state,
                         const RawAddress& address) override {
    LOG(INFO) << __func__ << ": device=" << address << " state=" << (int)state;
    do_in_jni_thread(FROM_HERE, Bind(&AtpLocatorCallbacks::OnConnectionState,
                                     Unretained(callbacks), state, address));
  }

  void OnEnableBleDirectionFinding(uint8_t status, const RawAddress& address) override {
      LOG(INFO) << __func__ << ": device=" << address << " status=" << loghex(status);
      do_in_jni_thread(FROM_HERE, Bind(&AtpLocatorCallbacks::OnEnableBleDirectionFinding,
                                       Unretained(callbacks), status, address));
  }

  void OnLeAoaResults(uint8_t status, double azimuth, uint8_t azimuth_unc,
      double elevation, uint8_t elevation_unc, const RawAddress& address) override {
    LOG(INFO) << __func__ << ": device=" << address << " status=" << status << " azimuth=" << azimuth
              << " elevation=" << elevation;
    do_in_jni_thread(FROM_HERE, Bind(&AtpLocatorCallbacks::OnLeAoaResults,
                                     Unretained(callbacks), status, azimuth, azimuth_unc,
                                     elevation, elevation_unc, address));
  }

  void Connect(const RawAddress& address, bool isDirect) override {
    LOG(INFO) << __func__ << ": device=" << address;
    do_in_bta_thread(FROM_HERE, Bind(&AtpLocator::Connect,
                                      Unretained(AtpLocator::Get()), address, isDirect));
  }

  void Disconnect(const RawAddress& address) override {
    LOG(INFO) << __func__ << ": device=" << address;
    do_in_bta_thread(FROM_HERE, Bind(&AtpLocator::Disconnect,
                                      Unretained(AtpLocator::Get()), address));
  }

  void EnableBleDirectionFinding(uint8_t sampling_enable, uint8_t slot_durations,
      uint8_t enable, uint16_t cte_req_int, uint8_t req_cte_len,
      uint8_t dir_finding_type, const RawAddress& address) override {
    LOG(INFO) << __func__ << ": device=" << address;
    do_in_bta_thread(FROM_HERE, Bind(&AtpLocator::EnableBleDirectionFinding,
                                     Unretained(AtpLocator::Get()), sampling_enable, slot_durations,
                                     enable, cte_req_int, req_cte_len, dir_finding_type, address));
  }

  void Cleanup(void) override {
    LOG(INFO) << __func__;
    do_in_bta_thread(FROM_HERE, Bind(&AtpLocator::CleanUp));
  }

 private:
  AtpLocatorCallbacks* callbacks;
};

}  // namespace

AtpLocatorInterface* btif_atp_locator_get_interface() {
  LOG(INFO) << __func__;
  if (!AtpLocatorInstance)
    AtpLocatorInstance.reset(new AtpLocatorInterfaceImpl());

  return AtpLocatorInstance.get();
}
#endif // DIR_FINDING_FEATURE