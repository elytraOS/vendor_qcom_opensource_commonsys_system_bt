/*
 * Copyright 2021 HIMSA II K/S - www.himsa.com. Represented by EHIMA -
 * www.ehima.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 /*
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.

    * Redistribution and use in source and binary forms, with or without
      modification, are permitted (subject to the limitations in the
      disclaimer below) provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
*/


#define LOG_TAG "BTAudioClientLeAudioAIDL"

#include "le_audio_software.h"

#include <unordered_map>
#include <vector>

#include "codec_status.h"
//#include "hal_version_manager.h"
#include "btif_ahim.h"

namespace bluetooth {
namespace audio {
namespace aidl {
namespace le_audio {

LeAudioSinkTransport* le_audio_sink = nullptr;
LeAudioSourceTransport* le_audio_source = nullptr;

using ::aidl::android::hardware::bluetooth::audio::AudioConfiguration;
using ::aidl::android::hardware::bluetooth::audio::AudioLocation;
using ::aidl::android::hardware::bluetooth::audio::ChannelMode;
using ::aidl::android::hardware::bluetooth::audio::CodecType;
using ::aidl::android::hardware::bluetooth::audio::Lc3Configuration;
using ::aidl::android::hardware::bluetooth::audio::LeAudioCodecConfiguration;
//using ::aidl::android::hardware::bluetooth::audio::LeAudioMode;
using ::aidl::android::hardware::bluetooth::audio::PcmConfiguration;
//using ::aidl::android::hardware::bluetooth::audio::UnicastConfiguration;
using ::bluetooth::audio::aidl::le_audio::LeAudioClientInterface;
using ::bluetooth::audio::aidl::le_audio::StreamCallbacks;
using ::bluetooth::audio::aidl::AudioConfiguration;
using ::bluetooth::audio::aidl::BluetoothAudioCtrlAck;

static ChannelMode le_audio_channel_mode2audio_hal(uint8_t channels_count) {
  LOG(INFO) << __func__ << ": channels_count: " << channels_count;
  switch (channels_count) {
    case 1:
      return ChannelMode::MONO;
    case 2:
      return ChannelMode::STEREO;
  }
  return ChannelMode::UNKNOWN;
}

LeAudioTransport::LeAudioTransport(void (*flush)(void),
                                   StreamCallbacks stream_cb,
                                   PcmConfiguration pcm_config,
                                   bool is_broadcast_session)
    : flush_(std::move(flush)),
      stream_cb_(std::move(stream_cb)),
      remote_delay_report_ms_(0),
      total_bytes_processed_(0),
      data_position_({}),
      pcm_config_(std::move(pcm_config)),
      lea_pending_cmd_(A2DP_CTRL_CMD_NONE),
      is_pending_start_request_(false),
      is_broadcast_session_(is_broadcast_session){};

BluetoothAudioCtrlAck LeAudioTransport::StartRequest(bool is_low_latency,
                                                     uint8_t direction) {

  BluetoothAudioCtrlAck status = BluetoothAudioCtrlAck::PENDING;
  uint8_t profile = is_broadcast_session_ ? BROADCAST : AUDIO_GROUP_MGR;

  LOG(INFO) << __func__ << ": is_low_latency: " << is_low_latency
                        << ", direction: " << loghex(direction)
                        << ", profile: " << loghex(profile);
  is_pending_start_request_ = true;
  btif_ahim_process_request(A2DP_CTRL_CMD_START, profile, direction);
  lea_pending_cmd_ = A2DP_CTRL_CMD_START;
  return status;
}

BluetoothAudioCtrlAck LeAudioTransport::SuspendRequest(uint8_t direction) {
  BluetoothAudioCtrlAck status = BluetoothAudioCtrlAck::PENDING;
  uint8_t profile = is_broadcast_session_ ? BROADCAST : AUDIO_GROUP_MGR;

  LOG(INFO) << __func__ << ": direction: " << loghex(direction)
                        << ", profile: " << loghex(profile);
  btif_ahim_process_request(A2DP_CTRL_CMD_SUSPEND, profile, direction);
  lea_pending_cmd_ = A2DP_CTRL_CMD_SUSPEND;
  return status;
}

void LeAudioTransport::StopRequest(uint8_t direction) {
  uint8_t profile = is_broadcast_session_ ? BROADCAST : AUDIO_GROUP_MGR;

  LOG(INFO) << __func__ << ": direction: " << loghex(direction)
                        << ", profile: " << loghex(profile);
  btif_ahim_process_request(A2DP_CTRL_CMD_STOP, profile, direction);
}

bool LeAudioTransport::GetPresentationPosition(uint64_t* remote_delay_report_ns,
                                               uint64_t* total_bytes_processed,
                                               timespec* data_position) {
  LOG(INFO) << __func__ << ": data=" << total_bytes_processed_
            << " byte(s), timestamp=" << data_position_.tv_sec << "."
            << data_position_.tv_nsec
            << "s, delay report=" << remote_delay_report_ms_ << " msec.";
  if (remote_delay_report_ns != nullptr) {
    *remote_delay_report_ns = remote_delay_report_ms_ * 1000000u;
  }
  if (total_bytes_processed != nullptr)
    *total_bytes_processed = total_bytes_processed_;
  if (data_position != nullptr) *data_position = data_position_;

  return true;
}

void LeAudioTransport::SourceMetadataChanged(
    const source_metadata_t& source_metadata) {
  auto track_count = source_metadata.track_count;

  if (is_broadcast_session_) {
      LOG(WARNING) << __func__ <<": ignore metadata update for broadcast session";
      return;
  }

  if (track_count == 0) {
    LOG(WARNING) << __func__ << ": invalid number of metadata changed tracks";
    return;
  }

  auto usage = source_metadata.tracks->usage;

  LOG(INFO) << __func__ << ": is_broadcast_session_: "
                        << is_broadcast_session_
                        << ", track_count: " << track_count
                        << ", usage: " << usage;

  btif_ahim_update_src_metadata(source_metadata);
}

void LeAudioTransport::SinkMetadataChanged(
    const sink_metadata_t& sink_metadata) {
  auto track_count = sink_metadata.track_count;

  if (is_broadcast_session_) {
      LOG(WARNING) << __func__ <<": ignore metadata update for broadcast session";
      return;
  }

  if (track_count == 0) {
    LOG(WARNING) << __func__ << ": invalid number of metadata changed tracks";
    return;
  }

  auto source = sink_metadata.tracks->source;

  LOG(INFO) << __func__ << ": is_broadcast_session_: "
                        << is_broadcast_session_
                        << ", track_count: " << track_count
                        << ", source: " << source;

  btif_ahim_update_sink_metadata(sink_metadata);
}


tA2DP_CTRL_CMD LeAudioTransport:: GetPendingCmd() const {
  return lea_pending_cmd_;
}

void LeAudioTransport::ResetPendingCmd() {
  lea_pending_cmd_ = A2DP_CTRL_CMD_NONE;
}

void LeAudioTransport::ResetPresentationPosition() {
  VLOG(2) << __func__ << ": called.";
  remote_delay_report_ms_ = 0;
  total_bytes_processed_ = 0;
  data_position_ = {};
}

void LeAudioTransport::LogBytesProcessed(size_t bytes_processed) {
  if (bytes_processed) {
    total_bytes_processed_ += bytes_processed;
    clock_gettime(CLOCK_MONOTONIC, &data_position_);
  }
}

void LeAudioTransport::SetRemoteDelay(uint16_t delay_report_ms) {
  LOG(INFO) << __func__ << ": delay_report=" << delay_report_ms << " msec";
  remote_delay_report_ms_ = delay_report_ms;
}

const PcmConfiguration& LeAudioTransport::LeAudioGetSelectedHalPcmConfig() {
  return pcm_config_;
}

void LeAudioTransport::LeAudioSetSelectedHalPcmConfig(uint32_t sample_rate_hz,
                                                      uint8_t bit_rate,
                                                      uint8_t channels_count,
                                                      uint32_t data_interval) {
  pcm_config_.sampleRateHz = (sample_rate_hz);
  pcm_config_.bitsPerSample = (bit_rate);
  pcm_config_.channelMode = le_audio_channel_mode2audio_hal(channels_count);
  pcm_config_.dataIntervalUs = data_interval;
}

bool LeAudioTransport::IsPendingStartStream(void) {
  return is_pending_start_request_;
}
void LeAudioTransport::ClearPendingStartStream(void) {
  is_pending_start_request_ = false;
}

inline void flush_unicast_sink() {
  if (LeAudioSinkTransport::interface_unicast_ == nullptr) return;

  LeAudioSinkTransport::interface_unicast_->FlushAudioData();
}

inline void flush_broadcast_sink() {
  if (LeAudioSinkTransport::interface_broadcast_ == nullptr) return;

  LeAudioSinkTransport::interface_broadcast_->FlushAudioData();
}

inline bool is_broadcaster_session(SessionType session_type) {
  if (session_type ==
          SessionType::LE_AUDIO_BROADCAST_HARDWARE_OFFLOAD_ENCODING_DATAPATH ||
      session_type ==
          SessionType::LE_AUDIO_BROADCAST_SOFTWARE_ENCODING_DATAPATH) {
    return true;
  }

  return false;
}

LeAudioSinkTransport::LeAudioSinkTransport(SessionType session_type,
                                           StreamCallbacks stream_cb)
    : IBluetoothSinkTransportInstance(session_type, (AudioConfiguration){}) {
  transport_ = new LeAudioTransport(
      is_broadcaster_session(session_type) ? flush_broadcast_sink
                                           : flush_unicast_sink,
      std::move(stream_cb), {16000, ChannelMode::STEREO, 16, 0},
      is_broadcaster_session(session_type));
};

LeAudioSinkTransport::~LeAudioSinkTransport() { delete transport_; }

BluetoothAudioCtrlAck LeAudioSinkTransport::StartRequest(bool is_low_latency) {
  return transport_->StartRequest(is_low_latency, TO_AIR);
}

BluetoothAudioCtrlAck LeAudioSinkTransport::SuspendRequest() {
  return transport_->SuspendRequest(TO_AIR);
}

void LeAudioSinkTransport::StopRequest() { transport_->StopRequest(TO_AIR); }

bool LeAudioSinkTransport::GetPresentationPosition(
    uint64_t* remote_delay_report_ns, uint64_t* total_bytes_read,
    timespec* data_position) {
  return transport_->GetPresentationPosition(remote_delay_report_ns,
                                             total_bytes_read, data_position);
}

void LeAudioSinkTransport::SourceMetadataChanged(
    const source_metadata_t& source_metadata) {

  auto track_count = source_metadata.track_count;
  auto usage = source_metadata.tracks->usage;
  LOG(INFO) << __func__ << ", track_count: " << track_count
                        << ", usage: " << usage;

  transport_->SourceMetadataChanged(source_metadata);
}

void LeAudioSinkTransport::SinkMetadataChanged(
    const sink_metadata_t& sink_metadata) {

  auto track_count = sink_metadata.track_count;
  auto source = sink_metadata.tracks->source;
  LOG(INFO) << __func__ << ", track_count: " << track_count
                        << ", source: " << source;

  transport_->SinkMetadataChanged(sink_metadata);
}

void LeAudioSinkTransport::ResetPresentationPosition() {
  transport_->ResetPresentationPosition();
}

void LeAudioSinkTransport::LogBytesRead(size_t bytes_read) {
  transport_->LogBytesProcessed(bytes_read);
}

void LeAudioSinkTransport::SetRemoteDelay(uint16_t delay_report_ms) {
  transport_->SetRemoteDelay(delay_report_ms);
}

const PcmConfiguration& LeAudioSinkTransport::LeAudioGetSelectedHalPcmConfig() {
  return transport_->LeAudioGetSelectedHalPcmConfig();
}

void LeAudioSinkTransport::LeAudioSetSelectedHalPcmConfig(
    uint32_t sample_rate_hz, uint8_t bit_rate, uint8_t channels_count,
    uint32_t data_interval) {
  transport_->LeAudioSetSelectedHalPcmConfig(sample_rate_hz, bit_rate,
                                             channels_count, data_interval);
}

bool LeAudioSinkTransport::IsPendingStartStream(void) {
  return transport_->IsPendingStartStream();
}
void LeAudioSinkTransport::ClearPendingStartStream(void) {
  transport_->ClearPendingStartStream();
}

tA2DP_CTRL_CMD LeAudioSinkTransport::GetPendingCmd() const {
  return transport_->GetPendingCmd();
}

void LeAudioSinkTransport::ResetPendingCmd() {
  transport_->ResetPendingCmd();
}

void flush_source() {
  if (LeAudioSourceTransport::interface == nullptr) return;

  LeAudioSourceTransport::interface->FlushAudioData();
}

LeAudioSourceTransport::LeAudioSourceTransport(SessionType session_type,
                                               StreamCallbacks stream_cb)
    : IBluetoothSourceTransportInstance(session_type, (AudioConfiguration){}) {
  transport_ = new LeAudioTransport(flush_source, std::move(stream_cb),
                                    {16000, ChannelMode::MONO, 16, 0}, false);
};

LeAudioSourceTransport::~LeAudioSourceTransport() { delete transport_; }

BluetoothAudioCtrlAck LeAudioSourceTransport::StartRequest(
    bool is_low_latency) {
  return transport_->StartRequest(is_low_latency, FROM_AIR);
}

BluetoothAudioCtrlAck LeAudioSourceTransport::SuspendRequest() {
  return transport_->SuspendRequest(FROM_AIR);
}

void LeAudioSourceTransport::StopRequest() { transport_->StopRequest(FROM_AIR); }

bool LeAudioSourceTransport::GetPresentationPosition(
    uint64_t* remote_delay_report_ns, uint64_t* total_bytes_written,
    timespec* data_position) {
  return transport_->GetPresentationPosition(
      remote_delay_report_ns, total_bytes_written, data_position);
}

void LeAudioSourceTransport::SourceMetadataChanged(
    const source_metadata_t& source_metadata) {

  auto track_count = source_metadata.track_count;
  auto usage = source_metadata.tracks->usage;
  LOG(INFO) << __func__  << ", track_count: " << track_count
                         << ", usage: " << usage;

  transport_->SourceMetadataChanged(source_metadata);
}

void LeAudioSourceTransport::SinkMetadataChanged(
    const sink_metadata_t& sink_metadata) {

  auto track_count = sink_metadata.track_count;
  auto source = sink_metadata.tracks->source;
  LOG(INFO) << __func__ << ", track_count: " << track_count
                        << ", source: " << source;

  transport_->SinkMetadataChanged(sink_metadata);
}

void LeAudioSourceTransport::ResetPresentationPosition() {
  transport_->ResetPresentationPosition();
}

void LeAudioSourceTransport::LogBytesWritten(size_t bytes_written) {
  transport_->LogBytesProcessed(bytes_written);
}

void LeAudioSourceTransport::SetRemoteDelay(uint16_t delay_report_ms) {
  transport_->SetRemoteDelay(delay_report_ms);
}

const PcmConfiguration&
LeAudioSourceTransport::LeAudioGetSelectedHalPcmConfig() {
  return transport_->LeAudioGetSelectedHalPcmConfig();
}

void LeAudioSourceTransport::LeAudioSetSelectedHalPcmConfig(
    uint32_t sample_rate_hz, uint8_t bit_rate, uint8_t channels_count,
    uint32_t data_interval) {
  transport_->LeAudioSetSelectedHalPcmConfig(sample_rate_hz, bit_rate,
                                             channels_count, data_interval);
}

bool LeAudioSourceTransport::IsPendingStartStream(void) {
  return transport_->IsPendingStartStream();
}
void LeAudioSourceTransport::ClearPendingStartStream(void) {
  transport_->ClearPendingStartStream();
}

tA2DP_CTRL_CMD LeAudioSourceTransport::GetPendingCmd() const {
  return transport_->GetPendingCmd();
}

void LeAudioSourceTransport::ResetPendingCmd() {
  transport_->ResetPendingCmd();
}

// cleint interface code

aidl::BluetoothAudioSinkClientInterface* get_aidl_client_interface(
    bool is_broadcaster) {
  if (is_broadcaster)
    return aidl::le_audio::LeAudioSinkTransport::interface_broadcast_;

  return aidl::le_audio::LeAudioSinkTransport::interface_unicast_;
}

aidl::le_audio::LeAudioSinkTransport* get_aidl_transport_instance(
    bool is_broadcaster) {
  if (is_broadcaster)
    return aidl::le_audio::LeAudioSinkTransport::instance_broadcast_;

  return aidl::le_audio::LeAudioSinkTransport::instance_unicast_;
}

bool is_aidl_offload_encoding_session(bool is_broadcaster) {
  return get_aidl_client_interface(is_broadcaster)
                 ->GetTransportInstance()
                 ->GetSessionType() ==
             aidl::SessionType::LE_AUDIO_HARDWARE_OFFLOAD_ENCODING_DATAPATH ||
         get_aidl_client_interface(is_broadcaster)
                 ->GetTransportInstance()
                 ->GetSessionType() ==
             aidl::SessionType::
                 LE_AUDIO_BROADCAST_HARDWARE_OFFLOAD_ENCODING_DATAPATH;
}

LeAudioClientInterface* LeAudioClientInterface::interface = nullptr;
LeAudioClientInterface* LeAudioClientInterface::Get() {
  if (property_get_bool(BLUETOOTH_AUDIO_HAL_PROP_DISABLED, false)) {
    LOG(ERROR) << __func__ << ": BluetoothAudio HAL is disabled";
    return nullptr;
  }

  if (LeAudioClientInterface::interface == nullptr)
    LeAudioClientInterface::interface = new LeAudioClientInterface();

  return LeAudioClientInterface::interface;
}

void LeAudioClientInterface::Sink::Cleanup() {
  LOG(INFO) << __func__ << " sink";
  StopSession();

  if (aidl::le_audio::LeAudioSinkTransport::interface_unicast_) {
    delete aidl::le_audio::LeAudioSinkTransport::interface_unicast_;
    aidl::le_audio::LeAudioSinkTransport::interface_unicast_ = nullptr;
  }
  if (aidl::le_audio::LeAudioSinkTransport::interface_broadcast_) {
    delete aidl::le_audio::LeAudioSinkTransport::interface_broadcast_;
    aidl::le_audio::LeAudioSinkTransport::interface_broadcast_ = nullptr;
  }
  if (aidl::le_audio::LeAudioSinkTransport::instance_unicast_) {
    delete aidl::le_audio::LeAudioSinkTransport::instance_unicast_;
    aidl::le_audio::LeAudioSinkTransport::instance_unicast_ = nullptr;
  }
  if (aidl::le_audio::LeAudioSinkTransport::instance_broadcast_) {
    delete aidl::le_audio::LeAudioSinkTransport::instance_broadcast_;
    aidl::le_audio::LeAudioSinkTransport::instance_broadcast_ = nullptr;
  }
}

void LeAudioClientInterface::Sink::SetPcmParameters(
    const PcmParameters& params) {
  return get_aidl_transport_instance(is_broadcaster_)
      ->LeAudioSetSelectedHalPcmConfig(
          params.sample_rate, params.bits_per_sample, params.channels_count,
          params.data_interval_us);
}

// Update Le Audio delay report to BluetoothAudio HAL
void LeAudioClientInterface::Sink::SetRemoteDelay(uint16_t delay_report_ms) {
  LOG(INFO) << __func__ << ": delay_report_ms=" << delay_report_ms << " ms";

  get_aidl_transport_instance(is_broadcaster_)->SetRemoteDelay(
      delay_report_ms);
}

void LeAudioClientInterface::Sink::StartSession() {
  LOG(ERROR) << __func__;
  AudioConfigurationAIDL audio_config;
  if (is_aidl_offload_encoding_session(is_broadcaster_)) {
    if (is_broadcaster_) {
      aidl::le_audio::LeAudioBroadcastConfiguration le_audio_config = {};
      audio_config.set<AudioConfigurationAIDL::leAudioBroadcastConfig>(
          le_audio_config);
    } else {
      aidl::le_audio::LeAudioConfiguration le_audio_config = {};
      audio_config.set<AudioConfigurationAIDL::leAudioConfig>(
          le_audio_config);
    }
  } else {
    audio_config.set<AudioConfigurationAIDL::pcmConfig>(
        get_aidl_transport_instance(is_broadcaster_)
            ->LeAudioGetSelectedHalPcmConfig());
  }
  get_aidl_client_interface(is_broadcaster_)->StartSession();
}

tA2DP_CTRL_CMD LeAudioClientInterface::Sink::GetPendingCmd() {
  LOG(INFO) << __func__;
  return get_aidl_transport_instance(is_broadcaster_)->GetPendingCmd();
}

void LeAudioClientInterface::Sink::ResetPendingCmd() {
  LOG(INFO) << __func__;
  return get_aidl_transport_instance(is_broadcaster_)->ResetPendingCmd();
}

void LeAudioClientInterface::Sink::ConfirmSuspendRequest() {
  LOG(INFO) << __func__;
  // TODO
  /*
  if (!aidl::le_audio::LeAudioSinkTransport::instance->IsPendingStartStream()) {
    LOG(WARNING) << ", no pending start stream request";
    return;
  } */
  get_aidl_client_interface(is_broadcaster_)->StreamSuspended(
      aidl::BluetoothAudioCtrlAck::SUCCESS_FINISHED);
}

void LeAudioClientInterface::Sink::CancelSuspendRequest() {
  LOG(INFO) << __func__;
  // TODO
  /*
  if (!aidl::le_audio::LeAudioSinkTransport::instance->IsPendingStartStream()) {
    LOG(WARNING) << ", no pending start stream request";
    return;
  } */
  get_aidl_client_interface(is_broadcaster_)->StreamSuspended(
      aidl::BluetoothAudioCtrlAck::FAILURE);
}

void LeAudioClientInterface::Sink::CancelSuspendRequestWithReconfig() {
  LOG(INFO) << __func__;
  // TODO
  /*
  if (!aidl::le_audio::LeAudioSinkTransport::instance->IsPendingStartStream()) {
    LOG(WARNING) << ", no pending start stream request";
    return;
  } */
  get_aidl_client_interface(is_broadcaster_)->StreamStarted(
      aidl::BluetoothAudioCtrlAck::SUCCESS_RECONFIGURATION);
}

void LeAudioClientInterface::Sink::ConfirmStreamingRequest() {
  LOG(INFO) << __func__;
  /*if (!get_aidl_transport_instance(is_broadcaster_)->IsPendingStartStream()) {
    LOG(WARNING) << ", no pending start stream request";
    return;
  } */
  get_aidl_transport_instance(is_broadcaster_)->ClearPendingStartStream();
  get_aidl_client_interface(is_broadcaster_)->StreamStarted(
      aidl::BluetoothAudioCtrlAck::SUCCESS_FINISHED);
}

void LeAudioClientInterface::Sink::CancelStreamingRequest() {
  LOG(INFO) << __func__;
  if (!get_aidl_transport_instance(is_broadcaster_)->IsPendingStartStream()) {
    LOG(WARNING) << ", no pending start stream request";
    return;
  }
  get_aidl_transport_instance(is_broadcaster_)->ClearPendingStartStream();
  get_aidl_client_interface(is_broadcaster_)->StreamStarted(
      aidl::BluetoothAudioCtrlAck::FAILURE);
}

void LeAudioClientInterface::Sink::StopSession() {
  LOG(INFO) << __func__ << " sink";
  get_aidl_transport_instance(is_broadcaster_)->ClearPendingStartStream();
  get_aidl_client_interface(is_broadcaster_)->EndSession();
}

void LeAudioClientInterface::Sink::UpdateAudioConfigToHal(
                         AudioConfigurationAIDL& offload_config) {
  LOG(INFO) << __func__;
  if (!is_aidl_offload_encoding_session(is_broadcaster_)) {
    LOG(INFO) << __func__ << "seesion is not le audio hardware encoding, return";
    return;
  }
  get_aidl_client_interface(is_broadcaster_)->
                                  UpdateAudioConfig(offload_config);
}

size_t LeAudioClientInterface::Sink::Read(uint8_t* p_buf, uint32_t len) {
  return get_aidl_client_interface(is_broadcaster_)->ReadAudioData(p_buf,
                                                                        len);
}

void LeAudioClientInterface::Source::Cleanup() {
  LOG(INFO) << __func__ << " source";
  StopSession();
  if (aidl::le_audio::LeAudioSourceTransport::interface) {
    delete aidl::le_audio::LeAudioSourceTransport::interface;
    aidl::le_audio::LeAudioSourceTransport::interface = nullptr;
  }
  if (aidl::le_audio::LeAudioSourceTransport::instance) {
    delete aidl::le_audio::LeAudioSourceTransport::instance;
    aidl::le_audio::LeAudioSourceTransport::instance = nullptr;
  }
}

void LeAudioClientInterface::Source::SetPcmParameters(
    const PcmParameters& params) {
  return aidl::le_audio::LeAudioSourceTransport::instance
      ->LeAudioSetSelectedHalPcmConfig(
          params.sample_rate, params.bits_per_sample, params.channels_count,
          params.data_interval_us);
}

void LeAudioClientInterface::Source::SetRemoteDelay(uint16_t delay_report_ms) {
  LOG(INFO) << __func__ << ": delay_report_ms=" << delay_report_ms << " ms";
  return aidl::le_audio::LeAudioSourceTransport::instance->SetRemoteDelay(
      delay_report_ms);
}

void LeAudioClientInterface::Source::StartSession() {
  LOG(INFO) << __func__;
  AudioConfigurationAIDL audio_config;
  if (aidl::le_audio::LeAudioSourceTransport::
          interface->GetTransportInstance()
              ->GetSessionType() ==
      aidl::SessionType::LE_AUDIO_HARDWARE_OFFLOAD_DECODING_DATAPATH) {
    aidl::le_audio::LeAudioConfiguration le_audio_config;
    audio_config.set<AudioConfigurationAIDL::leAudioConfig>(
        aidl::le_audio::LeAudioConfiguration{});
  } else {
    audio_config.set<AudioConfigurationAIDL::pcmConfig>(
        aidl::le_audio::LeAudioSourceTransport::instance
            ->LeAudioGetSelectedHalPcmConfig());
  }
  aidl::le_audio::LeAudioSourceTransport::interface->StartSession();
}

tA2DP_CTRL_CMD LeAudioClientInterface::Source::GetPendingCmd() {
  LOG(INFO) << __func__;
  return aidl::le_audio::LeAudioSourceTransport::instance->GetPendingCmd();
}

void LeAudioClientInterface::Source::ResetPendingCmd() {
  LOG(INFO) << __func__;
  return aidl::le_audio::LeAudioSourceTransport::instance->ResetPendingCmd();
}

void LeAudioClientInterface::Source::ConfirmSuspendRequest() {
  LOG(INFO) << __func__;
  // TODO
  /*
  if (!aidl::le_audio::LeAudioSourceTransport::instance->IsPendingStartStream()) {
    LOG(WARNING) << ", no pending start stream request";
    return;
  } */
  aidl::le_audio::LeAudioSourceTransport::interface->StreamSuspended(
      aidl::BluetoothAudioCtrlAck::SUCCESS_FINISHED);
}

void LeAudioClientInterface::Source::CancelSuspendRequest() {
  LOG(INFO) << __func__;
  // TODO
  /*
  if (!aidl::le_audio::LeAudioSourceTransport::instance->IsPendingStartStream()) {
    LOG(WARNING) << ", no pending start stream request";
    return;
  } */
  aidl::le_audio::LeAudioSourceTransport::interface->StreamSuspended(
      aidl::BluetoothAudioCtrlAck::FAILURE);
}

void LeAudioClientInterface::Source::CancelSuspendRequestWithReconfig() {
  LOG(INFO) << __func__;
  // TODO
  /*
  if (!aidl::le_audio::LeAudioSourceTransport::instance->IsPendingStartStream()) {
    LOG(WARNING) << ", no pending start stream request";
    return;
  } */
  aidl::le_audio::LeAudioSourceTransport::interface->StreamSuspended(
      aidl::BluetoothAudioCtrlAck::SUCCESS_RECONFIGURATION);
}

void LeAudioClientInterface::Source::ConfirmStreamingRequest() {
  LOG(INFO) << __func__;
  /*if ((aidl::le_audio::LeAudioSourceTransport::instance &&
       !aidl::le_audio::LeAudioSourceTransport::instance
            ->IsPendingStartStream())) {
    LOG(WARNING) << ", no pending start stream request";
    return;
  }*/

  aidl::le_audio::LeAudioSourceTransport::instance->ClearPendingStartStream();
  aidl::le_audio::LeAudioSourceTransport::interface->StreamStarted(
      aidl::BluetoothAudioCtrlAck::SUCCESS_FINISHED);
}

void LeAudioClientInterface::Source::CancelStreamingRequest() {
  LOG(INFO) << __func__;
  if (!aidl::le_audio::LeAudioSourceTransport::instance
           ->IsPendingStartStream()) {
    LOG(WARNING) << ", no pending start stream request";
    return;
  }
  aidl::le_audio::LeAudioSourceTransport::instance->ClearPendingStartStream();
  aidl::le_audio::LeAudioSourceTransport::interface->StreamStarted(
      aidl::BluetoothAudioCtrlAck::FAILURE);
}

void LeAudioClientInterface::Source::StopSession() {
  LOG(INFO) << __func__ << " source";
  aidl::le_audio::LeAudioSourceTransport::instance->ClearPendingStartStream();
  aidl::le_audio::LeAudioSourceTransport::interface->EndSession();
}

void LeAudioClientInterface::Source::UpdateAudioConfigToHal(
                              AudioConfigurationAIDL& offload_config) {
  LOG(INFO) << __func__;
  if (aidl::le_audio::LeAudioSourceTransport::interface->GetTransportInstance()
          ->GetSessionType() !=
      aidl::SessionType::LE_AUDIO_HARDWARE_OFFLOAD_DECODING_DATAPATH) {
    return;
  }
  aidl::le_audio::LeAudioSourceTransport::interface->
                                  UpdateAudioConfig(offload_config);
}

size_t LeAudioClientInterface::Source::Write(const uint8_t* p_buf,
                                             uint32_t len) {
  return aidl::le_audio::LeAudioSourceTransport::interface->WriteAudioData(
      p_buf, len);
}

LeAudioClientInterface::Sink* LeAudioClientInterface::GetSink(
    StreamCallbacks stream_cb,
    thread_t* message_loop,
    bool is_broadcasting_session_type) {
  Sink* sink = is_broadcasting_session_type ? broadcast_sink_ : unicast_sink_;
  if (sink == nullptr) {
    sink = new Sink(is_broadcasting_session_type);
  } else {
    LOG(WARNING) << __func__ << ", Sink is already acquired";
    return nullptr;
  }
  LOG(INFO) << __func__;

  aidl::SessionType session_type =
      is_broadcasting_session_type
          ? aidl::SessionType::LE_AUDIO_BROADCAST_HARDWARE_OFFLOAD_ENCODING_DATAPATH
          : aidl::SessionType::LE_AUDIO_HARDWARE_OFFLOAD_ENCODING_DATAPATH;

  if (session_type ==
      aidl::SessionType::LE_AUDIO_HARDWARE_OFFLOAD_ENCODING_DATAPATH) {
    aidl::le_audio::LeAudioSinkTransport::instance_unicast_ =
        new aidl::le_audio::LeAudioSinkTransport(session_type,
                                                 std::move(stream_cb));
    aidl::le_audio::LeAudioSinkTransport::interface_unicast_ =
        new aidl::BluetoothAudioSinkClientInterface(
            aidl::le_audio::LeAudioSinkTransport::instance_unicast_, message_loop);
    if (!aidl::le_audio::LeAudioSinkTransport::interface_unicast_->IsValid()) {
      LOG(WARNING) << __func__
                   << ": BluetoothAudio HAL for Le Audio is invalid?!";
      delete aidl::le_audio::LeAudioSinkTransport::interface_unicast_;
      aidl::le_audio::LeAudioSinkTransport::interface_unicast_ = nullptr;
      delete aidl::le_audio::LeAudioSinkTransport::instance_unicast_;
      aidl::le_audio::LeAudioSinkTransport::instance_unicast_ = nullptr;
      delete sink;
      sink = nullptr;

      return nullptr;
    }
  } else {
    aidl::le_audio::LeAudioSinkTransport::instance_broadcast_ =
        new aidl::le_audio::LeAudioSinkTransport(session_type,
                                                 std::move(stream_cb));
    aidl::le_audio::LeAudioSinkTransport::interface_broadcast_ =
        new aidl::BluetoothAudioSinkClientInterface(
            aidl::le_audio::LeAudioSinkTransport::instance_broadcast_, message_loop);
    if (!aidl::le_audio::LeAudioSinkTransport::interface_broadcast_->IsValid()) {
      LOG(WARNING) << __func__
                  << ": BluetoothAudio HAL for Le Audio is invalid?!";
      delete aidl::le_audio::LeAudioSinkTransport::interface_broadcast_;
      aidl::le_audio::LeAudioSinkTransport::interface_broadcast_ = nullptr;
      delete aidl::le_audio::LeAudioSinkTransport::instance_broadcast_;
      aidl::le_audio::LeAudioSinkTransport::instance_broadcast_ = nullptr;
      delete sink;
      sink = nullptr;

      return nullptr;
    }
  }
  return sink;
}

bool LeAudioClientInterface::IsUnicastSinkAcquired() {
  return unicast_sink_ != nullptr;
}
bool LeAudioClientInterface::IsBroadcastSinkAcquired() {
  return broadcast_sink_ != nullptr;
}

bool LeAudioClientInterface::ReleaseSink(LeAudioClientInterface::Sink* sink) {
  if (sink != unicast_sink_ && sink != broadcast_sink_) {
    LOG(WARNING) << __func__ << ", can't release not acquired sink";
    return false;
  }

  if ((aidl::le_audio::LeAudioSinkTransport::interface_unicast_ &&
       aidl::le_audio::LeAudioSinkTransport::instance_unicast_) ||
      (aidl::le_audio::LeAudioSinkTransport::interface_broadcast_ &&
      aidl::le_audio::LeAudioSinkTransport::instance_broadcast_))
    sink->Cleanup();

  if (sink == unicast_sink_) {
    delete (unicast_sink_);
    unicast_sink_ = nullptr;
  } else if (sink == broadcast_sink_) {
    delete (broadcast_sink_);
    broadcast_sink_ = nullptr;
  }

  return true;
}

LeAudioClientInterface::Source* LeAudioClientInterface::GetSource(
    StreamCallbacks stream_cb,
    thread_t* message_loop) {
  if (source_ == nullptr) {
    source_ = new Source();
  } else {
    LOG(WARNING) << __func__ << ", Source is already acquired";
    return nullptr;
  }

  LOG(INFO) << __func__;

  aidl::SessionType session_type =
      aidl::SessionType::LE_AUDIO_HARDWARE_OFFLOAD_DECODING_DATAPATH;
  aidl::le_audio::LeAudioSourceTransport::instance =
      new aidl::le_audio::LeAudioSourceTransport(session_type,
                                                 std::move(stream_cb));
  aidl::le_audio::LeAudioSourceTransport::interface =
      new aidl::BluetoothAudioSourceClientInterface(
          aidl::le_audio::LeAudioSourceTransport::instance, message_loop);
  if (!aidl::le_audio::LeAudioSourceTransport::interface->IsValid()) {
    LOG(WARNING) << __func__
                 << ": BluetoothAudio HAL for Le Audio is invalid?!";
    delete aidl::le_audio::LeAudioSourceTransport::interface;
    aidl::le_audio::LeAudioSourceTransport::interface = nullptr;
    delete aidl::le_audio::LeAudioSourceTransport::instance;
    aidl::le_audio::LeAudioSourceTransport::instance = nullptr;
    delete source_;
    source_ = nullptr;

    return nullptr;
  }
  return source_;
}

bool LeAudioClientInterface::IsSourceAcquired() { return source_ != nullptr; }

bool LeAudioClientInterface::ReleaseSource(
    LeAudioClientInterface::Source* source) {
  if (source != source_) {
    LOG(WARNING) << __func__ << ", can't release not acquired source";
    return false;
  }

  if ((aidl::le_audio::LeAudioSourceTransport::interface &&
       aidl::le_audio::LeAudioSourceTransport::instance))
    source->Cleanup();

  delete (source_);
  source_ = nullptr;

  return true;
}

/*
std::unordered_map<int32_t, uint8_t> sampling_freq_map{
    {8000, ::le_audio::codec_spec_conf::kLeAudioSamplingFreq8000Hz},
    {16000, ::le_audio::codec_spec_conf::kLeAudioSamplingFreq16000Hz},
    {24000, ::le_audio::codec_spec_conf::kLeAudioSamplingFreq24000Hz},
    {32000, ::le_audio::codec_spec_conf::kLeAudioSamplingFreq32000Hz},
    {44100, ::le_audio::codec_spec_conf::kLeAudioSamplingFreq44100Hz},
    {48000, ::le_audio::codec_spec_conf::kLeAudioSamplingFreq48000Hz},
    {88200, ::le_audio::codec_spec_conf::kLeAudioSamplingFreq88200Hz},
    {96000, ::le_audio::codec_spec_conf::kLeAudioSamplingFreq96000Hz},
    {176400, ::le_audio::codec_spec_conf::kLeAudioSamplingFreq176400Hz},
    {192000, ::le_audio::codec_spec_conf::kLeAudioSamplingFreq192000Hz}};

std::unordered_map<int32_t, uint8_t> frame_duration_map{
    {7500, ::le_audio::codec_spec_conf::kLeAudioCodecLC3FrameDur7500us},
    {10000, ::le_audio::codec_spec_conf::kLeAudioCodecLC3FrameDur10000us}};

std::unordered_map<int32_t, uint16_t> octets_per_frame_map{
    {30, ::le_audio::codec_spec_conf::kLeAudioCodecLC3FrameLen30},
    {40, ::le_audio::codec_spec_conf::kLeAudioCodecLC3FrameLen40},
    {120, ::le_audio::codec_spec_conf::kLeAudioCodecLC3FrameLen120}};

std::unordered_map<AudioLocation, uint32_t> audio_location_map{
    {AudioLocation::UNKNOWN,
     ::le_audio::codec_spec_conf::kLeAudioLocationMonoUnspecified},
    {AudioLocation::FRONT_LEFT,
     ::le_audio::codec_spec_conf::kLeAudioLocationFrontLeft},
    {AudioLocation::FRONT_RIGHT,
     ::le_audio::codec_spec_conf::kLeAudioLocationFrontRight},
    {static_cast<AudioLocation>(
         static_cast<uint8_t>(AudioLocation::FRONT_LEFT) |
         static_cast<uint8_t>(AudioLocation::FRONT_RIGHT)),
     ::le_audio::codec_spec_conf::kLeAudioLocationFrontLeft |
         ::le_audio::codec_spec_conf::kLeAudioLocationFrontRight}};
*/
/*AudioConfiguration offload_config_to_hal_audio_config(
    offload_config) {
  Lc3Configuration lc3_config{
      .pcmBitDepth = static_cast<int8_t>(offload_config.bits_per_sample),
      .samplingFrequencyHz = static_cast<int32_t>(offload_config.sampling_rate),
      .frameDurationUs = static_cast<int32_t>(offload_config.frame_duration),
      .octetsPerFrame = static_cast<int32_t>(offload_config.octets_per_frame),
      .blocksPerSdu = static_cast<int8_t>(offload_config.blocks_per_sdu),
  };
  UnicastConfiguration ucast_config = {
      .peerDelay = static_cast<int32_t>(offload_config.peer_delay),
      .leAudioCodecConfig = LeAudioCodecConfiguration(lc3_config)};

  for (auto& [handle, location] : offload_config.stream_map) {
    ucast_config.streamMap.push_back({
        .streamHandle = handle,
        .audioChannelAllocation = static_cast<int32_t>(location),
    });
  }


  LeAudioConfiguration le_audio_config{
      .mode = LeAudioMode::UNICAST,
      .modeConfig = LeAudioConfiguration::LeAudioModeConfig(ucast_config),
  };
  return AudioConfiguration(le_audio_config);
}
*/
}  // namespace le_audio
}  // namespace aidl
}  // namespace audio
}  // namespace bluetooth
