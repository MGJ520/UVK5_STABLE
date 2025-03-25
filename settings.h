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

#ifndef SETTINGS_H
#define SETTINGS_H

//收音机频率范围
#define FM_LowerLimit_define 640   //64.0mhz
#define FM_UpperLimit_define 1080  //108.0mhz

#include <stdbool.h>
#include <stdint.h>

#include "frequencies.h"
#include <helper/battery.h>
#include "radio.h"
#include <driver/backlight.h>
#if ENABLE_CHINESE_FULL==4

enum POWER_OnDisplayMode_t {
    POWER_ON_DISPLAY_MODE_NONE,
    POWER_ON_DISPLAY_MODE_PIC,
    POWER_ON_DISPLAY_MODE_MESSAGE

};
typedef enum POWER_OnDisplayMode_t POWER_OnDisplayMode_t;
#endif


enum TxLockModes_t {
    F_LOCK_DEF, // 所有默认频率 + 可配置，表示锁定所有默认频率，并允许配置其他频率
    F_LOCK_FCC, // 锁定符合美国联邦通信委员会（FCC）规定的频率
    F_LOCK_CE, // 锁定符合欧洲共同体（CE）规定的频率
    F_LOCK_GB, // 锁定符合英国（GB）规定的频率
    F_LOCK_ALL, // 在所有频率上禁用发射，表示完全锁定，不允许任何频率的发射
    F_LOCK_NONE, // 在所有频率上启用发射，表示没有频率锁定，允许所有频率的发射
    F_LOCK_LEN // 锁定频率列表的长度，可能用于表示频率锁定列表的大小或数量
};

enum {
    SCAN_RESUME_TO = 0, // 扫描恢复到原始信道，表示扫描结束后恢复到开始扫描的信道
    SCAN_RESUME_CO, // 扫描恢复到当前信道，表示扫描结束后保持在当前扫描到的信道
    SCAN_RESUME_SE // 扫描恢复到上一个结束的信道，表示扫描结束后恢复到上次扫描结束时的信道
};

enum {
    CROSS_BAND_OFF = 0,
    CROSS_BAND_CHAN_A,
    CROSS_BAND_CHAN_B
};

enum {
    DUAL_WATCH_OFF = 0,
    DUAL_WATCH_CHAN_A,
    DUAL_WATCH_CHAN_B
};

enum {
    TX_OFFSET_FREQUENCY_DIRECTION_OFF = 0,
    TX_OFFSET_FREQUENCY_DIRECTION_ADD,
    TX_OFFSET_FREQUENCY_DIRECTION_SUB
};

enum {
    OUTPUT_POWER_LOW = 0,
    OUTPUT_POWER_MID,
    OUTPUT_POWER_HIGH
};


enum ACTION_OPT_t {
    ACTION_OPT_NONE = 0,
    ACTION_OPT_FLASHLIGHT, // 闪光灯操作选项，用于配置与闪光灯相关的动作
    ACTION_OPT_POWER, // 电源操作选项，用于配置与电源相关的动作
    ACTION_OPT_MONITOR, // 监听操作选项，用于配置监听模式相关的动作
    ACTION_OPT_SCAN, // 扫描操作选项，用于配置扫描模式相关的动作
    ACTION_OPT_VOX, // 声控操作选项，用于配置声控发射相关的动作
    ACTION_OPT_ALARM, // 警报操作选项，用于配置警报功能相关的动作
    ACTION_OPT_FM, // 调频操作选项，用于配置FM收音机相关的动作
    ACTION_OPT_1750, // 1750Hz操作选项，可能用于配置1750Hz信令相关的动作
    ACTION_OPT_KEYLOCK, // 键锁定操作选项，用于配置键盘锁定相关的动作
    ACTION_OPT_A_B, // A/B操作选项，可能用于配置频道或模式的切换动作
    ACTION_OPT_VFO_MR, // VFO/MR操作选项，用于配置VFO和记忆频道之间的切换动作
    ACTION_OPT_SWITCH_DEMODUL, // 切换解调操作选项，用于配置解调方式的切换动作
    ACTION_OPT_D_DCD, // DCD（载波检测）操作选项，用于配置载波检测相关的动作
    ACTION_OPT_WIDTH, // 宽度操作选项，可能用于配置信号带宽或滤波器宽度的调整动作

#ifdef ENABLE_SIDEFUNCTIONS_SEND
    ACTION_OPT_SEND_CURRENT,
    ACTION_OPT_SEND_OTHER,
#endif
    ACTION_OPT_LEN
};

#ifdef ENABLE_VOICE
enum VOICE_Prompt_t
	{
		VOICE_PROMPT_OFF = 0,
		VOICE_PROMPT_CHINESE,
		VOICE_PROMPT_ENGLISH
	};
	typedef enum VOICE_Prompt_t VOICE_Prompt_t;
#endif

enum ALARM_Mode_t {
    ALARM_MODE_SITE = 0,
    ALARM_MODE_TONE
};
typedef enum ALARM_Mode_t ALARM_Mode_t;

//修改
enum ROGER_Mode_t {
    ROGER_MODE_OFF = 0,
    ROGER_MODE_ROGER,
    ROGER_MODE_OTHER_Motorola,
    ROGER_MODE_OTHER_Mi,
    ROGER_MODE_OTHER_HLD,
    ROGER_MODE_MDC_END,
    ROGER_MODE_MDC_HEAD,
    ROGER_MODE_MDC_BOTH,
    ROGER_MODE_MDC_HEAD_ROGER
};
typedef enum ROGER_Mode_t ROGER_Mode_t;

enum CHANNEL_DisplayMode_t {
    MDF_FREQUENCY = 0,
    MDF_CHANNEL,
    MDF_NAME,
    MDF_NAME_FREQ
};
typedef enum CHANNEL_DisplayMode_t CHANNEL_DisplayMode_t;

typedef struct {
    uint8_t               ScreenChannel[2]; // 当前在收音机中设置的信道的数组（记忆频道或频率频道）
    uint8_t               FreqChannel[2];   // 最后使用的 频率 信道的数组
    uint8_t               MrChannel[2];     // 最后使用的 记忆 信道的数组
#ifdef ENABLE_NOAA
    uint8_t           NoaaChannel[2];
#endif

    // The actual VFO index (0-upper/1-lower) that is now used for RX,
    // It is being alternated by dual watch, and flipped by crossband
    uint8_t               RX_VFO;

    // The main VFO index (0-upper/1-lower) selected by the user
    uint8_t               TX_VFO;

    uint8_t               field7_0xa;
    uint8_t               field8_0xb;

#ifdef ENABLE_FMRADIO
    uint16_t          FM_SelectedFrequency; // 当前选定的FM频率
    uint8_t           FM_SelectedChannel; // 当前选定的FM频道
    bool              FM_IsMrMode; // FM是否处于记忆模式（MrMode）
    uint16_t          FM_FrequencyPlaying; // 当前正在播放的FM频率
    uint16_t          FM_LowerLimit; // FM频率的下限
    uint16_t          FM_UpperLimit; // FM频率的上限
#endif

    uint8_t               SQUELCH_LEVEL;    // 静噪等级，用于设置接收信号必须超过的阈值以关闭静噪
    uint8_t               TX_TIMEOUT_TIMER; // 发射超时定时器，用于设置发射操作的超时时间
    bool                  KEY_LOCK;         // 键锁定标志，用于指示是否锁定键盘操作
    bool                  VOX_SWITCH;       // 声控开关标志，用于指示是否启用声控发射功能
    uint8_t               VOX_LEVEL;        // 声控级别，用于设置触发声控发射的音量阈值

#ifdef ENABLE_VOICE
    VOICE_Prompt_t    VOICE_PROMPT;
#endif
    bool                  BEEP_CONTROL; // 控制蜂鸣器是否启用的标志
    uint8_t               CHANNEL_DISPLAY_MODE; // 信道显示模式，用于设置如何显示当前信道信息
    bool                  TAIL_TONE_ELIMINATION; // 尾音清除，用于消除通信结束后的尾音
    bool                  VFO_OPEN; // VFO开启标志，指示VFO模式是否激活
    uint8_t               DUAL_WATCH; // 双监听模式设置，用于同时监听两个信道
    uint8_t               CROSS_BAND_RX_TX; // 交叉带接收/发送设置，用于配置收发频率在不同波段
    uint8_t               BATTERY_SAVE; // 电池节省模式设置，用于减少电量消耗
    uint8_t               BACKLIGHT_TIME; // 背光时间设置，用于控制显示屏背光持续时间
    uint8_t               SCAN_RESUME_MODE; // 扫描恢复模式设置，用于确定扫描操作如何恢复
    uint8_t               SCAN_LIST_DEFAULT; // 扫描列表默认设置，用于设置默认扫描列表
    bool                  SCAN_LIST_ENABLED[2]; // 扫描列表启用标志数组，用于控制不同扫描列表是否启用
    uint8_t               SCANLIST_PRIORITY_CH1[2]; // 扫描列表1的优先信道设置数组，用于设置优先扫描的信道
    uint8_t               SCANLIST_PRIORITY_CH2[2]; // 扫描列表2的优先信道设置数组，用于设置优先扫描的信道


    uint8_t               field29_0x26;
    uint8_t               field30_0x27;

    uint8_t               field37_0x32;
    uint8_t               field38_0x33;

//    bool                  AUTO_KEYPAD_LOCK;
#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
    ALARM_Mode_t      ALARM_MODE;
#endif
#if ENABLE_CHINESE_FULL==4
    POWER_OnDisplayMode_t POWER_ON_DISPLAY_MODE;
#endif
    ROGER_Mode_t          ROGER; // 尾音模式类型，用于设置确认信号的模式或行为
    uint8_t               REPEATER_TAIL_TONE_ELIMINATION; // 中继器尾音消除设置，用于配置中继器尾音消除的功能或等级
#ifdef ENABLE_CUSTOM_SIDEFUNCTIONS
    uint8_t               KEY_1_SHORT_PRESS_ACTION; // 键1短按动作，定义键1短按时的功能或操作
    uint8_t               KEY_1_LONG_PRESS_ACTION; // 键1长按动作，定义键1长按时的功能或操作
    uint8_t               KEY_2_SHORT_PRESS_ACTION; // 键2短按动作，定义键2短按时的功能或操作
    uint8_t               KEY_2_LONG_PRESS_ACTION; // 键2长按动作，定义键2长按时的功能或操作

#endif
    uint8_t               MIC_SENSITIVITY;        // 麦克风灵敏度，用于设置麦克风的增益级别
    uint8_t               MIC_SENSITIVITY_TUNING; // 麦克风增益具体数值

    // 修改 uint8_t               CHAN_1_CALL;
#ifdef ENABLE_DTMF_CALLING
    char                  ANI_DTMF_ID[8];
	char                  KILL_CODE[8];
	char                  REVIVE_CODE[8];
#endif
    char                  DTMF_UP_CODE[16];

    uint8_t               field57_0x6c;
    uint8_t               field58_0x6d;

    char                  DTMF_DOWN_CODE[16];

    uint8_t               field60_0x7e;
    uint8_t               field61_0x7f;

#ifdef ENABLE_DTMF_CALLING
    char                  DTMF_SEPARATE_CODE; // DTMF分离码，用于定义DTMF信号中用于分离的字符
    char                  DTMF_GROUP_CALL_CODE; // DTMF群呼码，用于定义DTMF信号中用于发起群呼的字符
    uint8_t               DTMF_DECODE_RESPONSE; // DTMF解码响应，设置DTMF信号解码后的响应行为
    uint8_t               DTMF_auto_reset_time; // DTMF自动重置时间，设置DTMF功能在一段时间无操作后自动重置的时间

#endif
    uint16_t              DTMF_PRELOAD_TIME; // DTMF预加载时间，设置DTMF信号开始前的时间延迟
    uint16_t              DTMF_FIRST_CODE_PERSIST_TIME; // DTMF首个码持久时间，设置DTMF信号首个码的持续时间
    uint16_t              DTMF_HASH_CODE_PERSIST_TIME; // DTMF井号码持久时间，设置DTMF井号(#)码的持续时间
    uint16_t              DTMF_CODE_PERSIST_TIME; // DTMF码持久时间，设置DTMF信号中普通码的持续时间
    uint16_t              DTMF_CODE_INTERVAL_TIME; // DTMF码间隔时间，设置DTMF信号中各个码之间的间隔时间
    bool                  DTMF_SIDE_TONE; // DTMF侧音标志，用于指示是否在发送DTMF信号时启用侧音

#ifdef ENABLE_DTMF_CALLING
    bool                  PERMIT_REMOTE_KILL; //摇臂功能
#endif
    int16_t               BK4819_XTAL_FREQ_LOW;
#ifdef ENABLE_NOAA
    bool              NOAA_AUTO_SCAN;
#endif
    uint8_t               VOLUME_GAIN;
    uint8_t               DAC_GAIN;

    VFO_Info_t            VfoInfo[2];
//    uint32_t              POWER_ON_PASSWORD; 取消这个
    uint16_t              VOX1_THRESHOLD;
    uint16_t              VOX0_THRESHOLD;

    uint8_t               field77_0x95;
    uint8_t               field78_0x96;
    uint8_t               field79_0x97;

#ifdef ENABLE_CUSTOM_SIDEFUNCTIONS
    uint8_t 			  KEY_M_LONG_PRESS_ACTION;
#endif
    uint8_t               BACKLIGHT_MAX;

    BATTERY_Type_t		  BATTERY_TYPE;
#ifdef ENABLE_RSSI_BAR
    uint8_t               S0_LEVEL;
	uint8_t               S9_LEVEL;
#endif
    uint32_t MDC1200_ID;
} EEPROM_Config_t;

extern EEPROM_Config_t gEeprom;

void     SETTINGS_InitEEPROM(void);
void     SETTINGS_LoadCalibration(void);
uint32_t SETTINGS_FetchChannelFrequency(const int channel);
void     SETTINGS_FetchChannelName(char *s, const int channel);
void     SETTINGS_FactoryReset(bool bIsAll);
#ifdef ENABLE_FMRADIO
void SETTINGS_SaveFM(void);
#endif
void SETTINGS_SaveVfoIndices(void);
void SETTINGS_SaveSettings(void);
void SETTINGS_SaveChannelName(uint8_t channel, const char * name);
void SETTINGS_SaveChannel(uint8_t Channel, uint8_t VFO, const VFO_Info_t *pVFO, uint8_t Mode);
void SETTINGS_SaveBatteryCalibration(const uint16_t * batteryCalibration);
void SETTINGS_UpdateChannel(uint8_t channel, const VFO_Info_t *pVFO, bool keep);



#endif