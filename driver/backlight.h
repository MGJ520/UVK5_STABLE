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

#ifndef DRIVER_BACKLIGHT_H
#define DRIVER_BACKLIGHT_H
#define BACKLIGHT_MAX_BRIGHTNESS  10
#include <stdint.h>
#include <stdbool.h>

extern uint16_t gBacklightCountdown_500ms;
extern uint8_t gBacklightBrightness;

void BACKLIGHT_Turn_Gradient(int end);
void BACKLIGHT_InitHardware();
void BACKLIGHT_TurnOn();
void BACKLIGHT_TurnOff();
bool BACKLIGHT_IsOn();
void BACKLIGHT_SetBrightness(uint8_t brightness);
uint8_t BACKLIGHT_GetBrightness(void);
#endif

