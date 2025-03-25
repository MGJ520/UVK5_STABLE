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
#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "bitmaps.h"
#include "driver/st7565.h"
#include "functions.h"
#include "ui/battery.h"
#include "../misc.h"

void UI_DrawBattery(uint8_t *bitmap, uint8_t level, uint8_t blink) {
    if (level < 2 && blink == 1) {
        memset(bitmap, 0, sizeof(BITMAP_BatteryLevel1));
        return;
    }

    memcpy(bitmap, BITMAP_BatteryLevel1, sizeof(BITMAP_BatteryLevel1));

    if (level <= 2) {
        return;
    }

    const uint8_t bars = MIN(4, level - 2);
    for (int i = 0; i < bars; i++) {
#ifndef ENABLE_REVERSE_BAT_SYMBOL
        memcpy(bitmap + sizeof(BITMAP_BatteryLevel1) - 4 - (i * 3), BITMAP_BatteryLevel, 2);
#else
        memcpy(bitmap + 3 + (i * 3) + 0, BITMAP_BatteryLevel, 2);
#endif
    }
}

// 显示电池电量的用户界面函数
void UI_DisplayBattery(uint8_t level, uint8_t blink) {
    // 定义一个数组用于存储电池电量图标的位图数据，大小与BITMAP_BatteryLevel1相同
    uint8_t bitmap[sizeof(BITMAP_BatteryLevel1)];

    // 根据电池电量和是否闪烁的要求，绘制电池电量图标
    UI_DrawBattery(bitmap, level, blink);

    // 在LCD屏幕上绘制电池电量图标，位置在屏幕最右侧，从顶部开始
    // LCD_WIDTH - sizeof(bitmap) 确定了图标的X坐标，使其位于屏幕最右侧
    // 0 是图标的Y坐标，表示从顶部开始
    // bitmap 是图标位图的指针
    // sizeof(bitmap) 是位图数据的大小
    ST7565_DrawLine(LCD_WIDTH - sizeof(bitmap), 0, bitmap, sizeof(bitmap));
}
