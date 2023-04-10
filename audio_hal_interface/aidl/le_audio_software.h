/*
 * Copyright 2021 The Android Open Source Project
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

#pragma once

#include <hardware/audio.h>
#include "osi/include/thread.h"
#include <functional>
#include "audio_aidl_interfaces.h"
#include "transport_instance.h"
//#include "bta/le_audio/le_audio_types.h"
#include "client_interface.h"
#include "audio_a2dp_hw/include/audio_a2dp_hw.h"

namespace bluetooth {
namespace audio {
namespace aidl {
namespace le_audio {
using AudioConfigurationAIDL =
    ::aidl::android::hardware::bluetooth::audio::AudioConfiguration;
using ::aidl::android::hardware::bluetooth::audio::LeAudioBroadcastConfiguration;
using ::aidl::android::hardware::bluetooth::audio::LeAudioCodecConfiguration;
using ::aidl::android::hardware::bluetooth::audio::LeAudioConfiguration;
using ::aidl::android::hardware::bluetooth::audio::PcmConfiguration;
using ::aidl::android::hardware::bluetooth::audio::SessionType;
using ::aidl::android::hardware::bluetooth::audio::UnicastCapability;
using ::bluetooth::audio::aidl::BluetoothAudioCtrlAck;

constexpr uint8_t kChannelNumberMono = 1;
constexpr uint8_t kChannelNumberStereo = 2;

constexpr uint32_t kSampleRate48000 = 48000;
constexpr uint32_t kSampleRate44100 = 44100;
constexpr uint32_t kSampleRate32000 = 32000;
constexpr uint32_t kSampleRate24000 = 24000;
constexpr uint32_t kSampleRate16000 = 16000;
constexpr uint32_t kSampleRate8000 = 8000;

constexpr uint8_t kBitsPerSample16 = 16;
constexpr uint8_t kBitsPerSample24 = 24;
constexpr uint8_t kBitsPerSample32 = 32;

void flush_source();

bool is_source_hal_enabled();
bool is_sink_hal_enabled();

struct StreamCallbacks {
  std::function<bool(bool start_media_task)> on_resume_;
  std::function<bool(void)> on_suspend_;
  std::function<bool(const source_metadata_t&)> on_metadata_update_;
  std::function<bool(const sink_metadata_t&)> on_sink_metadata_update_;
};

class LeAudioTransport {
 public:
  LeAudioTransport(void (*flush)(void), StreamCallbacks stream_cb,
                   PcmConfiguration pcm_config, bool is_broadcast_session);

  BluetoothAudioCtrlAck StartRequest(bool is_low_latency, uint8_t direction);

  BluetoothAudioCtrlAck SuspendRequest(uint8_t direction);

  void StopRequest(uint8_t direction);

  bool GetPresentationPosition(uint64_t* remote_delay_report_ns,
                               uint64_t* total_bytes_processed,
                               timespec* data_position);

  void SourceMetadataChanged(const source_metadata_t& source_metadata);

  void SinkMetadataChanged(const sink_metadata_t& sink_metadata);

  tA2DP_CTRL_CMD GetPendingCmd() const;

  void ResetPendingCmd();

  void ResetPresentationPosition();

  void LogBytesProcessed(size_t bytes_processed);

  void SetRemoteDelay(uint16_t delay_report_ms);

  uint16_t GetRemoteDelay();

  const PcmConfiguration& LeAudioGetSelectedHalPcmConfig();

  void LeAudioSetSelectedHalPcmConfig(uint32_t sample_rate_hz, uint8_t bit_rate,
                                      uint8_t channels_count,
                                      uint32_t data_interval);

  bool IsPendingStartStream(void);
  void ClearPendingStartStream(void);
  bool IsBroadcastSession() { return is_broadcast_session_; }

 private:
  void (*flush_)(void);
  StreamCallbacks stream_cb_;
  uint16_t remote_delay_report_ms_;
  uint64_t total_bytes_processed_;
  timespec data_position_;
  PcmConfiguration pcm_config_;
  tA2DP_CTRL_CMD lea_pending_cmd_;
  bool is_pending_start_request_;
  bool is_broadcast_session_;
};

// Sink transport implementation for Le Audio
class LeAudioSinkTransport
    : public ::bluetooth::audio::aidl::IBluetoothSinkTransportInstance {
 public:
  LeAudioSinkTransport(SessionType session_type, StreamCallbacks stream_cb);

  ~LeAudioSinkTransport();

  BluetoothAudioCtrlAck StartRequest(bool is_low_latency) override;

  BluetoothAudioCtrlAck SuspendRequest() override;

  void StopRequest() override;

  bool GetPresentationPosition(uint64_t* remote_delay_report_ns,
                               uint64_t* total_bytes_read,
                               timespec* data_position) override;

  void SourceMetadataChanged(const source_metadata_t& source_metadata) override;

  void SinkMetadataChanged(const sink_metadata_t& sink_metadata) override;

  tA2DP_CTRL_CMD GetPendingCmd() const;

  void ResetPendingCmd();

  void ResetPresentationPosition() override;

  void LogBytesRead(size_t bytes_read) override;

  void SetRemoteDelay(uint16_t delay_report_ms);

  uint16_t GetRemoteDelay();

  const PcmConfiguration& LeAudioGetSelectedHalPcmConfig();

  void LeAudioSetSelectedHalPcmConfig(uint32_t sample_rate_hz, uint8_t bit_rate,
                                      uint8_t channels_count,
                                      uint32_t data_interval);

  bool IsPendingStartStream(void);
  void ClearPendingStartStream(void);

  static inline LeAudioSinkTransport* instance_unicast_ = nullptr;
  static inline LeAudioSinkTransport* instance_broadcast_ = nullptr;
  static inline BluetoothAudioSinkClientInterface* interface_unicast_ = nullptr;
  static inline BluetoothAudioSinkClientInterface* interface_broadcast_ = nullptr;

 private:
  LeAudioTransport* transport_;
};

class LeAudioSourceTransport
    : public ::bluetooth::audio::aidl::IBluetoothSourceTransportInstance {
 public:
  LeAudioSourceTransport(SessionType session_type, StreamCallbacks stream_cb);

  ~LeAudioSourceTransport();

  BluetoothAudioCtrlAck StartRequest(bool is_low_latency) override;

  BluetoothAudioCtrlAck SuspendRequest() override;

  void StopRequest() override;

  bool GetPresentationPosition(uint64_t* remote_delay_report_ns,
                               uint64_t* total_bytes_written,
                               timespec* data_position) override;

  void SourceMetadataChanged(const source_metadata_t& source_metadata) override;

  void SinkMetadataChanged(const sink_metadata_t& sink_metadata) override;

  tA2DP_CTRL_CMD GetPendingCmd() const;

  void ResetPendingCmd();

  void ResetPresentationPosition() override;

  void LogBytesWritten(size_t bytes_written) override;

  void SetRemoteDelay(uint16_t delay_report_ms);

  uint16_t GetRemoteDelay();

  const PcmConfiguration& LeAudioGetSelectedHalPcmConfig();

  void LeAudioSetSelectedHalPcmConfig(uint32_t sample_rate_hz, uint8_t bit_rate,
                                      uint8_t channels_count,
                                      uint32_t data_interval);

  bool IsPendingStartStream(void);
  void ClearPendingStartStream(void);

  static inline LeAudioSourceTransport* instance = nullptr;
  static inline BluetoothAudioSourceClientInterface* interface = nullptr;

 private:
  LeAudioTransport* transport_;
};

class LeAudioClientInterface {
 public:
  struct PcmParameters {
    uint32_t data_interval_us;
    uint32_t sample_rate;
    uint8_t bits_per_sample;
    uint8_t channels_count;
  };

 private:
  class IClientInterfaceEndpoint {
   public:
    virtual ~IClientInterfaceEndpoint() = default;
    virtual void Cleanup() = 0;
    virtual void SetPcmParameters(const PcmParameters& params) = 0;
    virtual void SetRemoteDelay(uint16_t delay_report_ms) = 0;
    virtual uint16_t GetRemoteDelay() = 0;
    virtual void StartSession() = 0;
    virtual void StopSession() = 0;
    virtual tA2DP_CTRL_CMD GetPendingCmd() = 0;
    virtual void ResetPendingCmd() = 0;
    virtual void UpdateAudioConfigToHal(
                 AudioConfigurationAIDL& offload_config) = 0;
    virtual void ConfirmStreamingRequest() = 0;
    virtual void CancelStreamingRequest() = 0;
    virtual void ConfirmSuspendRequest() = 0;
    virtual void CancelSuspendRequest() = 0;
    virtual void CancelSuspendRequestWithReconfig() = 0;
  };

 public:
  class Sink : public IClientInterfaceEndpoint {
   public:
    Sink(bool is_broadcaster = false) : is_broadcaster_(is_broadcaster){};
    virtual ~Sink() = default;

    void Cleanup() override;
    void SetPcmParameters(const PcmParameters& params) override;
    void SetRemoteDelay(uint16_t delay_report_ms) override;
    uint16_t GetRemoteDelay() override;
    void StartSession() override;
    void StopSession() override;
    tA2DP_CTRL_CMD GetPendingCmd() override;
    void ResetPendingCmd() override;
    void UpdateAudioConfigToHal(
                 AudioConfigurationAIDL& offload_config) override;
    void ConfirmStreamingRequest() override;
    void CancelStreamingRequest() override;
    void ConfirmSuspendRequest() override;
    void CancelSuspendRequest() override;
    void CancelSuspendRequestWithReconfig() override;
    void SetOffloadParameters(LeAudioConfiguration *lea_config);

    // Read the stream of bytes sinked to us by the upper layers
    size_t Read(uint8_t* p_buf, uint32_t len);
    bool IsBroadcaster() { return is_broadcaster_; }

   private:
    bool is_broadcaster_ = false;
  };
  class Source : public IClientInterfaceEndpoint {
   public:
    virtual ~Source() = default;

    void Cleanup() override;
    void SetPcmParameters(const PcmParameters& params) override;
    void SetRemoteDelay(uint16_t delay_report_ms) override;
    uint16_t GetRemoteDelay() override;
    void StartSession() override;
    void StopSession() override;
    tA2DP_CTRL_CMD GetPendingCmd() override;
    void ResetPendingCmd() override;
    void UpdateAudioConfigToHal(
                 AudioConfigurationAIDL& offload_config) override;
    void ConfirmStreamingRequest() override;
    void CancelStreamingRequest() override;
    void ConfirmSuspendRequest() override;
    void CancelSuspendRequest() override;
    void CancelSuspendRequestWithReconfig() override;
    void SetOffloadParameters(LeAudioConfiguration *lea_config);

    // Source the given stream of bytes to be sinked into the upper layers
    size_t Write(const uint8_t* p_buf, uint32_t len);
  };

  // Get LE Audio sink client interface if it's not previously acquired and not
  // yet released.
  Sink* GetSink(StreamCallbacks stream_cb,
                thread_t* message_loop,
                bool is_broadcasting_session_type);
  // This should be called before trying to get sink interface
  bool IsUnicastSinkAcquired();
  // This should be called before trying to get broadcast sink interface
  bool IsBroadcastSinkAcquired();
  // Release sink interface if belongs to LE audio client interface
  bool ReleaseSink(Sink* sink);

  // Get LE Audio source client interface if it's not previously acquired and
  // not yet released.
  Source* GetSource(StreamCallbacks stream_cb,
                    thread_t* message_loop);
  // This should be called before trying to get source interface
  bool IsSourceAcquired();
  // Release source interface if belongs to LE audio client interface
  bool ReleaseSource(Source* source);

  // Get interface, if previously not initialized - it'll initialize singleton.
  static LeAudioClientInterface* Get();

 private:
  static LeAudioClientInterface* interface;
  Sink* unicast_sink_ = nullptr;
  Sink* broadcast_sink_ = nullptr;
  Source* source_ = nullptr;
};

}  // namespace le_audio
}  // namespace aidl
}  // namespace audio
}  // namespace bluetooth
