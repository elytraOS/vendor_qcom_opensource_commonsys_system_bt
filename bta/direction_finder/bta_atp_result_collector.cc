/******************************************************************************
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 *******************************************************************************/
#ifdef DIR_FINDING_FEATURE

#include <string>
#include <list>
#include "bt_trace.h"
#include "stack/include/btm_ble_direction_finder_api_types.h"
#include "stack/include/btm_ble_direction_finder_api.h"
#include "bta_atp_locator_api.h"
#include "stack/btm/btm_int_types.h"
#include "stack/include/hcidefs.h"
#include "btm_int.h"
#include "bta_atp_result_collector.h"

bool gAoaAlgoInitialized = false;
#ifdef AOA_ALGO_INT
AOA_STATUS (*gAoaIfInitHandle)(Int32, AoAConfig*);
AOA_STATUS (*gAoaIfAddMeasurement)(Int32, AoARawPacket*);
AOA_STATUS (*gAoaIfEstimateAngle)(Int32, Uint64);
AOA_STATUS (*gAoaIfDeinitHandle)(Int32);
#endif

namespace bluetooth {
namespace atplocator {

std::map<RawAddress, int> gDeviceToLinkRefMap;
std::map<int, RawAddress> gLinkRefToDeviceMap;
std::map<int, int> gLinkRefToCounterMap;
std::map<RawAddress, int> gDeviceToEventCntMap;
#define AOA_ALGO_LINKREF_BEG 0
/*
 * AoA Results collector
**/
#ifdef AOA_ALGO_INT
void AoaIfAddMeasurementCb(AOA_STATUS status, Int32 linkRef,
                           Uint64 arrival_time) {
  LOG(INFO) << __func__ << ": gAoaIfAddMeasurementCb : linkref: " << linkRef
            << " arrival_time: " << arrival_time << " status:" << status;
  AtpLocator *atpLocatorHandle = AtpLocator::Get();
  int counter = gLinkRefToCounterMap[linkRef];
  if (counter == 10) {
    counter = 1;
    AoaResultsCollector *aoaRc = atpLocatorHandle->FindOrCreateAoaResultsCollector(GetAddressFromLinkRefrence(linkRef), nullptr);
    aoaRc->GetAngleEstimation(arrival_time);

    gLinkRefToCounterMap[linkRef] = counter;
  }
  else {
    gLinkRefToCounterMap[linkRef] = ++counter;
  }
}

void AoaIfAngleEstimateCb(AOA_STATUS status, Int32 linkRef,
                          Uint64 arrival_time, AoAResult* aoa_result) {
  LOG(INFO) << __func__ << ": AoaIfAngleEstimateCb : linkref: " << linkRef
            << " arrival_time: " << arrival_time;
  AtpLocator *atpLocatorHandle = AtpLocator::Get();
  if (atpLocatorHandle == nullptr) {
    LOG(ERROR) << __func__ << ": Not able to AtpLocator Handle ";
    return;
  } else {
    RawAddress bda = GetAddressFromLinkRefrence(linkRef);
    if (aoa_result) {
      atpLocatorHandle->OnLeAoaResults(bda, status, aoa_result->azimuth, aoa_result->azimuthUnc,
                                       aoa_result->elevation, aoa_result->elevationUnc);
    }
  }
}
#endif

uint8_t GetSwitchingPatternLength() {
  LOG(INFO) << __func__;
  AtpLocator *atpLocatorHandle = AtpLocator::Get();
  return(atpLocatorHandle->GetSwitchingPatternLen());
}

uint8_t* GetSwitchingPattern() {
  LOG(INFO) << __func__;
  AtpLocator *atpLocatorHandle = AtpLocator::Get();
  return(atpLocatorHandle->GetSwitchingPattern());
}

int GetEventCntFromAddress(RawAddress address) {
  return gDeviceToEventCntMap[address];
}

void SetEventCntForAddress(RawAddress address, int eventCnt) {
  gDeviceToEventCntMap[address] = eventCnt;
}

RawAddress GetAddressFromLinkRefrence(int linkRef) {
  return gLinkRefToDeviceMap[linkRef];
}

int GetLinkReference(RawAddress addr) {
  int linkRef = -1;
  if (gDeviceToLinkRefMap.find(addr) == gDeviceToLinkRefMap.end()) {
    int curMaxLinkRef = AOA_ALGO_LINKREF_BEG;
    for (auto i = gDeviceToLinkRefMap.begin(); i != gDeviceToLinkRefMap.end(); i++) {
      curMaxLinkRef = gDeviceToLinkRefMap[i->first];
    }
    linkRef = curMaxLinkRef + 1;
    gDeviceToLinkRefMap[addr] = linkRef;
    gLinkRefToDeviceMap[linkRef] = addr;
    gLinkRefToCounterMap[linkRef] = 0;
    gDeviceToEventCntMap[addr] = 0;
  } else {
    linkRef = gDeviceToLinkRefMap[addr];
  }
  LOG(INFO) << __func__ << ": returning linkRef " << linkRef;
  return linkRef;
}

#ifdef AOA_ALGO_INT
void InitializeAlgoHandles() {
  if (gAoaAlgoInitialized == false) {
    void *libHandle = nullptr;

    if (!libHandle) {
      LOG(ERROR) << __func__ << ": dlopen error aoa algo: ";
      return;
    }

    *(void**)(&gAoaIfInitHandle) = dlsym(libHandle, "aoaIfInit");

    if (!gAoaIfInitHandle) {
      LOG(ERROR) << __func__ << ": dlopen error gAoaIfInitHandle: ";
      return;
    }

    *(void**)(&gAoaIfAddMeasurement) = dlsym(libHandle, "aoaIfAddMeasurement");

    if (!gAoaIfAddMeasurement) {
      LOG(ERROR) << __func__ << ": dlopen error gAoaIfAddMeasurement: ";
      return;
    }

    *(void**)(&gAoaIfEstimateAngle) = dlsym(libHandle, "aoaIfEstimateAngle");

    if (!gAoaIfEstimateAngle) {
      LOG(ERROR) << __func__ << ": dlopen error gAoaIfEstimateAngle: ";
      return;
    }

    *(void**)(&gAoaIfDeinitHandle) = dlsym(libHandle, "aoaIfDeinit");

    if (!gAoaIfDeinitHandle) {
      LOG(ERROR) << __func__ << ": dlopen error gAoaIfDeinitHandle: ";
      return;
    }

    //dlclose(libHandle);
    gAoaAlgoInitialized = true;
    LOG(INFO) << __func__ << ": SUCCESS ";
  }
}
#endif
}  // namespace atplocator
}  // namespace bluetooth
#endif //DIR_FINDING_FEATURE