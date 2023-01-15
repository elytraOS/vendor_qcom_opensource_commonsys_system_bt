/******************************************************************************
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 *******************************************************************************/
#ifdef DIR_FINDING_FEATURE

#ifndef ATP_RESULTS_COLLECTOR_H
#define ATP_RESULTS_COLLECTOR_H

#include <string>
#include <fstream>
#include <iomanip>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <list>
#include <base/strings/string_number_conversions.h>
#include "bt_trace.h"
#include "bta_atp_locator_api.h"
#include "stack/include/btm_ble_direction_finder_api_types.h"
#include "stack/include/btm_ble_direction_finder_api.h"
#include "stack/btm/btm_int_types.h"
#include "stack/include/hcidefs.h"
#include "btm_int.h"
#include <dlfcn.h>
#include "osi/include/properties.h"
#include "device/include/controller.h"
#ifdef AOA_ALGO_INT
#include <aoa_if.h>
#endif

#define AOA_ALGO_INPUT_PATH_PREFIX "/data/misc/bluedroid/aoa_algo_input_"
extern bool gAoaAlgoInitialized;
#ifdef AOA_ALGO_INT
extern AOA_STATUS (*gAoaIfInitHandle)(Int32, AoAConfig*);
extern AOA_STATUS (*gAoaIfAddMeasurement)(Int32, AoARawPacket*);
extern AOA_STATUS (*gAoaIfEstimateAngle)(Int32, Uint64);
extern AOA_STATUS (*gAoaIfDeinitHandle)(Int32);
#endif

class AtpLocatorImpl;

namespace bluetooth {
namespace atplocator {

extern int GetLinkReference(RawAddress addr);
extern RawAddress GetAddressFromLinkRefrence(int linkRef);
extern int GetEventCntFromAddress(RawAddress address);
extern void SetEventCntForAddress(RawAddress address, int eventCnt);
extern void InitializeAlgoHandles();

extern void AoaIfAddMeasurementCb(AOA_STATUS, Int32, Uint64);
extern void AoaIfAngleEstimateCb(AOA_STATUS, Int32, Uint64, AoAResult*);

extern uint8_t GetSwitchingPatternLength();
extern uint8_t* GetSwitchingPattern();

#define AOA_ALGO_LIB_PATH "/system_ext/lib64/libaoainterface.so"

/*
 * AoA Result collector
**/

class AoaResultsCollector {
 public:
  RawAddress GetAddress() {
    return m_bd_addrs;
  }

  AtpLocatorImpl* GetAtpLocatorHandle() {
    return mAtpLocatorImpl;
  }

  uint16_t GetAclHandle() {
    return m_acl_hdl;
  }

  void SetAclHandle(uint16_t handle) {
    m_acl_hdl = handle;
  }

#ifdef AOA_ALGO_INT
int getCurrentTime(char buffer[]) {
  const int buffersize = 31;
  const int tempsize = 21;
  struct timespec now;
  struct tm tm;
  int retval = clock_gettime(CLOCK_REALTIME, &now);
  gmtime_r(&now.tv_sec, &tm);
  strftime(buffer, tempsize, "%Y%m%dT%H%M%S_", &tm);
  snprintf(buffer + tempsize -1, (buffersize-tempsize), "%09luZ", now.tv_nsec);
  return retval;
}

void DumpAoaAlgoInputsToFile(std::vector<uint8_t> connIqRptEvtResult, std::string algo_input_str) {
  LOG(INFO) << __func__ << ": Dumping HCI LE Conn IQ Report event results to a file : ";
  int i=0;
  char fname[255] = {0};
  char timebuffer[31];
  std::string fileName;
  getCurrentTime(timebuffer);
  std::vector<uint8_t> i_samples_vec;
  std::vector<uint8_t> q_samples_vec;
  LOG(INFO) << __func__ << ": Address: " << (loghex(m_bd_addrs.address[0]))
            << ":" << loghex(m_bd_addrs.address[1]) << ":" << loghex(m_bd_addrs.address[2])
            << ":" << loghex(m_bd_addrs.address[3]) << ":" << loghex(m_bd_addrs.address[4])
            << ":" << loghex(m_bd_addrs.address[5]);
  snprintf(fname, sizeof(fname), "%s%02x%02x%02x%02x%02x%02x", AOA_ALGO_INPUT_PATH_PREFIX,
           m_bd_addrs.address[0], m_bd_addrs.address[1], m_bd_addrs.address[2], m_bd_addrs.address[3],
           m_bd_addrs.address[4], m_bd_addrs.address[5]);
  fileName = fname;
  fileName.append(".txt");
  LOG(INFO) << __func__ << ": FileName : " << fileName;
  std::ofstream out(fileName, std::ios::out | std::ios::app);

  int eventCnt = GetEventCntFromAddress(m_bd_addrs);
  if (eventCnt == 0) {
    SetEventCntForAddress(m_bd_addrs, 1);
  } else {
    SetEventCntForAddress(m_bd_addrs, ++eventCnt);
  }

  if (((GetEventCntFromAddress(m_bd_addrs) - 1) % 10) == 0) {
    out << " ======================================================================" << std::endl;
    out << " ======================================================================" << std::endl;
  }
  if (connIqRptEvtResult.size() > 0) {
    out << " HCI event number: [" << GetEventCntFromAddress(m_bd_addrs) << "]"<< std::endl;
    LOG(INFO) << __func__ << ": Timestamp:" <<timebuffer << GetEventCntFromAddress(m_bd_addrs);
    out << " Timestamp: " << timebuffer << GetEventCntFromAddress(m_bd_addrs) << std::endl;
    out << " Algo input bytes: [" << algo_input_str << "]"<< std::endl;
    out << " HCI event:" << loghex(connIqRptEvtResult[i]) << std::endl;
    out << " BLE HCI event opcode:" << loghex(connIqRptEvtResult[++i]) << std::endl;
    out << " HCI event length:" << loghex(connIqRptEvtResult[++i]) << std::endl;
    out << " BLE subevent code:" << loghex(connIqRptEvtResult[++i]) << std::endl;
    uint16_t conn_handle = (connIqRptEvtResult[++i] + (connIqRptEvtResult[++i] << 8));
    out << " Connection handle:" << loghex(conn_handle) << std::endl;
    out << " RX PHY:" << loghex(connIqRptEvtResult[++i]) << std::endl;
    out << " Data Channel Index:" << loghex(connIqRptEvtResult[++i]) << std::endl;
    uint16_t rssi = (connIqRptEvtResult[++i] + (connIqRptEvtResult[++i] << 8));
    out << " RSSI:" << loghex(rssi) << std::endl;
    out << " RSSI Antenna ID:" << loghex(connIqRptEvtResult[++i]) << std::endl;
    out << " CTE Type:" << loghex(connIqRptEvtResult[++i]) << std::endl;
    out << " Slot Durations:" << loghex(connIqRptEvtResult[++i]) << std::endl;
    out << " Packet Status:" << loghex(connIqRptEvtResult[++i]) << std::endl;
    uint16_t conn_event_counter = ((connIqRptEvtResult[++i]) + (connIqRptEvtResult[++i] << 8));
    out << " Connection event counter:" << loghex(conn_event_counter) << std::endl;
    uint8_t sample_count = connIqRptEvtResult[++i];
    out << " Sample Count:" << loghex(sample_count) << std::endl;
    if (sample_count > 0) {
      for (int j=0; j<sample_count; j++) {
        i_samples_vec.push_back(connIqRptEvtResult[++i]);
        q_samples_vec.push_back(connIqRptEvtResult[++i]);
      }
      out << " I Samples: [";
      for (int k=0; k < (int)i_samples_vec.size(); k++) {
        out << loghex(i_samples_vec[k]);
        if (k < (sample_count-1)) {
          out << ",";
        }
      }
      out << "]" << std::endl;

      out << " Q Samples: [";
      for (int k=0; k < (int)q_samples_vec.size(); k++) {
        out << loghex(q_samples_vec[k]);
        if (k < (sample_count-1)) {
          out << ",";
        }
      }
      out << "]" << std::endl;
    }
  }
  out << " ======================================================================" << std::endl;
  out.close();
}

Uint64 timeInMilliseconds(void) {
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return (((Uint64)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

void convertHciEvtResultToAlgoInput(uint8_t* p, uint16_t evt_len) {
  LOG(INFO) << __func__;
  uint8_t algo_input_arr[evt_len+3]; //HCI evt + opcode(3E) + len + subevt code
  uint64_t arrival_time = timeInMilliseconds();
  uint16_t data_len = evt_len+3;
  uint8_t* p_algo_input = (uint8_t*) osi_malloc(data_len);
  std::string algo_input_str = "";
  char input_str[255] = {0};

  p_algo_input[0] = 4; //HCI event
  p_algo_input[1] = 0x3E; //BLE HCI event code
  p_algo_input[2] = evt_len; //Length
  p_algo_input[3] = 0x16; //BLE subevent code, LE Conn IQ Report event

  memcpy(&p_algo_input[4], p, evt_len);
  std::vector<uint8_t> values(p_algo_input, p_algo_input + (evt_len + 3));

  if (data_len > 0) {
    for (int i=0; i< data_len; i++) {
      snprintf(input_str, sizeof(input_str), "%02x", p_algo_input[i]);
      algo_input_str.append(input_str);
      if (i<data_len-1) {
        algo_input_str.append(",");
      }
    }
  }
  LOG(INFO) << __func__ << " Printing HCI LE Conn IQ Report event results : ";
  LOG(INFO) << __func__ << algo_input_str.c_str();
  DumpAoaAlgoInputsToFile(values, algo_input_str);

  UpdateAoaAlgoMeasurement(arrival_time, data_len, p_algo_input);
}

void InitializeAoaAlgo() {
  LOG(INFO) << __func__;

  struct stat info;
  const char filename[] = "/system_ext/lib64/calibration_data.bin";
  FILE *calib_file_handler = fopen(filename, "rb");
  char *endptr;
  uint8_t switching_pattern_len = 0;
  uint8_t* antenna_ids;

  if(calib_file_handler == NULL) {
    LOG(ERROR) << __func__ << "Unable to read calibration file at: %s" << filename;
    return;
  }

  if (stat(filename, &info) == -1) {
    LOG(ERROR) << __func__ << "Unable to read stat info of cal file at: %s" << filename;
    fclose(calib_file_handler);
    return;
  } else {
    LOG(INFO) << __func__ << "Calibration File size is %d" << info.st_size;
  }

  uint8_t* cal_byte_arr = (uint8_t*) malloc(info.st_size);
  fread(cal_byte_arr, sizeof(uint8_t), info.st_size/sizeof(uint8_t), calib_file_handler);

  switching_pattern_len = GetSwitchingPatternLength();
  LOG(INFO) << __func__ << ": Switching pattern len:" << loghex(switching_pattern_len);
  antenna_ids = GetSwitchingPattern();
  for(int i=0; i<switching_pattern_len; i++) {
     LOG(INFO) << __func__ << ":" << loghex(antenna_ids[i]);
  }
  mAlgoAoaConfig.nSwitchSeq = switching_pattern_len;
  mAlgoAoaConfig.switchingSeq = &antenna_ids[0];

  mAlgoAoaConfig.firstSwitch = 11.0;
  mAlgoAoaConfig.guardTime = 0.9;
  mAlgoAoaConfig.nPacketsPerChannel = 5;
  mAlgoAoaConfig.sampleHistory = 400;
  mAlgoAoaConfig.preProcLevel = PRE_PROC_LEVEL_SCATTER; //1
  mAlgoAoaConfig.calibration_length = info.st_size/sizeof(uint8_t);
  mAlgoAoaConfig.calibration_data = cal_byte_arr;
  mAlgoAoaConfig.preQuadScale = PRE_QUAD_TERM_SCALE_EACH; //2
  mAlgoAoaConfig.measurementCallBack = AoaIfAddMeasurementCb;
  mAlgoAoaConfig.angleEstimateCallback = AoaIfAngleEstimateCb;
#ifdef AOA_ALGO_DL
  InitializeAlgoHandles();
  int linkRef = GetLinkReference(m_bd_addrs);
  if (gAoaAlgoInitialized) {
    gAoaIfInitHandle((Int32)linkRef, &mAlgoAoaConfig);
  }
#endif
}

void UpdateAoaAlgoMeasurement(uint64_t arrival_time, uint16_t data_length, uint8_t* p_data) {
  LOG(INFO) << __func__;
  AoARawPacket aoa_raw_pkt;
  aoa_raw_pkt.arrivalTime = arrival_time;
  aoa_raw_pkt.dataLen = data_length;
  aoa_raw_pkt.data = p_data;

#ifdef AOA_ALGO_DL
  InitializeAlgoHandles();
  if (gAoaAlgoInitialized) {
    gAoaIfAddMeasurement(GetLinkReference(m_bd_addrs), &aoa_raw_pkt);
  }
#endif
}

void GetAngleEstimation(uint64_t request_time) {
  LOG(INFO) << __func__;
#ifdef AOA_ALGO_DL
  InitializeAlgoHandles();
  if (gAoaAlgoInitialized) {
    gAoaIfEstimateAngle(GetLinkReference(m_bd_addrs), request_time);
  }
#endif
}

#endif //AOA_ALGO_INT

void CleanupData() {
  LOG(INFO) << __func__;
#ifdef AOA_ALGO_DL
  InitializeAlgoHandles();
  if (gAoaAlgoInitialized) {
    LOG(INFO) << __func__ << " addr:" << m_bd_addrs;
    gAoaIfDeinitHandle(GetLinkReference(m_bd_addrs));
  }
#endif
#ifdef AOA_ALGO_INT
  memset(&mAlgoAoaConfig, 0, sizeof(mAlgoAoaConfig));
#endif
}

AoaResultsCollector(const RawAddress& address, AtpLocatorImpl *atpLocatorImpl):
                   m_bd_addrs(address),
                   mAtpLocatorImpl(atpLocatorImpl) {
  LOG(INFO) << __func__ << " constructor AoaResultsCollector called";
#ifdef AOA_ALGO_INT
  InitializeAoaAlgo();
#endif
}
public:
#ifdef AOA_ALGO_INT
  AoAConfig mAlgoAoaConfig;
#endif
private:
  RawAddress m_bd_addrs;

  AtpLocatorImpl* mAtpLocatorImpl;
  uint16_t m_acl_hdl;
};
}  // namespace atplocator
}  //  namespace bluetooth
#endif //ATP_RESULTS_COLLECTOR_H
#endif //DIR_FINDING_FEATURE