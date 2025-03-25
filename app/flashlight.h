#ifndef APP_FLASHLIGHT_H
#define APP_FLASHLIGHT_H

#ifdef ENABLE_FLASHLIGHT

#include <stdint.h>
#include <stdbool.h> // 添加这一行以包含 bool 类型定义

extern bool gFlashLightState;
extern volatile uint16_t     gFlashLightBlinkCounter;

void ACTION_FlashLight(void);

#endif

#endif