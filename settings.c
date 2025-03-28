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

#include <string.h>

#include "app/dtmf.h"
#ifdef ENABLE_FMRADIO
#include "app/fm.h"
#endif
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "misc.h"
#include "settings.h"
#include "ui/menu.h"

static const uint32_t gDefaultFrequencyTable[] =
        {
                14500000,
                14550000,
                43300000,
                43320000,
                43350000
        };

EEPROM_Config_t gEeprom={0};


 //读取eeprom信息,有默认配置
void SETTINGS_InitEEPROM(void)
{
    //修改 增加读取循环信息
     ReadcycleCount();


    uint8_t Data[16] = {0};
    // 0E70..0E77-------------------------------------------------------------------------
    EEPROM_ReadBuffer(0x0E70, Data, 8);
    //修改 删除一键功能
    // gEeprom.CHAN_1_CALL       = IS_MR_CHANNEL(Data[0]) ? Data[0] : MR_CHANNEL_FIRST;
    gEeprom.SQUELCH_LEVEL        = (Data[1] < 10) ? Data[1] : 1;
    gEeprom.TX_TIMEOUT_TIMER     = (Data[2] < 11) ? Data[2] : 1;
#ifdef ENABLE_NOAA
    gEeprom.NOAA_AUTO_SCAN       = (Data[3] <  2) ? Data[3] : false;
#endif
    gEeprom.KEY_LOCK             = (Data[4] <  2) ? Data[4] : false;

#ifdef ENABLE_VOX
    gEeprom.VOX_SWITCH           = (Data[5] <  2) ? Data[5] : false;
	gEeprom.VOX_LEVEL            = (Data[6] < 10) ? Data[6] : 2;
#endif
    gEeprom.MIC_SENSITIVITY      = (Data[7] <  5) ? Data[7] : 4;

    // 0E78..0E7F-------------------------------------------------------------------------
    EEPROM_ReadBuffer(0x0E78, Data, 8);

    gEeprom.BACKLIGHT_MAX 		  = (Data[0] & 0xF) <= 10 ? (Data[0] & 0xF) : 10;
    gEeprom.CHANNEL_DISPLAY_MODE  = (Data[1] < 4) ? Data[1] : MDF_FREQUENCY;    // 4 instead of 3 - extra display mode
    gEeprom.CROSS_BAND_RX_TX      = (Data[2] < 3) ? Data[2] : CROSS_BAND_OFF;
    gEeprom.BATTERY_SAVE          = (Data[3] < 5) ? Data[3] : 1;
    gEeprom.DUAL_WATCH            = (Data[4] < 3) ? Data[4] : DUAL_WATCH_CHAN_A;
    gEeprom.BACKLIGHT_TIME        = (Data[5] < ARRAY_SIZE(gSubMenu_BACKLIGHT)) ? Data[5] : 3;
    gEeprom.TAIL_TONE_ELIMINATION = (Data[6] < 2) ? Data[6] : true;
    gEeprom.VFO_OPEN              = (Data[7] < 2) ? Data[7] : true;

    // 0E80..0E87 当前频道存储-------------------------------------------------------------------------
    EEPROM_ReadBuffer(0x0E80, Data, 8);
    gEeprom.ScreenChannel[0]      = IS_VALID_CHANNEL(Data[0]) ? Data[0] : (FREQ_CHANNEL_FIRST + BAND6_400MHz);
    gEeprom.ScreenChannel[1]      = IS_VALID_CHANNEL(Data[3]) ? Data[3] : (FREQ_CHANNEL_FIRST + BAND6_400MHz);
    gEeprom.MrChannel[0]          = IS_MR_CHANNEL(Data[1])    ? Data[1] : MR_CHANNEL_FIRST;
    gEeprom.MrChannel[1]          = IS_MR_CHANNEL(Data[4])    ? Data[4] : MR_CHANNEL_FIRST;
    gEeprom.FreqChannel[0]        = IS_FREQ_CHANNEL(Data[2])  ? Data[2] : (FREQ_CHANNEL_FIRST + BAND6_400MHz);
    gEeprom.FreqChannel[1]        = IS_FREQ_CHANNEL(Data[5])  ? Data[5] : (FREQ_CHANNEL_FIRST + BAND6_400MHz);
#ifdef ENABLE_NOAA
    gEeprom.NoaaChannel[0] = IS_NOAA_CHANNEL(Data[6])  ? Data[6] : NOAA_CHANNEL_FIRST;
	gEeprom.NoaaChannel[1] = IS_NOAA_CHANNEL(Data[7])  ? Data[7] : NOAA_CHANNEL_FIRST;
#endif

// 0E88..0E8F-------------------------------------------------------------------------
#ifdef ENABLE_FMRADIO
    {
		struct
		{
			uint16_t SelectedFrequency;
			uint8_t  SelectedChannel;
			uint8_t  IsMrMode;
			uint8_t  Padding[8];
		} __attribute__((packed)) FM;

		EEPROM_ReadBuffer(0x0E88, &FM, 8);

		gEeprom.FM_LowerLimit = FM_LowerLimit_define;
		gEeprom.FM_UpperLimit = FM_UpperLimit_define;

		if (FM.SelectedFrequency < gEeprom.FM_LowerLimit || FM.SelectedFrequency > gEeprom.FM_UpperLimit)
			gEeprom.FM_SelectedFrequency = 960;
		else
			gEeprom.FM_SelectedFrequency = FM.SelectedFrequency;

		gEeprom.FM_SelectedChannel       = FM.SelectedChannel;
		gEeprom.FM_IsMrMode              = (FM.IsMrMode < 2) ? FM.IsMrMode : false;
	}

	// 0E40..0E67-------------------------------------------------------------------------
	EEPROM_ReadBuffer(0x0E40, gFM_Channels, sizeof(gFM_Channels));
	FM_ConfigureChannelState();
#endif

    // 0E90..0E97-------------------------------------------------------------------------
    EEPROM_ReadBuffer(0x0E90, Data, 8);


     gEeprom.BEEP_CONTROL                 = (Data[0] < 2)              ? Data[0] :0;
     gEeprom.MDC1200_ID                   =((uint16_t) (Data[2] << 8))|((uint16_t)(Data[1] ));
//   gEeprom.KEY_1_LONG_PRESS_ACTION      = (Data[2] < ACTION_OPT_LEN) ? Data[2] : ACTION_OPT_FLASHLIGHT;
//   gEeprom.KEY_2_SHORT_PRESS_ACTION     = (Data[3] < ACTION_OPT_LEN) ? Data[3] : ACTION_OPT_SCAN;
//   gEeprom.KEY_2_LONG_PRESS_ACTION      = (Data[4] < ACTION_OPT_LEN) ? Data[4] : ACTION_OPT_NONE;
     gEeprom.SCAN_RESUME_MODE             = (Data[5] < 3)              ? Data[5] : SCAN_RESUME_CO;
//   gEeprom.AUTO_KEYPAD_LOCK             = (Data[6] < 2)              ? Data[6] : false;
#if ENABLE_CHINESE_FULL==4
    gEeprom.POWER_ON_DISPLAY_MODE         = (Data[7] < 4)              ? Data[7] : POWER_ON_DISPLAY_MODE_NONE;
#endif

#ifdef ENABLE_CUSTOM_SIDEFUNCTIONS
    // 1FF8..1FFF-------------------------------------------------------------------------
    EEPROM_ReadBuffer(0x1FF8, Data, 8);
    gEeprom.KEY_M_LONG_PRESS_ACTION       = (Data[0] < ACTION_OPT_LEN) ? Data[0] : ACTION_OPT_SWITCH_DEMODUL;
    gEeprom.KEY_1_SHORT_PRESS_ACTION      = (Data[1] < ACTION_OPT_LEN-2) ? Data[1] : ACTION_OPT_MONITOR;
    gEeprom.KEY_1_LONG_PRESS_ACTION       = (Data[2] < ACTION_OPT_LEN) ? Data[2] : ACTION_OPT_D_DCD;
    gEeprom.KEY_2_SHORT_PRESS_ACTION      = (Data[3] < ACTION_OPT_LEN-2) ? Data[3] : ACTION_OPT_WIDTH;
    gEeprom.KEY_2_LONG_PRESS_ACTION       = (Data[4] < ACTION_OPT_LEN) ? Data[4] : ACTION_OPT_FLASHLIGHT;
#endif

    // 0E98..0E9F 取消密码-------------------------------------------------------------------------
//    EEPROM_ReadBuffer(0x0E98, Data, 8);
//    memcpy(&gEeprom.POWER_ON_PASSWORD, Data, 4);

    // 0EA0..0EA7-------------------------------------------------------------------------
    EEPROM_ReadBuffer(0x0EA0, Data, 8);
#ifdef ENABLE_VOICE
    gEeprom.VOICE_PROMPT = (Data[0] < 3) ? Data[0] : VOICE_PROMPT_ENGLISH;
#endif
#ifdef ENABLE_RSSI_BAR
    if((Data[1] < 200 && Data[1] > 90) && (Data[2] < Data[1]-9 && Data[1] < 160 && Data[2] > 50)) {
			gEeprom.S0_LEVEL = Data[1];
			gEeprom.S9_LEVEL = Data[2];
		}
		else {
			gEeprom.S0_LEVEL = 130;
			gEeprom.S9_LEVEL = 76;
		}
#endif


    // 0EA8..0EAF-------------------------------------------------------------------------
    EEPROM_ReadBuffer(0x0EA8, Data, 8);
#ifdef ENABLE_ALARM
    gEeprom.ALARM_MODE                 = (Data[0] <  2) ? Data[0] : true;
#endif
    //修改
    gEeprom.ROGER                          = (Data[1] <  9) ? Data[1] : ROGER_MODE_OFF;
    gEeprom.REPEATER_TAIL_TONE_ELIMINATION = (Data[2] < 11) ? Data[2] : 0;
    gEeprom.TX_VFO                         = (Data[3] <  2) ? Data[3] : 0;
    gEeprom.BATTERY_TYPE                   = (Data[4] < BATTERY_TYPE_UNKNOWN) ? Data[4] : BATTERY_TYPE_1600_MAH;

    // 0ED0..0ED7-------------------------------------------------------------------------
    EEPROM_ReadBuffer(0x0ED0, Data, 8);
    gEeprom.DTMF_SIDE_TONE               = (Data[0] <   2) ? Data[0] : true;

#ifdef ENABLE_DTMF_CALLING
    gEeprom.DTMF_SEPARATE_CODE           = DTMF_ValidateCodes((char *)(Data + 1), 1) ? Data[1] : '*';
	gEeprom.DTMF_GROUP_CALL_CODE         = DTMF_ValidateCodes((char *)(Data + 2), 1) ? Data[2] : '#';
	gEeprom.DTMF_DECODE_RESPONSE         = (Data[3] <   4) ? Data[3] : 0;
gEeprom.DTMF_auto_reset_time = (Data[4] < 61 && Data[4] >= 5) ? Data[4] : 10;
#endif
    gEeprom.DTMF_PRELOAD_TIME            = (Data[5] < 101) ? Data[5] * 10 : 300;
    gEeprom.DTMF_FIRST_CODE_PERSIST_TIME = (Data[6] < 101) ? Data[6] * 10 : 100;
    gEeprom.DTMF_HASH_CODE_PERSIST_TIME  = (Data[7] < 101) ? Data[7] * 10 : 100;

    // 0ED8..0EDF-------------------------------------------------------------------------
    EEPROM_ReadBuffer(0x0ED8, Data, 8);
    gEeprom.DTMF_CODE_PERSIST_TIME  = (Data[0] < 101) ? Data[0] * 10 : 100;
    gEeprom.DTMF_CODE_INTERVAL_TIME = (Data[1] < 101) ? Data[1] * 10 : 100;
#ifdef ENABLE_DTMF_CALLING
    gEeprom.PERMIT_REMOTE_KILL      = (Data[2] <   2) ? Data[2] : true;

	// 0EE0..0EE7-------------------------------------------------------------------------

EEPROM_ReadBuffer(0x0EE0, Data, sizeof(gEeprom.ANI_DTMF_ID));
	if (DTMF_ValidateCodes((char *)Data, sizeof(gEeprom.ANI_DTMF_ID))) {
		memcpy(gEeprom.ANI_DTMF_ID, Data, sizeof(gEeprom.ANI_DTMF_ID));
	} else {
		strcpy(gEeprom.ANI_DTMF_ID, "123");
	}


	// 0EE8..0EEF-------------------------------------------------------------------------
EEPROM_ReadBuffer(0x0EE8, Data, sizeof(gEeprom.KILL_CODE));
	if (DTMF_ValidateCodes((char *)Data, sizeof(gEeprom.KILL_CODE))) {
		memcpy(gEeprom.KILL_CODE, Data, sizeof(gEeprom.KILL_CODE));
	} else {
		strcpy(gEeprom.KILL_CODE, "ABCD9");
	}

	// 0EF0..0EF7-------------------------------------------------------------------------
EEPROM_ReadBuffer(0x0EF0, Data, sizeof(gEeprom.REVIVE_CODE));
	if (DTMF_ValidateCodes((char *)Data, sizeof(gEeprom.REVIVE_CODE))) {
		memcpy(gEeprom.REVIVE_CODE, Data, sizeof(gEeprom.REVIVE_CODE));
	} else {
		strcpy(gEeprom.REVIVE_CODE, "9DCBA");
	}
#endif

    // 0EF8..0F07-------------------------------------------------------------------------
    EEPROM_ReadBuffer(0x0EF8, Data, sizeof(gEeprom.DTMF_UP_CODE));
    if (DTMF_ValidateCodes((char *)Data, sizeof(gEeprom.DTMF_UP_CODE))) {
        memcpy(gEeprom.DTMF_UP_CODE, Data, sizeof(gEeprom.DTMF_UP_CODE));
    } else {
        strcpy(gEeprom.DTMF_UP_CODE, "12345");
    }

    // 0F08..0F17-------------------------------------------------------------------------
    EEPROM_ReadBuffer(0x0F08, Data, sizeof(gEeprom.DTMF_DOWN_CODE));
    if (DTMF_ValidateCodes((char *)Data, sizeof(gEeprom.DTMF_DOWN_CODE))) {
        memcpy(gEeprom.DTMF_DOWN_CODE, Data, sizeof(gEeprom.DTMF_DOWN_CODE));
    } else {
        strcpy(gEeprom.DTMF_DOWN_CODE, "54321");
    }

    // 0F18..0F1F-------------------------------------------------------------------------
    EEPROM_ReadBuffer(0x0F18, Data, 8);
    gEeprom.SCAN_LIST_DEFAULT = (Data[0] < 3) ? Data[0] : 0;  // we now have 'all' channel scan option
    for (unsigned int i = 0; i < 2; i++)
    {
        const unsigned int j = 1 + (i * 3);
        gEeprom.SCAN_LIST_ENABLED[i]     = (Data[j + 0] < 2) ? Data[j] : false;
        gEeprom.SCANLIST_PRIORITY_CH1[i] =  Data[j + 1];
        gEeprom.SCANLIST_PRIORITY_CH2[i] =  Data[j + 2];
    }

    // 0F40..0F47-------------------------------------------------------------------------
    EEPROM_ReadBuffer(0x0F40, Data, 8);
    gSetting_F_LOCK            = (Data[0] < F_LOCK_LEN) ? Data[0] : F_LOCK_DEF;
//  gSetting_350TX             = (Data[1] < 2) ? Data[1] : false;  // was true
#ifdef ENABLE_DTMF_CALLING
    gSetting_KILLED            = (Data[2] < 2) ? Data[2] : false;
#endif
//  gSetting_200TX             = (Data[3] < 2) ? Data[3] : false;
//  gSetting_500TX             = (Data[4] < 2) ? Data[4] : false;
//  gSetting_350EN             = (Data[5] < 2) ? Data[5] : true;
    gSetting_ScrambleEnable    = (Data[6] < 2) ? Data[6] : true;
//  gSetting_TX_EN             = (Data[7] & (1u << 0)) ? true : false;
    gSetting_live_DTMF_decoder = !!(Data[7] & (1u << 1));
//  gSetting_battery_text      = (((Data[7] >> 2) & 3u) <= 2) ? (Data[7] >> 2) & 3 : 2;
//#ifdef ENABLE_AUDIO_BAR
    //gSetting_mic_bar       = (Data[7] & (1u << 4)) ? true : false;
//#endif
#ifdef ENABLE_AM_FIX
    gSetting_AM_fix = !!(Data[7] & (1u << 5));
#endif
  //  gSetting_backlight_on_tx_rx = (Data[7] >> 6) & 3u;

    if (!gEeprom.VFO_OPEN)
    {
        gEeprom.ScreenChannel[0] = gEeprom.MrChannel[0];
        gEeprom.ScreenChannel[1] = gEeprom.MrChannel[1];
    }

    // 0D60..0E27-------------------------------------------------------------------------
    EEPROM_ReadBuffer(0x0D60, gMR_ChannelAttributes, sizeof(gMR_ChannelAttributes));
    for(uint16_t i = 0; i < sizeof(gMR_ChannelAttributes); i++) {
        ChannelAttributes_t *att = &gMR_ChannelAttributes[i];
        if(att->__val == 0xff){
            att->__val = 0;
            att->band = 0xf;
        }
    }

    // 0F30..0F3F-------------------------------------------------------------------------
    char B[8];
    memset(B,0XFF,8);
    EEPROM_WriteBuffer(0x0F30, B,8);
    EEPROM_WriteBuffer(0x0F38, B,8);

    EEPROM_ReadBuffer(0x0F30, gCustomAesKey, sizeof(gCustomAesKey));
    bHasCustomAesKey = false;
    //锁定
    for (unsigned int i = 0; i < ARRAY_SIZE(gCustomAesKey); i++)
    {
        if (gCustomAesKey[i] != 0xFFFFFFFFu)
        {
            bHasCustomAesKey = true;
            return;
        }
    }
}

//RSSI 校准：读取并应用从 EEPROM 中存储的 RSSI（接收信号强度指示）校准数据。RSSI 校准有助于提高接收信号的准确性和可靠性。
//电池校准：读取并应用从 EEPROM 中存储的电池校准数据。这些数据有助于更准确地计算电池的电压和剩余电量。
//VOX 阈值设置：读取并应用从 EEPROM 中存储的 VOX（声音激活传输）阈值设置。这些阈值决定了何时激活对讲机的传输功能，通常是根据环境噪声水平来调整的。
//BK4819 和其他杂项设置：读取并应用从 EEPROM 中存储的与 BK4819 模块和其他杂项相关的设置，如晶体频率、音量增益和 DAC 增益。
// 加载校准数据的函数
void SETTINGS_LoadCalibration(void) {
    // 读取 RSSI 校准数据
    EEPROM_ReadBuffer(0x1EC0, gEEPROM_RSSI_CALIB[3], 8);
    // 复制到其他 RSSI 校准数组
    memcpy(gEEPROM_RSSI_CALIB[4], gEEPROM_RSSI_CALIB[3], 8);
    memcpy(gEEPROM_RSSI_CALIB[5], gEEPROM_RSSI_CALIB[3], 8);
    memcpy(gEEPROM_RSSI_CALIB[6], gEEPROM_RSSI_CALIB[3], 8);

    // 读取其他 RSSI 校准数据
    EEPROM_ReadBuffer(0x1EC8, gEEPROM_RSSI_CALIB[0], 8);
    // 复制到其他 RSSI 校准数组
    memcpy(gEEPROM_RSSI_CALIB[1], gEEPROM_RSSI_CALIB[0], 8);
    memcpy(gEEPROM_RSSI_CALIB[2], gEEPROM_RSSI_CALIB[0], 8);

    // 读取电池校准数据
    EEPROM_ReadBuffer(0x1F40, gBatteryCalibration, 12);
    // 如果电池校准数据异常，设置为默认值
    if (gBatteryCalibration[0] >= 5000) {
        gBatteryCalibration[0] = 1900;
        gBatteryCalibration[1] = 2000;
    }
    gBatteryCalibration[5] = 2300;

#ifdef ENABLE_VOX
    // 读取 VOX 阈值设置
    EEPROM_ReadBuffer(0x1F50 + (gEeprom.VOX_LEVEL * 2), &gEeprom.VOX1_THRESHOLD, 2);
    EEPROM_ReadBuffer(0x1F68 + (gEeprom.VOX_LEVEL * 2), &gEeprom.VOX0_THRESHOLD, 2);
#endif

////     读取麦克风灵敏度设置（未使用）
//    EEPROM_ReadBuffer(0x1F80 + gEeprom.MIC_SENSITIVITY, &Mic, 1);
//    gEeprom.MIC_SENSITIVITY_TUNING = (Mic < 32) ? Mic : 15;

    // 读取其他杂项设置
    {
        struct
        {
            int16_t  BK4819_XtalFreqLow;
            uint16_t EEPROM_1F8A;
            uint16_t EEPROM_1F8C;
            uint8_t  VOLUME_GAIN;
            uint8_t  DAC_GAIN;
        } __attribute__((packed)) Misc;

        // 读取 BK4819 和其他杂项设置
        EEPROM_ReadBuffer(0x1F88, &Misc, 8);

        // 解析读取的 BK4819 和其他杂项设置
        gEeprom.BK4819_XTAL_FREQ_LOW = (Misc.BK4819_XtalFreqLow >= -1000 && Misc.BK4819_XtalFreqLow <= 1000) ? Misc.BK4819_XtalFreqLow : 0;
        gEEPROM_1F8A                 = Misc.EEPROM_1F8A & 0x01FF;
        gEEPROM_1F8C                 = Misc.EEPROM_1F8C & 0x01FF;
        gEeprom.VOLUME_GAIN          = (Misc.VOLUME_GAIN < 64) ? Misc.VOLUME_GAIN : 58;
        gEeprom.DAC_GAIN             = (Misc.DAC_GAIN    < 16) ? Misc.DAC_GAIN    : 8;
        // 写入 BK4819 寄存器以应用新的设置
        BK4819_WriteRegister(BK4819_REG_3B, 22656 + gEeprom.BK4819_XTAL_FREQ_LOW);
        // 写入 BK4819 寄存器以应用新的设置（高字节未使用）
//		BK4819_WriteRegister(BK4819_REG_3C, gEeprom.BK4819_XTAL_FREQ_HIGH);
    }
}

uint32_t SETTINGS_FetchChannelFrequency(const int channel)
{
    struct
    {
        uint32_t frequency;
        uint32_t offset;
    } __attribute__((packed)) info;

    EEPROM_ReadBuffer(channel * 16, &info, sizeof(info));

    return info.frequency;
}

void SETTINGS_FetchChannelName(char *s, const int channel)
{
    if (s == NULL)
        return;
    s[0] = 0;
//#if ENABLE_CHINESE_FULL==4
//    memset(s, 0, 16);  // 's' had better be large enough !
//#else
//    memset(s, 0, 10);  // 's' had better be large enough !
//#endif
    if (channel < 0)
        return;

    if (!RADIO_CheckValidChannel(channel, false, 0))
        return;
//    EEPROM_ReadBuffer(0x0F50 + (channel * 16), s + 0, 8);

#if ENABLE_CHINESE_FULL==4 && !defined(ENABLE_ENGLISH)

    EEPROM_ReadBuffer(0x0F50 + (channel * 16), s, 16);
    int i;
    for (i = 0; i < 16; i++)
        if (!((s[i] >= 32 && s[i] <= 127)||(
        s[i]>=0xb0&&s[i]<=0xf7&&i!=15&&s[i+1]!=0)
        ))break;                // invalid char
            else if(s[i]>=0xb0&&s[i]<=0xf7&&i!=15&&s[i+1]!=0) i++;

#else
    EEPROM_ReadBuffer(0x0F50 + (channel * 16), s, 10);
//    EEPROM_ReadBuffer(0x0F58 + (channel * 16), s + 8, 2);
    int i;
    for (i = 0; i < 10; i++)
        if (s[i] < 32 || s[i] > 127)
            break;                // invalid char
#endif


    s[i--] = 0;                   // null term

    while (i >= 0 && s[i] == 32)  // trim trailing spaces
        s[i--] = 0;               // null term
        //中文信道名
//    strcpy(s,"\x9b\x2c\x9b\x2c\x9b\x2c\x9b\x2c\x9b\x2c\x9b\x2c\x9b\x2c");
}

//重置设置内容
void SETTINGS_FactoryReset(bool bIsAll)
{
    uint16_t i;
    uint8_t  Template[8];

    memset(Template, 0xFF, sizeof(Template));

    for (i = 0x0C80; i < 0x1E00; i += 8)
    {
        if (
                !(i >= 0x0EE0 && i < 0x0F18) &&             // ANI ID + DTMF codes
                !(i >= 0x0F30 && i < 0x0F50) &&             // AES KEY + F LOCK + Scramble Enable
                !(i >= 0x1C00 && i < 0x1E00) &&             // DTMF contacts
                !(i >= 0x0EB0 && i < 0x0ED0) &&             // Welcome strings
                !(i >= 0x0EA0 && i < 0x0EA8) &&             // Voice Prompt
                (bIsAll ||
                 (
                         !(i >= 0x0D60 && i < 0x0E28) &&    // MR Channel Attributes
                         !(i >= 0x0F18 && i < 0x0F30) &&    // Scan List
                         !(i >= 0x0F50 && i < 0x1C00) &&    // MR Channel Names 3248个
                         !(i >= 0x0E40 && i < 0x0E70) &&    // FM Channels
                         !(i >= 0x0E88 && i < 0x0E90)       // FM settings
                 ))
                )
        {
            EEPROM_WriteBuffer(i, Template,8);
        }
    }

    if (bIsAll)
    {
        RADIO_InitInfo(gRxVfo, FREQ_CHANNEL_FIRST + BAND6_400MHz, 43350000);

        // set the first few memory channels
        for (i = 0; i < ARRAY_SIZE(gDefaultFrequencyTable); i++)
        {
            const uint32_t Frequency   = gDefaultFrequencyTable[i];
            gRxVfo->freq_config_RX.Frequency = Frequency;
            gRxVfo->freq_config_TX.Frequency = Frequency;
            gRxVfo->Band               = FREQUENCY_GetBand(Frequency);
            SETTINGS_SaveChannel(MR_CHANNEL_FIRST + i, 0, gRxVfo, 2);
        }
    }
}

#ifdef ENABLE_FMRADIO
void SETTINGS_SaveFM(void)
	{
		unsigned int i;

		struct
		{
			uint16_t Frequency;
			uint8_t  Channel;
			bool     IsChannelSelected;
			uint8_t  Padding[4];
		} State;

		memset(&State, 0xFF, sizeof(State));
		State.Channel           = gEeprom.FM_SelectedChannel;
		State.Frequency         = gEeprom.FM_SelectedFrequency;
		State.IsChannelSelected = gEeprom.FM_IsMrMode;

		EEPROM_WriteBuffer(0x0E88, &State,8);
		for (i = 0; i < 5; i++)
			EEPROM_WriteBuffer(0x0E40 + (i * 8), &gFM_Channels[i * 4],8);
	}
#endif

void SETTINGS_SaveVfoIndices(void)
{
    uint8_t State[8];

#ifndef ENABLE_NOAA
    EEPROM_ReadBuffer(0x0E80, State, sizeof(State));
#endif

    State[0] = gEeprom.ScreenChannel[0];
    State[1] = gEeprom.MrChannel[0];
    State[2] = gEeprom.FreqChannel[0];
    State[3] = gEeprom.ScreenChannel[1];
    State[4] = gEeprom.MrChannel[1];
    State[5] = gEeprom.FreqChannel[1];
#ifdef ENABLE_NOAA
    State[6] = gEeprom.NoaaChannel[0];
		State[7] = gEeprom.NoaaChannel[1];
#endif

    EEPROM_WriteBuffer(0x0E80, State,8);
}


//存储eeprom
void SETTINGS_SaveSettings(void)
{
    uint8_t  State[8];
//  uint32_t Password[2];取消

//  修改
//  State[0] = gEeprom.CHAN_1_CALL;
    State[0] = MR_CHANNEL_FIRST;
    State[1] = gEeprom.SQUELCH_LEVEL;
    State[2] = gEeprom.TX_TIMEOUT_TIMER;
#ifdef ENABLE_NOAA
    State[3] = gEeprom.NOAA_AUTO_SCAN;
#else
    State[3] = false;
#endif
    State[4] = gEeprom.KEY_LOCK;
#ifdef ENABLE_VOX
    State[5] = gEeprom.VOX_SWITCH;
	State[6] = gEeprom.VOX_LEVEL;
#else
    State[5] = false;
    State[6] = 0;
#endif
    State[7] = gEeprom.MIC_SENSITIVITY;
    EEPROM_WriteBuffer(0x0E70, State,8);
    //---------------------------------------------------------------------------------

    //修改
    // State[0] = ( key_dir ==-1?0xB0:0xA0 ) + gEeprom.BACKLIGHT_MAX;
    State[0] = (0xA0) + gEeprom.BACKLIGHT_MAX; //A0保证兼容性
    State[1] = gEeprom.CHANNEL_DISPLAY_MODE;
    State[2] = gEeprom.CROSS_BAND_RX_TX;
    State[3] = gEeprom.BATTERY_SAVE;
    State[4] = gEeprom.DUAL_WATCH;
    State[5] = gEeprom.BACKLIGHT_TIME;
    State[6] = gEeprom.TAIL_TONE_ELIMINATION;
    State[7] = gEeprom.VFO_OPEN;
    EEPROM_WriteBuffer(0x0E78, State,8);
    //---------------------------------------------------------------------------------

    State[0] = gEeprom.BEEP_CONTROL;
//  State[0] |= 0;//gEeprom.KEY_M_LONG_PRESS_ACTION << 1;
//  State[1]=(uint8_t)(gEeprom.MDC1200_ID&(0x000000ff));
//  State[2]=(uint8_t)((gEeprom.MDC1200_ID&0x0000ff00)>>8);
//  State[3]=(uint8_t)((gEeprom.MDC1200_ID&0x00ff0000)>>16);
//  State[4]=(uint8_t)((gEeprom.MDC1200_ID&0xff000000)>>24);
    State[1]=(uint8_t)(gEeprom.MDC1200_ID&(0x00ff));      //= 0; gEeprom.KEY_1_SHORT_PRESS_ACTION;
    State[2]=(uint8_t)((gEeprom.MDC1200_ID&(0xff00))>>8); //= 0;gEeprom.KEY_1_LONG_PRESS_ACTION;
    State[3] = 0;//gEeprom.KEY_2_SHORT_PRESS_ACTION;
    State[4] = 0;//gEeprom.KEY_2_LONG_PRESS_ACTION;
    State[5] = gEeprom.SCAN_RESUME_MODE;
    State[6] = 0;//gEeprom.AUTO_KEYPAD_LOCK;
#if ENABLE_CHINESE_FULL==4
    State[7] = gEeprom.POWER_ON_DISPLAY_MODE;
#endif

    EEPROM_WriteBuffer(0x0E90, State,8);
    //---------------------------------------------------------------------------------

#ifdef ENABLE_CUSTOM_SIDEFUNCTIONS
    State[0] = gEeprom.KEY_M_LONG_PRESS_ACTION;
    State[1] = gEeprom.KEY_1_SHORT_PRESS_ACTION;
    State[2] = gEeprom.KEY_1_LONG_PRESS_ACTION;
    State[3] = gEeprom.KEY_2_SHORT_PRESS_ACTION;
    State[4] = gEeprom.KEY_2_LONG_PRESS_ACTION;
    State[5] = 0;
    State[6] = 0;
    State[7] = 0;
    EEPROM_WriteBuffer(0x1FF8, State, 5);
#endif
    //保存按键定义---------------------------------------------------------------------------------

//    memset(Password, 0xFF, sizeof(Password));
//    Password[0] = gEeprom.POWER_ON_PASSWORD;
//    EEPROM_WriteBuffer(0x0E98, Password,8);

    //保存密码---------------------------------------------------------------------------------

    memset(State, 0xFF, sizeof(State));
#ifdef ENABLE_VOICE
    State[0] = gEeprom.VOICE_PROMPT;
#endif

#ifdef ENABLE_RSSI_BAR
    State[1] = gEeprom.S0_LEVEL;
	State[2] = gEeprom.S9_LEVEL;
#endif
    EEPROM_WriteBuffer(0x0EA0, State,8);
    //信号条---------------------------------------------------------------------------------

#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
    State[0] = gEeprom.ALARM_MODE;
#else
    State[0] = false;
#endif
    State[1] = gEeprom.ROGER;
    State[2] = gEeprom.REPEATER_TAIL_TONE_ELIMINATION;
    State[3] = gEeprom.TX_VFO;
    State[4] = gEeprom.BATTERY_TYPE;//电池类型增加
    EEPROM_WriteBuffer(0x0EA8, State,8);
    //---------------------------------------------------------------------------------

    State[0] = gEeprom.DTMF_SIDE_TONE;
#ifdef ENABLE_DTMF_CALLING
    State[1] = gEeprom.DTMF_SEPARATE_CODE;
	State[2] = gEeprom.DTMF_GROUP_CALL_CODE;
	State[3] = gEeprom.DTMF_DECODE_RESPONSE;
	State[4] = gEeprom.DTMF_auto_reset_time;
#endif
    State[5] = gEeprom.DTMF_PRELOAD_TIME / 10U;
    State[6] = gEeprom.DTMF_FIRST_CODE_PERSIST_TIME / 10U;
    State[7] = gEeprom.DTMF_HASH_CODE_PERSIST_TIME / 10U;
    EEPROM_WriteBuffer(0x0ED0, State,8);
    //---------------------------------------------------------------------------------

    memset(State, 0xFF, sizeof(State));
    State[0] = gEeprom.DTMF_CODE_PERSIST_TIME / 10U;
    State[1] = gEeprom.DTMF_CODE_INTERVAL_TIME / 10U;
#ifdef ENABLE_DTMF_CALLING
    State[2] = gEeprom.PERMIT_REMOTE_KILL;
#endif
    EEPROM_WriteBuffer(0x0ED8, State,8);
    //---------------------------------------------------------------------------------

    State[0] = gEeprom.SCAN_LIST_DEFAULT;
    State[1] = gEeprom.SCAN_LIST_ENABLED[0];
    State[2] = gEeprom.SCANLIST_PRIORITY_CH1[0];
    State[3] = gEeprom.SCANLIST_PRIORITY_CH2[0];
    State[4] = gEeprom.SCAN_LIST_ENABLED[1];
    State[5] = gEeprom.SCANLIST_PRIORITY_CH1[1];
    State[6] = gEeprom.SCANLIST_PRIORITY_CH2[1];
    State[7] = 0xFF;
    EEPROM_WriteBuffer(0x0F18, State,8);
    //---------------------------------------------------------------------------------

    memset(State, 0xFF, sizeof(State));
    State[0]  = gSetting_F_LOCK;
//  State[1]  = gSetting_350TX;
#ifdef ENABLE_DTMF_CALLING
    State[2]  = gSetting_KILLED;
#endif
//  State[3]  = gSetting_200TX;
//  State[4]  = gSetting_500TX;
//  State[5]  = gSetting_350EN;
    State[6]  = gSetting_ScrambleEnable;
//  if (!gSetting_TX_EN)             State[7] &= ~(1u << 0);
    if (!gSetting_live_DTMF_decoder) State[7] &= ~(1u << 1);
    State[7] = (State[7] & ~(3u << 2)) | ((0 & 3u) << 2);

#ifdef ENABLE_AM_FIX
    if (!gSetting_AM_fix)            State[7] &= ~(1u << 5);
#endif
    State[7] = (State[7] & ~(3u << 6)) | ((2 & 3u) << 6);
    EEPROM_WriteBuffer(0x0F40, State,8);

    //---------------------------------------------------------------------------------
}

void SETTINGS_SaveChannel(uint8_t Channel, uint8_t VFO, const VFO_Info_t *pVFO, uint8_t Mode)
{
#ifdef ENABLE_NOAA
    if (IS_NOAA_CHANNEL(Channel))
		return;
#endif

    uint16_t OffsetVFO = Channel * 16;

    if (IS_FREQ_CHANNEL(Channel)) { // it's a VFO, not a channel
        OffsetVFO  = (VFO == 0) ? 0x0C80 : 0x0C90;
        OffsetVFO += (Channel - FREQ_CHANNEL_FIRST) * 32;
    }

    if (Mode >= 2 || IS_FREQ_CHANNEL(Channel)) { // copy VFO to a channel
        union {
            uint8_t _8[8];
            uint32_t _32[2];
        } State;

        State._32[0] = pVFO->freq_config_RX.Frequency;
        State._32[1] = pVFO->TX_OFFSET_FREQUENCY;
        EEPROM_WriteBuffer(OffsetVFO + 0, State._32,8);

        State._8[0] =  pVFO->freq_config_RX.Code;
        State._8[1] =  pVFO->freq_config_TX.Code;
        State._8[2] = (pVFO->freq_config_TX.CodeType << 4) | pVFO->freq_config_RX.CodeType;
        State._8[3] = (pVFO->Modulation << 4) | pVFO->TX_OFFSET_FREQUENCY_DIRECTION;
        State._8[4] = 0
                      | (pVFO->BUSY_CHANNEL_LOCK << 4)
                      | (pVFO->OUTPUT_POWER      << 2)
                      | (pVFO->CHANNEL_BANDWIDTH << 1)
                      | (pVFO->FrequencyReverse  << 0);
        State._8[5] = ((pVFO->DTMF_PTT_ID_TX_MODE & 7u) << 1)
#ifdef ENABLE_DTMF_CALLING
            | ((pVFO->DTMF_DECODING_ENABLE & 1u) << 0)
#endif
                ;
        State._8[6] =  pVFO->STEP_SETTING;
        State._8[7] =  pVFO->SCRAMBLING_TYPE;
        EEPROM_WriteBuffer(OffsetVFO + 8, State._8,8);

        SETTINGS_UpdateChannel(Channel, pVFO, true);

        if (IS_MR_CHANNEL(Channel)) {
#ifndef ENABLE_KEEP_MEM_NAME
            // clear/reset the channel name
            SETTINGS_SaveChannelName(Channel, "");
#else
            if (Mode >= 3) {
				SETTINGS_SaveChannelName(Channel, pVFO->Name);
			}
#endif
        }
    }

}

void SETTINGS_SaveBatteryCalibration(const uint16_t * batteryCalibration)
{
    uint16_t buf[4];
    EEPROM_WriteBuffer(0x1F40, batteryCalibration,8);
    EEPROM_ReadBuffer( 0x1F48, buf, sizeof(buf));
    buf[0] = batteryCalibration[4];
    buf[1] = batteryCalibration[5];
    EEPROM_WriteBuffer(0x1F48, buf,8);
}

void SETTINGS_SaveChannelName(uint8_t channel, const char * name)
{
    uint16_t offset = channel * 16;
    uint8_t buf[16] = {0};
    memcpy(buf, name, MIN(( int)strlen(name), MAX_EDIT_INDEX));
    EEPROM_WriteBuffer(0x0F50 + offset, buf,8);
    EEPROM_WriteBuffer(0x0F58 + offset, buf + 8,8);
}

void SETTINGS_UpdateChannel(uint8_t channel, const VFO_Info_t *pVFO, bool keep)
{
#ifdef ENABLE_NOAA
    if (!IS_NOAA_CHANNEL(channel))
#endif
    {
        uint8_t  state[8];
        ChannelAttributes_t  att = {
                .band = 0xf,
                .compander = 0,
                .scanlist1 = 0,
                .scanlist2 = 0,
        };        // default attributes

        uint16_t offset = 0x0D60 + (channel & ~7u);
        EEPROM_ReadBuffer(offset, state, sizeof(state));

        if (keep) {
            att.band = pVFO->Band;
            att.scanlist1 = pVFO->SCANLIST1_PARTICIPATION;
            att.scanlist2 = pVFO->SCANLIST2_PARTICIPATION;
            att.compander = pVFO->Compander;
            if (state[channel & 7u] == att.__val)
                return; // no change in the attributes
        }

        state[channel & 7u] = att.__val;
        EEPROM_WriteBuffer(offset, state,8);

        gMR_ChannelAttributes[channel] = att;

        if (IS_MR_CHANNEL(channel)) {	// it's a memory channel
            if (!keep) {
                // clear/reset the channel name
                SETTINGS_SaveChannelName(channel, "");
            }
        }
    }
}