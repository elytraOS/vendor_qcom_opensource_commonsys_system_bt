/******************************************************************************
 *
 *  Copyright (C) 2014 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  Changes from Qualcomm Innovation Center are provided under the following license:
 *
 *  Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted (subject to the limitations in the
 *  disclaimer below) provided that the following conditions are met:
 *
 *  Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *  Redistributions in binary form must reproduce the above
 *  copyright notice, this list of conditions and the following
 *  disclaimer in the documentation and/or other materials provided
 *  with the distribution.
 *
 *  Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *  contributors may be used to endorse or promote products derived
 *  from this software without specific prior written permission.
 *
 *  NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 *  GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 *  HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 *  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 *
 ******************************************************************************/

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device_features.h"
#include "hci_layer.h"
#include "hci_packet_factory.h"
#include "hci_packet_parser.h"
#include "osi/include/log.h"
#include "utils/include/bt_utils.h"
#include <btcommon_interface_defs.h>

static const char CONTROLLER_MODULE[] = "controller_module";

typedef struct controller_t {
  bool (*get_is_ready)(void);

  const RawAddress* (*get_address)(void);
  const bt_version_t* (*get_bt_version)(void);

  const bt_device_features_t* (*get_features_classic)(int index);
  uint8_t (*get_last_features_classic_index)(void);

  const bt_device_features_t* (*get_features_ble)(void);
  const bt_antenna_info_t* (*get_antenna_info_ble)(void);
  const uint8_t* (*get_ble_supported_states)(void);

  bool (*supports_simple_pairing)(void);
  bool (*supports_secure_connections)(void);
  bool (*supports_simultaneous_le_bredr)(void);
  bool (*supports_reading_remote_extended_features)(void);
  bool (*supports_interlaced_inquiry_scan)(void);
  bool (*supports_rssi_with_inquiry_results)(void);
  bool (*supports_extended_inquiry_response)(void);
  bool (*supports_master_slave_role_switch)(void);
  bool (*supports_enhanced_setup_synchronous_connection)(void);
  bool (*supports_enhanced_accept_synchronous_connection)(void);
  bool (*supports_set_min_encryption_key_size)(void);

  bool (*supports_ble)(void);
  bool (*supports_ble_packet_extension)(void);
  bool (*supports_ble_connection_parameters_request)(void);
  bool (*supports_ble_privacy)(void);
  bool (*supports_ble_set_privacy_mode)(void);
  bool (*supports_ble_2m_phy)(void);
  bool (*supports_ble_coded_phy)(void);
  bool (*supports_ble_extended_advertising)(void);
  bool (*supports_ble_periodic_advertising)(void);
  bool (*supports_ble_periodic_sync_transfer)(void);
  bool (*supports_ble_iso_broadcaster)(void);
  bool (*supports_ble_periodic_advertising_adi)(void);
  bool (*supports_ble_aoa)(void);

  // Get the cached acl data sizes for the controller.
  uint16_t (*get_acl_data_size_classic)(void);
  uint16_t (*get_acl_data_size_ble)(void);

  // Get the cached acl packet sizes for the controller.
  // This is a convenience function for the respective
  // acl data size + size of the acl header.
  uint16_t (*get_acl_packet_size_classic)(void);
  uint16_t (*get_acl_packet_size_ble)(void);

  uint16_t (*get_ble_default_data_packet_length)(void);
  uint16_t (*get_ble_maxium_advertising_data_length)(void);
  uint8_t (*get_ble_number_of_supported_advertising_sets)(void);

  // Get the number of acl packets the controller can buffer.
  uint16_t (*get_acl_buffer_count_classic)(void);
  uint8_t (*get_acl_buffer_count_ble)(void);

  uint8_t (*get_ble_white_list_size)(void);

  uint8_t (*get_ble_resolving_list_max_size)(void);
  void (*set_ble_resolving_list_max_size)(int resolving_list_max_size);
  uint8_t* (*get_local_supported_codecs)(uint8_t* number_of_codecs);
  bool (*supports_ble_offload_features)(void);
  uint8_t (*get_le_all_initiating_phys)(void);
  uint8_t* (*get_scrambling_supported_freqs)(uint8_t* number_of_freqs);
  const bt_device_soc_add_on_features_t* (*get_soc_add_on_features)(uint8_t *add_on_features_len);
  uint16_t (*get_product_id)(void);
  uint16_t (*get_response_version)(void);
  const bt_device_host_add_on_features_t*
      (*get_host_add_on_features)(uint8_t *add_on_features_len);
  bool (*supports_read_simple_pairing_options)(void);
  bool (*performs_remote_public_key_validation)(void);
  bt_soc_type_t (*get_soc_type)();
  const char* (*get_a2dp_offload_cap)();
  bool (*supports_spilt_a2dp)();
  bool (*supports_aac_frame_ctl)();
  bool (*supports_wipower)();
  bool (*is_multicast_enabled)();
  bool (*supports_twsp_remote_state)();
  uint32_t* (*get_vs_supported_codecs)(uint8_t* number_of_codecs);
  uint16_t (*get_ble_iso_data_packet_len)(void);
  uint8_t (*get_ble_iso_num_data_packets)(void);
  uint8_t (*get_std_supported_codec_transport)(uint8_t std_codec_id);
  uint8_t (*get_vs_supported_codec_transport)(uint32_t vs_codec_id);
  bool (*is_host_iso_channel_supported)(void);
  bool (*is_cis_master_role_supported)(void);
  bool (*is_cis_slave_role_supported)(void);
  bool (*is_pow_ctr_req_supported)(void);
  bool (*is_pathloss_monitoring_supported)(void);
  bool (*get_max_power_values)(uint8_t *);
  bool (*is_adv_audio_supported)(void);
  bool (*is_qbce_QLE_HCI_supported)(void);
  bool (*is_qbce_QCM_HCI_supported)(void);
  const bt_device_qll_local_supported_features_t* (*get_qll_features)(void);
  bool (*is_conn_subrating_supported)(void);
  bool (*is_conn_subrating_host_supported)(void);
} controller_t;

const controller_t* controller_get_interface();

const controller_t* controller_get_test_interface(
    const hci_t* hci_interface,
    const hci_packet_factory_t* packet_factory_interface,
    const hci_packet_parser_t* packet_parser_interface);

bool is_soc_lpa_enh_pwr_enabled();
