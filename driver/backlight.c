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

#include "backlight.h"
#include "bsp/dp32g030/gpio.h"
#include "bsp/dp32g030/pwmplus.h"
#include "bsp/dp32g030/portcon.h"
#include "driver/gpio.h"
#include "settings.h"
#include "system.h"

// this is decremented once every 500ms
uint16_t gBacklightCountdown_500ms = 0;
bool backlightOn;

void BACKLIGHT_InitHardware() {
    // 48MHz / 94 / 1024 ~ 500Hz
    //修改 消除干扰
    const uint32_t PWM_FREQUENCY_HZ = 25000;
    PWM_PLUS0_CLKSRC |= ((48000000 / 1024 / PWM_FREQUENCY_HZ) << 16);
    PWM_PLUS0_PERIOD = 1023;

    PORTCON_PORTB_SEL0 &= ~(0
                            // Back light
                            | PORTCON_PORTB_SEL0_B6_MASK
    );
    PORTCON_PORTB_SEL0 |= 0
                          // Back light PWM
                          | PORTCON_PORTB_SEL0_B6_BITS_PWMP0_CH0;

    PWM_PLUS0_GEN =
            PWMPLUS_GEN_CH0_OE_BITS_ENABLE |
            PWMPLUS_GEN_CH0_OUTINV_BITS_ENABLE |
            0;

    PWM_PLUS0_CFG =
            PWMPLUS_CFG_CNT_REP_BITS_ENABLE |
            PWMPLUS_CFG_COUNTER_EN_BITS_ENABLE |
            0;
}

void BACKLIGHT_Turn_Gradient(int end) {

    // 将start和end映射到PWM_MAX范围内的值
    int end_mapped = (1023ul * end * end * end) / (BACKLIGHT_MAX_BRIGHTNESS * BACKLIGHT_MAX_BRIGHTNESS * BACKLIGHT_MAX_BRIGHTNESS);

    // 分段线性映射的步数时间0.9s
    int time = 1050;

    // 初始化当前PWM值
    int current_value = 0;

    // 生成非线性映射的PWM值
    for (int i = 0; i <= time; ++i) {
        // 延迟以模拟渐变效果
        SYSTEM_DelayMs(1);
        // 设置PWM值
        PWM_PLUS0_CH0_COMP = current_value;
        // 计算下一个PWM值
        if (i < end_mapped *(2/5)) {
            // 在前半部分增加步长较快
            current_value += 2;
        } else {
            // 在后半部分增加步长较慢
            current_value += 1;
        }
        // 确保不会超出范围
        if (current_value > end_mapped) {
            current_value = end_mapped;
        }
    }
    PWM_PLUS0_CH0_COMP = end_mapped;
}

void BACKLIGHT_TurnOn(void) {
    if (gEeprom.BACKLIGHT_TIME == 0) {
        BACKLIGHT_TurnOff();
        return;
    }

    backlightOn = true;
    BACKLIGHT_SetBrightness(gEeprom.BACKLIGHT_MAX);

    switch (gEeprom.BACKLIGHT_TIME) {
        default:
        case 1:    // 5 sec
        case 2:    // 10 sec
        case 3:    // 20 sec
            gBacklightCountdown_500ms = 1 + (2 << (gEeprom.BACKLIGHT_TIME - 1)) * 5;
            break;
        case 4:    // 1 min
        case 5:    // 2 min
        case 6:    // 4 min
            gBacklightCountdown_500ms = 1 + (2 << (gEeprom.BACKLIGHT_TIME - 4)) * 60;
            break;
        case 7:    // always on
            gBacklightCountdown_500ms = 0;
            break;
    }
}

void BACKLIGHT_TurnOff() {
    BACKLIGHT_SetBrightness(0);
    gBacklightCountdown_500ms = 0;
    backlightOn = false;
}

bool BACKLIGHT_IsOn() {
    return backlightOn;
}

static uint8_t currentBrightness;

//设置背光亮度
void BACKLIGHT_SetBrightness(uint8_t brightness) {

    if (brightness > BACKLIGHT_MAX_BRIGHTNESS)
        brightness = BACKLIGHT_MAX_BRIGHTNESS;
    if(brightness==1)
        PWM_PLUS0_CH0_COMP=0;
    else
        PWM_PLUS0_CH0_COMP = (1023ul * brightness * brightness * brightness) / (BACKLIGHT_MAX_BRIGHTNESS * BACKLIGHT_MAX_BRIGHTNESS * BACKLIGHT_MAX_BRIGHTNESS);
}

uint8_t BACKLIGHT_GetBrightness(void) {
    return currentBrightness;
}