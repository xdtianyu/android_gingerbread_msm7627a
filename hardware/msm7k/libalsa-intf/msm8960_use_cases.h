/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
 */
#ifndef _MSM8960_USE_CASES_H_
#define _MSM8960_USE_CASES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "alsa_ucm.h"
#include "alsa_audio.h"

#define SND_UCM_END_OF_LIST "end"

/* mixer control structure */
typedef struct mixer_control {
    const char *control_name;
    const char *ena_string;
    const char *dis_string;
    unsigned type;
    unsigned ena_value;
    unsigned dis_value;
}mixer_control_t;

/* Use case mixer controls structure */
typedef struct card_mctrl {
    const char *case_name;
    int mixer_control_count;
    mixer_control_t *mixer_list;
    const char *dev_name;
}card_mctrl_t;

/* identifier node structure for identifier list*/
struct snd_ucm_ident_node {
    char *ident;
    struct snd_ucm_ident_node *next;
};

/* SND card context structure */
typedef struct card_ctxt {
    const char *card_name;
    int card_number;
    const char *control_device;
    struct mixer *mixer_handle;
    char *current_verb;
    struct snd_ucm_ident_node *dev_list_head;
    struct snd_ucm_ident_node *mod_list_head;
    pthread_mutex_t card_lock;
    pthread_mutexattr_t card_lock_attr;
    card_mctrl_t *card_ctrl;
}card_ctxt_t;

/** use case manager structure */
struct snd_use_case_mgr {
    int snd_card_index;
    int device_list_count;
    int modifier_list_count;
    char **current_device_list;
    char **current_modifier_list;
    card_ctxt_t *card_ctxt_ptr;
};

/* Mixer controls lists per use case/device */
static mixer_control_t HiFi_controls[] = {
    {"SLIMBUS_0_RX Audio Mixer MultiMedia1", NULL, NULL, 0, 1, 0},
};

static mixer_control_t HiFiRec_controls[] = {
    {"MultiMedia1 Mixer SLIM_0_TX", NULL, NULL, 0, 1, 0},
};

static mixer_control_t HiFiRx_controls[] = {
    {"SLIMBUS_0_RX Audio Mixer MultiMedia1", NULL, NULL, 0, 1, 0},
};

static mixer_control_t HiFiTx_controls[] = {
    {"MultiMedia1 Mixer SLIM_0_TX", NULL, NULL, 0, 1, 0},
};

static mixer_control_t HiFiBluetooth_controls[] = {
    {"INTERNAL_BT_SCO_RX Audio Mixer MultiMedia1", NULL, NULL, 0, 1, 0},
};

static mixer_control_t HiFiBTSCORx_controls[] = {
    {"INTERNAL_BT_SCO_RX Audio Mixer MultiMedia1", NULL, NULL, 0, 1, 0},
};

static mixer_control_t HiFiBTSCOTx_controls[] = {
    {"INTERNAL_BT_SCO_RX Audio Mixer MultiMedia1", NULL, NULL, 0, 1, 0},
};

static mixer_control_t HiFiHDMI_controls[] = {
    {"HDMI Mixer MultiMedia1", NULL, NULL, 0, 1, 0},
};

static mixer_control_t HiFiLP_controls[] = {
    {"SLIMBUS_0_RX Audio Mixer MultiMedia3", NULL, NULL, 0, 1, 0},
};

static mixer_control_t VoiceCall_controls[] = {
    {"SLIM_0_RX_Voice Mixer CSVoice", NULL, NULL, 0, 1, 0},
    {"Voice_Tx Mixer SLIM_0_TX_Voice", NULL, NULL, 0, 1, 0},
};

static mixer_control_t VoiceCallRx_controls[] = {
    {"SLIM_0_RX_Voice Mixer CSVoice", NULL, NULL, 0, 1, 0},
};

static mixer_control_t VoiceCallTx_controls[] = {
    {"Voice_Tx Mixer SLIM_0_TX_Voice", NULL, NULL, 0, 1, 0},
};

static mixer_control_t VoiceCallBTSCORx_controls[] = {
    {"INTERNAL_BT_SCO_RX_Voice Mixer CSVoice", NULL, NULL, 0, 1, 0},
};

static mixer_control_t VoiceCallBTSCOTx_controls[] = {
    {"INTERNAL_BT_SCO_RX_Voice Mixer CSVoice", NULL, NULL, 0, 1, 0},
    {"Voice_Tx Mixer INTERNAL_BT_SCO_TX_Voice", NULL, NULL, 0, 1, 0},
    {"SLIM_0_RX_Voice Mixer CSVoice", NULL, NULL, 0, 1, 0},
    {"Voice_Tx Mixer SLIM_0_TX_Voice", NULL, NULL, 0, 1, 0},
};

static mixer_control_t VoipCall_controls[] = {
    {"SLIM_0_RX_Voice Mixer Voip", NULL, NULL, 0, 1, 0},
};

static mixer_control_t FMDigitalRadio_controls[] = {
    {"SLIMBUS_0_RX Port Mixer INTERNAL_FM_TX", NULL, NULL, 0, 1, 0},
};

static mixer_control_t FMREC_controls[] = {
    {"MultMedia1 Mixer INTERNAL_FM_TX", NULL, NULL, 0, 1, 0},
};

static mixer_control_t Speaker_controls[] = {
    {"RX3 MIX1 INP1", "RX1", "ZERO", 1, 0, 0},
    {"RX4 MIX1 INP1", "RX2", "ZERO", 1, 0, 0},
    {"LINEOUT1 DAC Switch", NULL, NULL, 0, 1, 0},
    {"LINEOUT3 DAC Switch", NULL, NULL, 0, 1, 0},
    {"Speaker Function", "On", "Off", 1, 0, 0},
    {"LINEOUT1 Volume", NULL, NULL, 2, 100, 0},
    {"LINEOUT3 Volume", NULL, NULL, 2, 100, 0},
};

static mixer_control_t Headset_controls[] = {
    {"SLIM TX7 MUX", "DEC5", "ZERO", 1, 0, 0},
    {"DEC5 MUX", "ADC2", "ZERO", 1, 0, 0},
    {"DEC5 Volume", NULL, NULL, 2, 0, 0},
};

static mixer_control_t Handset_controls[] = {
    {"SLIM TX7 MUX", "DEC6", "ZERO", 1, 0, 0},
    {"DEC6 Volume", NULL, NULL, 2, 0, 0},
    {"DEC6 MUX", "ADC1", "ZERO", 1, 0, 0},
};

static mixer_control_t Line_controls[] = {
    {"SLIM TX7 MUX", "DEC1", "ZERO", 1, 0, 0},
    {"DEC1 MUX", "DMIC1", "ZERO", 1, 0, 0},
    {"MICBIAS1 CAPLESS Switch", NULL, NULL, 0, 1, 0},
};

static mixer_control_t Headphones_controls[] = {
    {"RX1 MIX1 INP1", "RX1", "ZERO", 1, 0, 0},
    {"RX2 MIX1 INP1", "RX2", "ZERO", 1, 0, 0},
    {"HPHL DAC Switch", NULL, NULL, 0, 1, 0},
    {"HPHR DAC Switch", NULL, NULL, 0, 1, 0},
    {"HPHL Volume", NULL, NULL, 2, 100, 100},
    {"HPHR Volume", NULL, NULL, 2, 100, 100},
    {"RX1 Digital Volume", NULL, NULL, 2, 0, 0},
    {"RX2 Digital Volume", NULL, NULL, 2, 0, 0},
};

static mixer_control_t Speaker_Headset_controls[] = {
    {"RX3 MIX1 INP1", "RX1", "ZERO", 1, 0, 0},
    {"RX4 MIX1 INP1", "RX2", "ZERO", 1, 0, 0},
    {"LINEOUT1 DAC Switch", NULL, NULL, 0, 1, 0},
    {"LINEOUT3 DAC Switch", NULL, NULL, 0, 1, 0},
    {"Speaker Function", "On", "Off", 1, 0, 0},
    {"LINEOUT1 Volume", NULL, NULL, 2, 100, 0},
    {"LINEOUT3 Volume", NULL, NULL, 2, 100, 0},
    {"RX1 MIX1 INP1", "RX1", "ZERO", 1, 0, 0},
    {"RX2 MIX1 INP1", "RX2", "ZERO", 1, 0, 0},
    {"HPHL DAC Switch", NULL, NULL, 0, 1, 0},
    {"HPHR DAC Switch", NULL, NULL, 0, 1, 0},
    {"HPHL Volume", NULL, NULL, 2, 100, 100},
    {"HPHR Volume", NULL, NULL, 2, 100, 100},
    {"RX1 Digital Volume", NULL, NULL, 2, 0, 0},
    {"RX2 Digital Volume", NULL, NULL, 2, 0, 0},
};

static mixer_control_t Speaker_FM_Tx_controls[] = {
    {"RX3 MIX1 INP1", "RX1", "ZERO", 1, 0, 0},
    {"RX4 MIX1 INP1", "RX2", "ZERO", 1, 0, 0},
    {"LINEOUT1 DAC Switch", NULL, NULL, 0, 1, 0},
    {"LINEOUT3 DAC Switch", NULL, NULL, 0, 1, 0},
    {"Speaker Function", "On", "Off", 1, 0, 0},
    {"LINEOUT1 Volume", NULL, NULL, 2, 100, 0},
    {"LINEOUT3 Volume", NULL, NULL, 2, 100, 0},
};

static mixer_control_t Earpiece_controls[] = {
    {"RX1 MIX1 INP1", "RX1", "ZERO", 1, 0, 0},
    {"DAC1 Switch", NULL, NULL, 0, 1, 0},
    {"RX1 Digital Volume", NULL, NULL, 2, 0, 0},
};

static mixer_control_t Bluetooth_controls[] = {
    {"INTERNAL_BT_SCO_RX Audio Mixer MultiMedia1", NULL, NULL, 0, 1, 0},
};

static mixer_control_t CaptureVoice_controls[] = {
    {"Voice_Tx Mixer SLIM_0_TX_Voice", NULL, NULL, 0, 1, 0},
};

static mixer_control_t CaptureVOIP_controls[] = {
    {"Voip_Tx Mixer SLIM_0_TX_Voip", NULL, NULL, 0, 1, 0},
};

static mixer_control_t CaptureMusic_controls[] = {
    {"MultiMedia1 Mixer SLIM_0_TX", NULL, NULL, 0, 1, 0},
};

/* Mixer controls mapping per use case/device */
static card_mctrl_t snd_soc_msm_ctrl[] = {
    { "HiFi", 2, HiFi_controls, "hw:0,0",
    },
    { "HiFi Rec", 2, HiFiRec_controls, "hw:0,0",
    },
    { "HiFiSpeaker", 1, HiFiRx_controls, "hw:0,0",
    },
    { "HiFi RecLine", 1, HiFiTx_controls, "hw:0,0",
    },
    { "HiFiHeadphones", 1, HiFiRx_controls, "hw:0,0",
    },
    { "HiFi RecHeadset", 1, HiFiTx_controls, "hw:0,0",
    },
    { "HiFi RecHandset", 1, HiFiTx_controls, "hw:0,0",
    },
    { "HiFiEarpiece", 1, HiFiRx_controls, "hw:0,0",
    },
    { "HiFiANC Headset", 1, HiFiRx_controls, "hw:0,0",
    },
    { "HiFiSpeaker Headset", 1, HiFiRx_controls, "hw:0,0",
    },
    { "HiFiSpeaker FM Tx", 0, NULL, "hw:0,0", /*TODO*/
    },
    { "HiFiHDMI", 1, HiFiHDMI_controls, "hw:0,0",
    },
    { "HiFiBluetooth", 1, HiFiBluetooth_controls, "hw:0,0",
    },
    { "HiFiBT SCO Rx", 1, HiFiBTSCORx_controls, "hw:0,0",
    },
    { "HiFi RecBT SCO Tx", 1, HiFiBTSCOTx_controls, "hw:0,0",
    },
    { "HiFi Low Power", 1, HiFiLP_controls, "hw:0,6",
    },
    { "HiFi Low PowerSpeaker", 1, HiFiLP_controls, "hw:0,6",
    },
    { "HiFi Low PowerHeadphones", 1, HiFiLP_controls, "hw:0,6",
    },
    { "HiFi Low PowerEarpiece", 1, HiFiLP_controls, "hw:0,6",
    },
    { "HiFi Low PowerANC Headset", 1, HiFiLP_controls, "hw:0,6",
    },
    { "HiFi Low PowerSpeaker Headset", 1, HiFiLP_controls, "hw:0,6",
    },
    { "HiFi Low PowerBluetooth", 1, HiFiLP_controls, "hw:0,6",
    },
    { "HiFi Low PowerFM Tx", 1, HiFiLP_controls, "hw:0,6", /*TODO: Is this valid case?? */
    },
    { "HiFi Low PowerHDMI", 1, HiFiLP_controls, "hw:0,6",
    },
    { "HiFi Low PowerBT SCO Rx", 1, HiFiBTSCORx_controls, "hw:0,6", /* TODO */
    },
    { "HiFi Low PowerBT SCO Tx", 1, HiFiBTSCOTx_controls, "hw:0,6", /* TODO */
    },
    { "Voice Call", 2, VoiceCall_controls, "hw:0,2",
    },
    { "Voice CallSpeaker", 1, VoiceCallRx_controls, "hw:0,2",
    },
    { "Voice CallLine", 1, VoiceCallTx_controls, "hw:0,2",
    },
    { "Voice CallHeadphones", 1, VoiceCallRx_controls, "hw:0,2",
    },
    { "Voice CallHeadset", 1, VoiceCallTx_controls, "hw:0,2",
    },
    { "Voice CallANC Headset", 1, VoiceCallRx_controls, "hw:0,2",
    },
    { "Voice CallHandset", 1, VoiceCallTx_controls, "hw:0,2",
    },
    { "Voice CallEarpiece", 1, VoiceCallRx_controls, "hw:0,2",
    },
    { "Voice CallSpeaker Headset", 1, VoiceCallRx_controls, "hw:0,2",
    },
    { "Voice CallSpeaker FM Tx", 1, VoiceCallRx_controls, "hw:0,2",
    },
    { "Voice CallBluetooth", 1, VoiceCallRx_controls, "hw:0,2",
    },
    { "Voice CallFM Tx", 1, VoiceCallRx_controls, "hw:0,2", /* Valid case?? */
    },
    { "Voice CallBT SCO Rx", 0, VoiceCallBTSCORx_controls, "hw:0,2",
    },
    { "Voice CallBT SCO Tx", 4, VoiceCallBTSCOTx_controls, "hw:0,2",
    },
    { "Voice CallHDMI", 1, VoiceCallRx_controls, "hw:0,2", /* Valid case?? */
    },
    { "Voice Call IP", 1, VoipCall_controls, "hw:0,3",
    },
    { "Voice Call IPSpeaker", 1, VoipCall_controls, "hw:0,3",
    },
    { "Voice Call IPLine", 1, VoipCall_controls, "hw:0,3",
    },
    { "Voice Call IPHeadphones", 1, VoipCall_controls, "hw:0,3",
    },
    { "Voice Call IPHeadset", 1, VoipCall_controls, "hw:0,3",
    },
    { "Voice Call IPHandset", 1, VoipCall_controls, "hw:0,3",
    },
    { "Voice Call IPANC Headset", 1, VoipCall_controls, "hw:0,3",
    },
    { "Voice Call IPSpeaker Headset", 1, VoipCall_controls, "hw:0,3",
    },
    { "Voice Call IPSpeaker FM Tx", 1, VoipCall_controls, "hw:0,3",
    },
    { "Voice Call IPBluetooth", 1, VoipCall_controls, "hw:0,3",
    },
    { "Voice Call IPEarpiece", 1, VoipCall_controls, "hw:0,3",
    },
    { "Voice Call IPFM Tx", 1, VoipCall_controls, "hw:0,3",
    },
    { "Voice Call IPBT SCO Rx", 1, VoipCall_controls, "hw:0,3",
    },
    { "Voice Call IPBT SCO Tx", 1, VoipCall_controls, "hw:0,3",
    },
    { "Voice Call IPHDMI", 1, VoipCall_controls, "hw:0,3",
    },
    { "FM Digital Radio", 1, FMDigitalRadio_controls, NULL,
    },
    { "FM Digital RadioSpeaker", 1, FMDigitalRadio_controls, NULL,
    },
    { "FM Digital RadioHeadphones", 1, FMDigitalRadio_controls, NULL,
    },
    { "FM Digital RadioEarpiece", 1, FMDigitalRadio_controls, NULL,
    },
    { "FM Digital RadioANC Headset", 1, FMDigitalRadio_controls, NULL,
    },
    { "FM Digital RadioSpeaker Headset", 1, FMDigitalRadio_controls, NULL,
    },
    { "FM Digital RadioSpeaker FM Tx", 1, FMDigitalRadio_controls, NULL, /* valid case?? */
    },
    { "FM Digital RadioBluetooth", 1, FMDigitalRadio_controls, NULL,
    },
    { "FM Digital RadioFM Tx", 1, FMDigitalRadio_controls, NULL,
    },
    { "FM Digital RadioHDMI", 1, FMDigitalRadio_controls, NULL,
    },
    { "FM RECLine", 1, FMREC_controls, "hw:0,1",
    },
    { "FM RECHeadset", 1, FMREC_controls, "hw:0,1",
    },
    { "FM RECHandset", 1, FMREC_controls, "hw:0,1",
    },
    { "FM RECBT SCO Rx", 0, NULL, "hw:0,1", /* Invalid case? */
    },
    { "FM RECBT SCO Tx", 0, NULL, "hw:0,1", /* Invalid case? */
    },
    { "FM Tx", 0, NULL, "hw:0,5",
    },
    { "Speaker", 7, Speaker_controls, NULL,
    },
    { "Headset", 3, Headset_controls, NULL,
    },
    { "ANC Headset", 8, Headphones_controls, NULL,
    },
    { "Handset", 3, Handset_controls, NULL,
    },
    { "Line", 3, Line_controls, NULL,
    },
    { "Headphones", 8, Headphones_controls, NULL,
    },
    { "Earpiece", 3, Earpiece_controls, NULL,
    },
    { "Bluetooth", 1, Bluetooth_controls, NULL,
    },
    { "BT SCO Rx", 0, NULL, "hw:0,2",
    },
    { "BT SCO Tx", 0, NULL, "hw:0,2",
    },
    { "Speaker Headset", 15, Speaker_Headset_controls, NULL,
    },
    { "Speaker FM Tx", 6, Speaker_FM_Tx_controls, "hw:0,5",
    },
    { "HDMI", 0, NULL, "hw:0,6",
    },
    { "Capture Voice", 1, CaptureVoice_controls, "hw:0,2",
    },
    { "Capture VoiceLine", 1, CaptureVoice_controls, "hw:0,2",
    },
    { "Capture VoiceHeadset", 1, CaptureVoice_controls, "hw:0,2",
    },
    { "Capture VoiceHandset", 1, CaptureVoice_controls, "hw:0,2",
    },
    { "Capture VoiceBT SCO Rx", 0, VoiceCallBTSCORx_controls, "hw:0,2",
    },
    { "Capture VoiceBT SCO Tx", 4, VoiceCallBTSCOTx_controls, "hw:0,2",
    },
    { "Capture VoiceBluetooth", 1, CaptureVoice_controls, "hw:0,2",
    },
    { "Capture VOIP", 1, CaptureVOIP_controls, "hw:0,3",
    },
    { "Capture VOIPLine", 1, CaptureVOIP_controls, "hw:0,3",
    },
    { "Capture VOIPHeadset", 1, CaptureVOIP_controls, "hw:0,3",
    },
    { "Capture VOIPHandset", 1, CaptureVOIP_controls, "hw:0,3",
    },
    { "Capture VOIPBluetooth", 1, CaptureVOIP_controls, "hw:0,3",
    },
    { "Capture VOIPBT SCO Rx", 1, CaptureVOIP_controls, "hw:0,3",
    },
    { "Capture VOIPBT SCO Tx", 1, CaptureVOIP_controls, "hw:0,3",
    },
    { "Play Music", 2, HiFi_controls, "hw:0,0",
    },
    { "Play MusicSpeaker", 1, HiFiRx_controls, "hw:0,0",
    },
    { "Play MusicHeadphones", 1, HiFiRx_controls, "hw:0,0",
    },
    { "Play MusicEarpiece", 1, HiFiRx_controls, "hw:0,0",
    },
    { "Play MusicANC Headset", 1, HiFiRx_controls, "hw:0,0",
    },
    { "Play MusicSpeaker Headset", 1, HiFiRx_controls, "hw:0,0",
    },
    { "Play MusicSpeaker FM Tx", 1, HiFiRx_controls, "hw:0,0",
    },
    { "Play MusicFM Tx", 1, HiFiRx_controls, "hw:0,0",
    },
    { "Play MusicBluetooth", 1, HiFiRx_controls, "hw:0,0",
    },
    { "Play MusicHDMI", 1, HiFiHDMI_controls, "hw:0,0",
    },
    { "Capture MusicLine", 1, HiFiTx_controls, "hw:0,0",
    },
    { "Capture MusicHeadset", 1, HiFiTx_controls, "hw:0,0",
    },
    { "Capture MusicHandset", 1, HiFiTx_controls, "hw:0,0",
    },
    { "Capture MusicBT SCO Rx", 1, HiFiBTSCORx_controls, "hw:0,0",
    },
    { "Capture MusicBT SCO Tx", 1, HiFiBTSCOTx_controls, "hw:0,0",
    },
    { "Capture MusicBluetooth", 1, HiFiTx_controls, "hw:0,0",
    },
    { "Play Voice", 2, VoiceCall_controls, "hw:0,2",
    },
    { "Play VoiceSpeaker", 1, VoiceCallRx_controls, "hw:0,2",
    },
    { "Play VoiceLine", 1, VoiceCallTx_controls, "hw:0,2",
    },
    { "Play VoiceHeadphones", 1, VoiceCallRx_controls, "hw:0,2",
    },
    { "Play VoiceHeadset", 1, VoiceCallTx_controls, "hw:0,2",
    },
    { "Play VoiceHandset", 1, VoiceCallTx_controls, "hw:0,2",
    },
    { "Play VoiceANC Headset", 1, VoiceCallRx_controls, "hw:0,2",
    },
    { "Play VoiceSpeaker Headset", 1, VoiceCallRx_controls, "hw:0,2",
    },
    { "Play VoiceSpeaker FM Tx", 1, VoiceCallRx_controls, "hw:0,2",
    },
    { "Play VoiceBluetooth", 1, VoiceCallRx_controls, "hw:0,2",
    },
    { "Play VoiceEarpiece", 1, VoiceCallRx_controls, "hw:0,2",
    },
    { "Play VoiceFM Tx", 1, VoiceCallRx_controls, "hw:0,2",
    },
    { "Play VoiceBT SCO Rx", 0, VoiceCallBTSCORx_controls, "hw:0,2",
    },
    { "Play VoiceBT SCO Tx", 4, VoiceCallBTSCOTx_controls, "hw:0,2",
    },
    { "Play VoiceHDMI", 1, VoiceCallRx_controls, "hw:0,2",
    },
    { SND_UCM_END_OF_LIST, 0, NULL, NULL,
    },
};

/* Context list per each sound card */
static card_ctxt_t card_ctxt_list[] = {
    { "snd_soc_msm", 0, "/dev/snd/controlC0", NULL, NULL, NULL, NULL, NULL, NULL, snd_soc_msm_ctrl,
    },
};

#define MAX_NUM_CARDS (sizeof(card_ctxt_list)/sizeof(struct card_ctxt))

/* Valid sound cards list */
static const char *card_list[] = {
    "snd_soc_msm",
};

/* New use cases, devices and modifiers added
 * which are not part of existing macros
 */
#define SND_USE_CASE_VERB_FM_REC         "FM REC"
#define SND_USE_CASE_VERB_HIFI_REC       "HiFi Rec"

#define SND_USE_CASE_DEV_FM_TX           "FM Tx"
#define SND_USE_CASE_DEV_ANC_HEADSET     "ANC Headset"
#define SND_USE_CASE_DEV_BTSCO_RX        "BT SCO Rx"
#define SND_USE_CASE_DEV_BTSCO_TX        "BT SCO Tx"
#define SND_USE_CASE_DEV_SPEAKER_HEADSET "Speaker Headset"
#define SND_USE_CASE_DEV_SPEAKER_FM_TX   "Speaker FM Tx"
#define SND_USE_CASE_DEV_ALL             "all devices"

#define SND_USE_CASE_MOD_PLAY_FM         "FM Digital Radio"
#define SND_USE_CASE_MOD_CAPTURE_FM      "FM REC"
#define SND_USE_CASE_MOD_PLAY_LPA        "HiFi Low Power"
#define SND_USE_CASE_MOD_PLAY_VOIP       "Voice Call IP"
#define SND_USE_CASE_MOD_CAPTURE_VOIP    "Capture VOIP"

/* List of valid use cases */
static const char *verb_list[] = {
    SND_USE_CASE_VERB_INACTIVE,
    SND_USE_CASE_VERB_HIFI,
    SND_USE_CASE_VERB_HIFI_LOW_POWER,
    SND_USE_CASE_VERB_VOICECALL,
    SND_USE_CASE_VERB_IP_VOICECALL,
    SND_USE_CASE_VERB_DIGITAL_RADIO,
    SND_USE_CASE_VERB_FM_REC,
    SND_USE_CASE_VERB_HIFI_REC,
    SND_UCM_END_OF_LIST,
};

/* List of valid devices */
static const char *device_list[] = {
    SND_USE_CASE_DEV_NONE,
    SND_USE_CASE_DEV_SPEAKER,    /* SPEAKER RX */
    SND_USE_CASE_DEV_LINE,       /* BUILTIN-MIC TX */
    SND_USE_CASE_DEV_HEADPHONES, /* HEADSET RX */
    SND_USE_CASE_DEV_HEADSET,    /* HEADSET TX */
    SND_USE_CASE_DEV_HANDSET,    /* HANDSET TX */
    SND_USE_CASE_DEV_EARPIECE,   /* HANDSET RX */
    SND_USE_CASE_DEV_ANC_HEADSET, /* ANC HEADSET RX */
    SND_USE_CASE_DEV_SPEAKER_HEADSET, /* COMBO SPEAKER+HEADSET RX */
    SND_USE_CASE_DEV_SPEAKER_FM_TX, /* COMBO SPEAKER+FM_TX RX */
    SND_USE_CASE_DEV_BTSCO_RX,      /* BTSCO RX*/
    SND_USE_CASE_DEV_BTSCO_TX,      /* BTSCO TX*/
    SND_USE_CASE_DEV_FM_TX,       /* FM RX */
    SND_USE_CASE_DEV_BLUETOOTH,  /* BLUETOOTH */
    SND_USE_CASE_DEV_HDMI,       /* HDMI RX */
    SND_UCM_END_OF_LIST,
};

/* List of valid modifiers */
static const char *mod_list[] = {
    SND_USE_CASE_MOD_CAPTURE_VOICE,
    SND_USE_CASE_MOD_CAPTURE_MUSIC,
    SND_USE_CASE_MOD_PLAY_MUSIC,
    SND_USE_CASE_MOD_PLAY_VOICE,
    SND_USE_CASE_MOD_PLAY_FM,
    SND_USE_CASE_MOD_CAPTURE_FM,
    SND_USE_CASE_MOD_PLAY_LPA,
    SND_USE_CASE_MOD_PLAY_VOIP,
    SND_USE_CASE_MOD_CAPTURE_VOIP,
    SND_UCM_END_OF_LIST,
};

/* Structure to maintain the valid devices and
 * modifiers list per each use case */
struct use_case_verb {
    const char *use_case_name;
    const char **device_list;
    const char **modifier_list;
};

/* Valid device lists for specific use case */
static const char *HiFi_device_list[] = {
    SND_USE_CASE_DEV_SPEAKER,
    SND_USE_CASE_DEV_LINE,
    SND_USE_CASE_DEV_HEADPHONES,
    SND_USE_CASE_DEV_HEADSET,
    SND_USE_CASE_DEV_HANDSET,
    SND_USE_CASE_DEV_BLUETOOTH,
    SND_USE_CASE_DEV_EARPIECE,
    SND_USE_CASE_DEV_HDMI,
    SND_USE_CASE_DEV_BTSCO_RX,
    SND_USE_CASE_DEV_BTSCO_TX,
    SND_UCM_END_OF_LIST,
};

static const char *VoiceCall_device_list[] = {
    SND_USE_CASE_DEV_SPEAKER,
    SND_USE_CASE_DEV_LINE,
    SND_USE_CASE_DEV_HEADPHONES,
    SND_USE_CASE_DEV_HEADSET,
    SND_USE_CASE_DEV_HANDSET,
    SND_USE_CASE_DEV_BLUETOOTH,
    SND_USE_CASE_DEV_EARPIECE,
    SND_USE_CASE_DEV_HDMI,
    SND_USE_CASE_DEV_BTSCO_RX,
    SND_USE_CASE_DEV_BTSCO_TX,
    SND_UCM_END_OF_LIST,
};

static const char *FMREC_device_list[] = {
    SND_USE_CASE_DEV_NONE,
    SND_UCM_END_OF_LIST,
};

/* Valid modifiers lists for specific use case */
static const char *HiFi_modifier_list[] = {
    SND_USE_CASE_MOD_CAPTURE_VOICE,
    SND_USE_CASE_MOD_CAPTURE_MUSIC,
    SND_USE_CASE_MOD_PLAY_MUSIC,
    SND_USE_CASE_MOD_PLAY_VOICE,
    SND_USE_CASE_MOD_PLAY_FM,
    SND_USE_CASE_MOD_CAPTURE_FM,
    SND_USE_CASE_MOD_PLAY_LPA,
    SND_UCM_END_OF_LIST,
};

static const char *HiFiLP_modifier_list[] = {
    SND_USE_CASE_MOD_CAPTURE_VOICE,
    SND_USE_CASE_MOD_CAPTURE_MUSIC,
    SND_USE_CASE_MOD_PLAY_MUSIC,
    SND_USE_CASE_MOD_PLAY_VOICE,
    SND_USE_CASE_MOD_PLAY_FM,
    SND_USE_CASE_MOD_CAPTURE_FM,
    SND_UCM_END_OF_LIST,
};

static const char *VoiceCall_modifier_list[] = {
    SND_USE_CASE_MOD_CAPTURE_VOICE,
    SND_USE_CASE_MOD_CAPTURE_MUSIC,
    SND_USE_CASE_MOD_PLAY_MUSIC,
    SND_USE_CASE_MOD_PLAY_FM,
    SND_USE_CASE_MOD_CAPTURE_FM,
    SND_USE_CASE_MOD_PLAY_LPA,
    SND_UCM_END_OF_LIST,
};

static const char *Radio_modifier_list[] = {
    SND_USE_CASE_MOD_CAPTURE_VOICE,
    SND_USE_CASE_MOD_CAPTURE_MUSIC,
    SND_USE_CASE_MOD_PLAY_MUSIC,
    SND_USE_CASE_MOD_PLAY_VOICE,
    SND_USE_CASE_MOD_CAPTURE_FM,
    SND_USE_CASE_MOD_PLAY_LPA,
    SND_UCM_END_OF_LIST,
};

static const char *FMREC_modifier_list[] = {
    SND_USE_CASE_MOD_CAPTURE_VOICE,
    SND_USE_CASE_MOD_CAPTURE_MUSIC,
    SND_USE_CASE_MOD_PLAY_MUSIC,
    SND_USE_CASE_MOD_PLAY_VOICE,
    SND_USE_CASE_MOD_PLAY_FM,
    SND_USE_CASE_MOD_PLAY_LPA,
    SND_UCM_END_OF_LIST,
};

static const char *FMTx_modifier_list[] = {
    SND_USE_CASE_MOD_CAPTURE_VOICE,
    SND_USE_CASE_MOD_CAPTURE_MUSIC,
    SND_USE_CASE_MOD_PLAY_MUSIC,
    SND_USE_CASE_MOD_PLAY_VOICE,
    SND_USE_CASE_MOD_PLAY_FM,
    SND_USE_CASE_MOD_CAPTURE_FM,
    SND_USE_CASE_MOD_PLAY_LPA,
    SND_UCM_END_OF_LIST,
};

/* Valid devices and modifiers list for each use case */
static struct use_case_verb use_case_verb_list[] = {
    { SND_USE_CASE_VERB_INACTIVE, NULL, NULL,
    },
    { SND_USE_CASE_VERB_HIFI, HiFi_device_list, HiFi_modifier_list,
    },
    { SND_USE_CASE_VERB_HIFI_LOW_POWER, HiFi_device_list, HiFiLP_modifier_list,
    },
    { SND_USE_CASE_VERB_VOICECALL, VoiceCall_device_list, VoiceCall_modifier_list,
    },
    { SND_USE_CASE_VERB_IP_VOICECALL, NULL, NULL,
    },
    { SND_USE_CASE_VERB_DIGITAL_RADIO, HiFi_device_list, Radio_modifier_list,
    },
    { SND_USE_CASE_VERB_FM_REC, FMREC_device_list, FMREC_modifier_list,
    },
    { SND_USE_CASE_VERB_HIFI_REC, HiFi_device_list, HiFi_modifier_list,
    },
};

/* List utility functions for maintaining enabled devices and modifiers */
int snd_ucm_add_ident_to_list(struct snd_ucm_ident_node **head, const char *value);
char *snd_ucm_get_value_at_index(struct snd_ucm_ident_node *head, int index);
int snd_ucm_get_size_of_list(struct snd_ucm_ident_node *head);
int snd_ucm_del_ident_from_list(struct snd_ucm_ident_node **head, const char *value);
int snd_ucm_free_list(struct snd_ucm_ident_node **head);
void snd_ucm_print_list(struct snd_ucm_ident_node *head);

#ifdef __cplusplus
}
#endif

#endif
