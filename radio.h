/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#ifndef RADIO_H
#define RADIO_H

#include <stdbool.h>
#include <stdint.h>

#include "dcs.h"
#include "frequencies.h"

enum {
    RADIO_CHANNEL_UP   = 0x01u,
    RADIO_CHANNEL_DOWN = 0xFFu,
};

enum {
    BANDWIDTH_WIDE = 0,
    BANDWIDTH_NARROW
};

enum PTT_ID_t {
    PTT_ID_OFF = 0,    // OFF
    PTT_ID_TX_UP,      // BEGIN OF TX
    PTT_ID_TX_DOWN,    // END OF TX
    PTT_ID_BOTH,       // BOTH
    PTT_ID_APOLLO      // Apolo quindar tones
};
typedef enum PTT_ID_t PTT_ID_t;

enum VfoState_t
{
    VFO_STATE_NORMAL = 0,
    VFO_STATE_BUSY,
    VFO_STATE_BAT_LOW,
    VFO_STATE_TX_DISABLE,
    VFO_STATE_TIMEOUT,
    VFO_STATE_ALARM,
    VFO_STATE_VOLTAGE_HIGH,
    _VFO_STATE_LAST_ELEMENT
};
typedef enum VfoState_t VfoState_t;

typedef enum {
    MODULATION_FM,
    MODULATION_AM,
    MODULATION_USB,

#ifdef ENABLE_BYP_RAW_DEMODULATORS
    MODULATION_BYP,
	MODULATION_RAW,
#endif

    MODULATION_UKNOWN
} ModulationMode_t;

extern const char gModulationStr[MODULATION_UKNOWN][4];

typedef struct
{
    uint32_t       Frequency;
    DCS_CodeType_t CodeType;
    uint8_t        Code;
    uint8_t        Padding[2];
} FREQ_Config_t;

typedef struct VFO_Info_t
{
    FREQ_Config_t  freq_config_RX; // 接收频率配置
    FREQ_Config_t  freq_config_TX; // 发送频率配置

    // 以下指针用于频率反转功能
    // 正常情况下指向freq_config_RX，如果反转功能激活则指向freq_config_TX
    FREQ_Config_t *pRX;

    // 以下指针用于频率反转功能
    // 正常情况下指向freq_config_TX，如果反转功能激活则指向freq_config_RX
    FREQ_Config_t *pTX;

    uint32_t       TX_OFFSET_FREQUENCY; // 发送频率偏移量
    uint16_t       StepFrequency; // 频率步进值

    uint8_t        CHANNEL_SAVE; // 通道保存标志

    uint8_t        TX_OFFSET_FREQUENCY_DIRECTION; // 发送频率偏移方向

    uint8_t        SquelchOpenRSSIThresh; // 静噪开启RSSI阈值
    uint8_t        SquelchOpenNoiseThresh; // 静噪开启噪声阈值
    uint8_t        SquelchCloseGlitchThresh; // 静噪关闭干扰阈值
    uint8_t        SquelchCloseRSSIThresh; // 静噪关闭RSSI阈值
    uint8_t        SquelchCloseNoiseThresh; // 静噪关闭噪声阈值
    uint8_t        SquelchOpenGlitchThresh; // 静噪开启干扰阈值

    STEP_Setting_t STEP_SETTING; // 步进设置
    uint8_t        OUTPUT_POWER; // 输出功率
    uint8_t        TXP_CalculatedSetting; // 发送功率计算设置
    bool           FrequencyReverse; // 频率反转标志

    uint8_t        SCRAMBLING_TYPE; // 扰码类型
    uint8_t        CHANNEL_BANDWIDTH; // 通道带宽

    uint8_t        SCANLIST1_PARTICIPATION; // 扫描列表1参与标志
    uint8_t        SCANLIST2_PARTICIPATION; // 扫描列表2参与标志

    uint8_t        Band; // 频段
#ifdef ENABLE_DTMF_CALLING
    uint8_t        DTMF_DECODING_ENABLE; // DTMF解码启用标志
#endif
    PTT_ID_t       DTMF_PTT_ID_TX_MODE; // DTMF PTT ID 发送模式

    uint8_t        BUSY_CHANNEL_LOCK; // 忙通道锁定标志

    ModulationMode_t    Modulation; // 调制模式

    uint8_t        Compander; // 压缩扩展器设置

    char           Name[16]; // 名称
} VFO_Info_t;


// Settings of the main VFO that is selected by the user
// The pointer follows gEeprom.TX_VFO index
extern VFO_Info_t    *gTxVfo;

// Settings of the actual VFO that is now used for RX,
// It is being alternated by dual watch, and flipped by crossband
// The pointer follows gEeprom.RX_VFO
extern VFO_Info_t    *gRxVfo;

// Equal to gTxVfo unless dual watch changes it on incomming transmition (this can only happen when XB off and DW on)
extern VFO_Info_t    *gCurrentVfo;

extern DCS_CodeType_t gCurrentCodeType;

extern VfoState_t     VfoState[2];

bool     RADIO_CheckValidChannel(uint16_t channel, bool checkScanList, uint8_t scanList);
uint8_t  RADIO_FindNextChannel(uint8_t ChNum, int8_t Direction, bool bCheckScanList, uint8_t RadioNum);
void     RADIO_InitInfo(VFO_Info_t *pInfo, const uint8_t ChannelSave, const uint32_t Frequency);
void     RADIO_ConfigureChannel(const unsigned int VFO, const unsigned int configure);
void     RADIO_ConfigureSquelchAndOutputPower(VFO_Info_t *pInfo);
void     RADIO_ApplyOffset(VFO_Info_t *pInfo);
void     RADIO_SelectVfos(void);
void RADIO_SetupRegisters(bool switchToForeground);
#ifdef ENABLE_NOAA
void RADIO_ConfigureNOAA(void);
#endif
void     RADIO_SetTxParameters(void);
void     RADIO_SetModulation(ModulationMode_t modulation);
void     RADIO_SetVfoState(VfoState_t State);
void     RADIO_PrepareTX(void);
void     RADIO_SendCssTail(void);
void     RADIO_PrepareCssTX(void);
void     RADIO_SendEndOfTransmission(void);
void RADIO_SetupAGC(bool listeningAM, bool disable);

#endif