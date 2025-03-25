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

#include "battery.h"
#include "driver/backlight.h"
#include "driver/st7565.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/battery.h"
#include "ui/menu.h"
#include "ui/ui.h"
#include "driver/eeprom.h"

// 上一次记录的电池百分比
uint8_t previousPercent;
// 电池循环次数计数器
uint16_t cycleCount;
// 自循环开始以来消耗的总电池百分比
uint8_t totalConsumedPercent;
//当前电池电量
uint8_t currentPercent;


uint16_t gBatteryCalibration[6];
uint16_t gBatteryCurrentVoltage;
uint16_t gBatteryCurrent;
uint16_t gBatteryVoltages[4];
uint16_t gBatteryVoltageAverage;
uint8_t gBatteryDisplayLevel;
bool gChargingWithTypeC;
bool gLowBatteryBlink;
bool gLowBattery;
bool gLowBatteryConfirmed;
uint16_t gBatteryCheckCounter;


typedef enum {
    BATTERY_LOW_INACTIVE,
    BATTERY_LOW_ACTIVE,
    BATTERY_LOW_CONFIRMED
} BatteryLow_t;


uint16_t lowBatteryCountdown;
const uint16_t lowBatteryPeriod = 30;

volatile uint16_t gPowerSave_10ms;


const uint16_t Voltage2PercentageTable[][7][2] = {
        [BATTERY_TYPE_1600_MAH] = {
                {828, 100},
                {814, 97},
                {760, 25},
                {729, 6},
                {630, 0},
                {0,   0},
                {0,   0},
        },

        [BATTERY_TYPE_2200_MAH] = {
                {832, 100},
                {813, 95},
                {740, 60},
                {707, 21},
                {682, 5},
                {630, 0},
                {0,   0},
        },
};

static_assert(ARRAY_SIZE(Voltage2PercentageTable[BATTERY_TYPE_1600_MAH]) ==
              ARRAY_SIZE(Voltage2PercentageTable[BATTERY_TYPE_2200_MAH]));




void SavecycleCount()
{
    //密码eeprom 范围
    uint8_t Data[4]; // 创建一个 4 字节的缓冲区
   // 假设使用小端字节序（低地址存放低位字节）
    Data[0] = (uint8_t)(cycleCount & 0xFF);
    Data[1] = (uint8_t)((cycleCount >> 8) & 0xFF);
    Data[2] = (uint8_t)(totalConsumedPercent & 0xFF);
    Data[3] = (uint8_t)(previousPercent & 0xFF);
    EEPROM_WriteBuffer(0x0E99,Data, 4);
}


void ReadcycleCount()
{
    uint8_t Data[4]={0,0,0,0}; // 创建一个 4 字节的缓冲区
    // 假设使用小端字节序（低地址存放低位字节）
    EEPROM_ReadBuffer(0x0E99,Data, 4);
    cycleCount=         (Data[1]<<8)+
                         Data[0] ;
    totalConsumedPercent=Data[2];
    previousPercent     =Data[3];
    currentPercent      =0;

}

//更新循环数据
void updateBatteryCycleStats() {
    // 如果是第一次开机，初始化起始百分比和前一次百分比
    if (previousPercent == 0) {
        previousPercent = currentPercent; // 设置前一次百分比为当前百分比
        return; // 初始化完成后直接返回
    }
    uint16_t deltaPercent=0;
    // 检测电池是否充电或消耗
    if (currentPercent > previousPercent)
    {
        // 如果当前百分比高于前一次百分比，则认为电池在上次关机时充电了
        deltaPercent = currentPercent - previousPercent; // 计算消耗的电量百分比
    }
    else if (currentPercent < previousPercent)
    {
        // 如果当前百分比低于前一次百分比，则认为电池在上次开机期间消耗了电量
        deltaPercent = previousPercent - currentPercent; // 计算消耗的电量百分比
    }
    totalConsumedPercent += deltaPercent; // 累加消耗的电量到总消耗百分比
    // 检查是否完成了一次电池循环（消耗了100%的电量）
    if (totalConsumedPercent >= 100)
    {
        cycleCount++; // 增加循环次数
        // 重置统计变量以开始下一次循环统计
        totalConsumedPercent = 0; // 重置总消耗百分比为0
        SavecycleCount(); // 保存循环次数（假设有这样一个函数）
    }
    // 更新前一次百分比为当前百分比，为下一次统计做准备
    previousPercent = currentPercent;
}

// 将电池电压转换为电量百分比的函数
unsigned int BATTERY_VoltsToPercent(const unsigned int voltage_10mV) {
    // 根据电池类型获取电压到百分比转换表
    const uint16_t (*crv)[2] = Voltage2PercentageTable[gEeprom.BATTERY_TYPE];

    // 定义一个乘法因子，用于计算过程中的比例转换
    const int mulipl = 1000;

    // 遍历电压到百分比转换表，寻找电压值所在的区间
    for (unsigned int i = 1; i < ARRAY_SIZE(Voltage2PercentageTable[BATTERY_TYPE_2200_MAH]); i++) {
        // 如果当前电压大于表中的电压值，则找到了对应的区间
        if (voltage_10mV > crv[i][0]) {
            // 计算斜率a（电压-百分比关系曲线的斜率）
            const int a = (crv[i - 1][1] - crv[i][1]) * mulipl / (crv[i - 1][0] - crv[i][0]);

            // 计算截距b（电压-百分比关系曲线的y轴截距）
            const int b = crv[i][1] - a * crv[i][0] / mulipl;

            // 根据斜率和截距计算当前电压对应的百分比
            const int p = a * voltage_10mV / mulipl + b;

            // 返回计算得到的百分比，但不超过100%
            return MIN(p, 100);
        }
    }
    // 如果电压低于表中的所有值，则返回0%
    return 0;
}


void BATTERY_GetReadings(const bool bDisplayBatteryLevel) {
// 保存之前的电池电量显示级别
    const uint8_t PreviousBatteryLevel = gBatteryDisplayLevel;

// 计算电池的平均电压，通过将四个电池电压值相加然后除以4得到
    const uint16_t Voltage =
           (gBatteryVoltages[0] +
            gBatteryVoltages[1] +
            gBatteryVoltages[2] +
            gBatteryVoltages[3]) / 4;

// 根据电池校准值计算电池电压的平均值
    gBatteryVoltageAverage = (Voltage * 760) / gBatteryCalibration[3];

// 判断电池电压平均值，并设置显示级别
    if (gBatteryVoltageAverage > 890)
        gBatteryDisplayLevel = 7; // 电池过压
    else if (gBatteryVoltageAverage < 630)
        gBatteryDisplayLevel = 0; // 电池电量严重不足
    else {
        // 默认电池显示级别设为1
        gBatteryDisplayLevel = 1;
        // 定义一个数组，存储电池电量的不同级别对应的百分比阈值
        const uint8_t levels[] = {5, 17, 41, 65, 88};
        // 调用函数将电池电压转换为百分比
        currentPercent = BATTERY_VoltsToPercent(gBatteryVoltageAverage);
        // 从高到低遍历电池电量的不同级别
        for (uint8_t i = 6; i >= 1; i--) {
            // 如果当前百分比大于当前级别的阈值，则设置电池显示级别
            if (currentPercent> levels[i - 2]) {
                gBatteryDisplayLevel = i;
                break; // 找到对应的级别后退出循环
            }
        }
    }

    if ((gScreenToDisplay == DISPLAY_MENU))
        gUpdateDisplay = true;

    if (gBatteryCurrent < 501) {
        if (gChargingWithTypeC) {
            gUpdateStatus = true;
            gUpdateDisplay = true;
        }

        gChargingWithTypeC = false;
    } else {
        if (!gChargingWithTypeC) {
            gUpdateStatus = true;
            gUpdateDisplay = true;
            BACKLIGHT_TurnOn();
        }
        gChargingWithTypeC = true;
    }

    if (PreviousBatteryLevel != gBatteryDisplayLevel) {
        if (gBatteryDisplayLevel > 2)
            gLowBatteryConfirmed = false;
        else if (gBatteryDisplayLevel < 2) {
            gLowBattery = true;
        } else {
            gLowBattery = false;
            if (bDisplayBatteryLevel)
                UI_DisplayBattery(gBatteryDisplayLevel, gLowBatteryBlink);
        }

        if (!gLowBatteryConfirmed)
            gUpdateDisplay = true;

        lowBatteryCountdown = 0;
    }

    //修改 增加循环功能
    updateBatteryCycleStats();
}

void BATTERY_TimeSlice500ms(void) {
    if (!gLowBattery) {
        return;
    }

    gLowBatteryBlink = ++lowBatteryCountdown & 1;

    UI_DisplayBattery(0, gLowBatteryBlink);

    if (gCurrentFunction == FUNCTION_TRANSMIT) {
        return;
    }

    // not transmitting

    if (lowBatteryCountdown < lowBatteryPeriod) {
#ifdef ENABLE_WARNING
        if (lowBatteryCountdown == lowBatteryPeriod-1 && !gChargingWithTypeC && !gLowBatteryConfirmed) {
            //电量不足报警
            AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP);
        }
#endif
        return;
    }

    lowBatteryCountdown = 0;

    if (gChargingWithTypeC) {
        return;
    }

    // not on charge
    if (!gLowBatteryConfirmed) {
        AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP);
#ifdef ENABLE_VOICE
        AUDIO_SetVoiceID(0, VOICE_ID_LOW_VOLTAGE);
#endif
    }

    if (gBatteryDisplayLevel != 0) {
#ifdef ENABLE_VOICE
        AUDIO_PlaySingleVoice(false);
#endif
        return;
    }

#ifdef ENABLE_VOICE
    AUDIO_PlaySingleVoice(true);
#endif

    gReducedService = true;

    FUNCTION_Select(FUNCTION_POWER_SAVE);

    ST7565_HardwareReset();

    if (gEeprom.BACKLIGHT_TIME < (ARRAY_SIZE(gSubMenu_BACKLIGHT) - 1)) {
        BACKLIGHT_TurnOff();
    }
}