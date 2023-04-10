/******************************************************************************
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/
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

#include "audio_a2dp_hw/include/audio_a2dp_hw.h"
#include <vendor/qti/hardware/bluetooth_audio/2.0/types.h>
#include "osi/include/thread.h"
#include "btif_av.h"
#include <hardware/audio.h>
//#include <aidl/android/hardware/bluetooth/audio/SessionType.h>

//using ::bluetooth::audio::aidl::SessionType;
using vendor::qti::hardware::bluetooth_audio::V2_0::SessionType;
using vendor::qti::hardware::bluetooth_audio::V2_0::SessionParamType;
//using SessionTypeAOSP =
 //    ::aidl::android::hardware::bluetooth::audio::SessionType;
#define AHIM_ENABLED 1
#define  RX_ONLY_CONFIG    0x1
#define  TX_ONLY_CONFIG    0x2
#define  TX_RX_BOTH_CONFIG 0x3

enum
{
  A2DP = 0x1,
  AUDIO_GROUP_MGR = 0x2,
  BROADCAST = 0x3,
  MAX_CLIENT
};

enum acm_evt{
  BTIF_ACM_PROCESS_HIDL_REQ_EVT = 0x1,
  BTIF_ACM_PROCESS_SRC_META_REQ_EVT = 0x2,
  BTIF_ACM_PROCESS_SNK_META_REQ_EVT = 0x3
};

#define BA_SIMULCAST 3
#define CHANNEL_MONO 0x00000000 //Mono/Unspecified
#define CHANNEL_FL   0x00000001 //Front Left
#define CHANNEL_FR   0x00000002 //Front Right
#define CHANNEL_FC   0x00000004 //Front Centre
#define CHANNEL_LFE  0x00000008 //Low Frequency Effect
#define CHANNEL_BL   0x00000010 //Back Left
#define CHANNEL_BR   0x00000020 //Back Right
#define FROM_AIR     0x01
#define TO_AIR       0x00

#define  ADV_AUD_CODEC_SAMPLE_RATE_NONE    0x0
#define  ADV_AUD_CODEC_SAMPLE_RATE_44100   0x1 << 0
#define  ADV_AUD_CODEC_SAMPLE_RATE_48000   0x1 << 1
#define  ADV_AUD_CODEC_SAMPLE_RATE_88200   0x1 << 2
#define  ADV_AUD_CODEC_SAMPLE_RATE_96000   0x1 << 3
#define  ADV_AUD_CODEC_SAMPLE_RATE_176400  0x1 << 4
#define  ADV_AUD_CODEC_SAMPLE_RATE_192000  0x1 << 5
#define  ADV_AUD_CODEC_SAMPLE_RATE_16000   0x1 << 6
#define  ADV_AUD_CODEC_SAMPLE_RATE_24000   0x1 << 7
#define  ADV_AUD_CODEC_SAMPLE_RATE_32000   0x1 << 8
#define  ADV_AUD_CODEC_SAMPLE_RATE_8000    0x1 << 9

#define WMCP_PROFILE       0x04
#define GCP_RX_PROFILE     0x20

#define RX_ONLY_CONFIG    0x1
#define TX_RX_BOTH_CONFIG 0x3

typedef void (* ahim_client_cb)(tA2DP_CTRL_CMD cmd, uint8_t direction);
typedef uint16_t (* ahim_get_sample_rate_callback)(uint8_t direction);
typedef uint8_t (* ahim_get_channel_mode_callback)(uint8_t direction);
typedef uint32_t (* ahim_get_bitrate_callback)(uint8_t direction);
typedef uint32_t (* ahim_get_mtu_callback)(uint32_t bit_rate, uint8_t direction);
typedef uint16_t (* ahim_get_frame_length)(uint8_t direction);
typedef uint8_t (* ahim_get_ch_count_callback)();
typedef bool (* ahim_get_simulcast_status)();
typedef uint16_t (* ahim_get_profile_status)();
typedef uint8_t (* ahim_get_codec_type)(uint8_t direction);
typedef bool (* ahim_is_codec_type_lc3q)(uint8_t direction);
typedef uint8_t (* ahim_get_codec_encoder_version)(uint8_t direction);
typedef uint8_t (* ahim_get_codec_decoder_version)(uint8_t direction);
typedef uint8_t (* ahim_get_min_sup_frame_dur)(uint8_t direction);
typedef uint8_t (* ahim_get_feature_map)(uint8_t direction);
typedef uint8_t (* ahim_lc3_blocks_per_sdu)(uint8_t direction);
typedef uint32_t (* ahim_get_audio_location)(uint32_t stream_id, uint8_t direction);
typedef void (* ahim_update_src_metadata)(const source_metadata_t& source_metadata);
typedef void (* ahim_update_snk_metadata)(const sink_metadata_t& sink_metadata);
typedef uint32_t (* ahim_get_mode_callback)();
typedef uint16_t (* ahim_get_frame_duration)(uint8_t direction);
typedef void (* ahim_update_params)(uint16_t delay, uint8_t mode);

typedef struct {
    uint8_t mode;
    ahim_client_cb client_cb;
    ahim_get_sample_rate_callback get_sample_rate_cb;
    ahim_get_channel_mode_callback get_channel_mode_cb;
    ahim_get_bitrate_callback get_bitrate_cb;
    ahim_get_mtu_callback get_mtu_cb;
    ahim_get_frame_length get_frame_length_cb;
    ahim_get_ch_count_callback get_ch_count_cb;
    ahim_get_simulcast_status get_simulcast_status_cb;
    ahim_get_profile_status get_profile_status_cb;
    ahim_get_codec_type get_codec_type;
    ahim_is_codec_type_lc3q get_is_codec_type_lc3q;
    ahim_get_codec_encoder_version get_codec_encoder_version;
    ahim_get_codec_decoder_version get_codec_decoder_version;
    ahim_get_min_sup_frame_dur get_min_sup_frame_dur;
    ahim_get_feature_map get_feature_map;
    ahim_lc3_blocks_per_sdu get_lc3_blocks_per_sdu;
    ahim_get_audio_location get_audio_location;
    ahim_update_src_metadata src_meta_update;
    ahim_update_snk_metadata snk_meta_update;
    ahim_get_mode_callback get_mode_cb;
    ahim_get_frame_duration get_frame_duration;
    ahim_update_params params_update;
}btif_ahim_client_callbacks_t;

extern btif_ahim_client_callbacks_t* pclient_cbs[MAX_CLIENT];

void reg_cb_with_ahim(uint8_t client_id, btif_ahim_client_callbacks_t* pclient_cb);

void btif_ahim_process_request(tA2DP_CTRL_CMD cmd, uint8_t profile, uint8_t direction);
void btif_ahim_update_current_profile(uint8_t profile);

void btif_ahim_update_src_metadata(const source_metadata_t& source_metadata);
void btif_ahim_update_sink_metadata(const sink_metadata_t& sink_metadata);

bool btif_ahim_init_hal(thread_t *t, uint8_t profile);

void btif_ahim_cleanup_hal(uint8_t profile);

bool btif_ahim_is_hal_2_0_supported();

bool btif_ahim_is_hal_2_0_enabled();

bool btif_ahim_is_qc_hal_enabled();

bool btif_ahim_is_qc_lea_enabled();

bool btif_ahim_is_aosp_aidl_hal_enabled();

bool btif_ahim_is_restart_session_needed(uint8_t profile);

void btif_ahim_update_session_params(SessionParamType param_type);

void btif_ahim_update_audio_config();

uint16_t btif_ahim_get_remote_delay();

bool btif_ahim_setup_codec(uint8_t profile);

void btif_ahim_start_session(uint8_t profile);

void btif_ahim_end_session(uint8_t profile);

tA2DP_CTRL_CMD btif_ahim_get_pending_command(uint8_t profile);

tA2DP_CTRL_CMD btif_ahim_get_pending_command(uint8_t profile,
                                                          uint8_t direction);

void btif_ahim_reset_pending_command(uint8_t profile);

void btif_ahim_update_params (uint16_t delay, uint8_t mode);

void btif_ahim_reset_pending_command(uint8_t profile, uint8_t direction);

void btif_ahim_update_pending_command(tA2DP_CTRL_CMD cmd, uint8_t profile);

void btif_ahim_ack_stream_started(const tA2DP_CTRL_ACK& ack, uint8_t profile);

void btif_ahim_ack_stream_suspended(const tA2DP_CTRL_ACK& ack, uint8_t profile);

void btif_ahim_ack_stream_direction_suspended(const tA2DP_CTRL_ACK& ack, uint8_t profile, uint8_t direction);

void btif_ahim_ack_stream_profile_suspended(const tA2DP_CTRL_ACK& ack,
                                           uint8_t profile,
                                           uint16_t sub_profile);

size_t btif_ahim_read(uint8_t* p_buf, uint32_t len);

void btif_ahim_set_remote_delay(uint16_t delay_report, uint8_t profile);

bool btif_ahim_is_streaming();

void btif_ahim_signal_src_metadata_complete();

void btif_ahim_signal_snk_metadata_complete();

SessionType btif_ahim_get_session_type(uint8_t profile);

bool btif_ahim_update_codec_offloading_capabilities(
    const std::vector<btav_a2dp_codec_config_t>& framework_preference);
