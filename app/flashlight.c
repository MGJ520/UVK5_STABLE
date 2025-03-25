#ifdef ENABLE_FLASHLIGHT

#include "driver/gpio.h"
#include "bsp/dp32g030/gpio.h"

#include "flashlight.h"

bool gFlashLightState;

void ACTION_FlashLight(void) {
    if (gFlashLightState) {
        //关灯
        GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
        gFlashLightState = 0;
    } else {
        //开灯
        GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
        gFlashLightState = 1;
    }
}

#endif