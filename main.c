#include "bsp/dp32g030/gpio.h"
#include "driver/gpio.h"
#include "app/si.h"

#include "driver/i2c.h"
#include "driver/system.h"
#include "frequencies.h"
#include "misc.h"
#include "app/doppler.h"
#include "driver/uart.h"
#include "string.h"
#include <stdio.h>
#include "ui/helper.h"
#include <string.h>
#include "driver/bk4819.h"
#include "font.h"
#include "ui/ui.h"
#include <stdint.h>
#include <string.h>
#include "font.h"
#include <stdio.h>     // NULL
#include "app/mdc1200.h"
#include "app/uart.h"
#include "string.h"
#include "app/messenger.h"

#ifdef ENABLE_DOPPLER
#include "app/doppler.h"
#endif
#ifdef ENABLE_AM_FIX
#include "am_fix.h"
#endif

#include "bsp/dp32g030/rtc.h"

#ifdef ENABLE_TIMER
#include "bsp/dp32g030/uart.h"
#include "bsp/dp32g030/timer.h"
#endif

#ifdef ENABLE_4732
#endif

#include "audio.h"
#include "board.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "version.h"
#include "app/app.h"
#include "app/dtmf.h"
#include "bsp/dp32g030/gpio.h"
#include "bsp/dp32g030/syscon.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/systick.h"

#ifdef ENABLE_UART
#include "driver/uart.h"
#endif

#include "app/spectrum.h"

#include "helper/battery.h"
#include "helper/boot.h"


#include "ui/welcome.h"
#include "ui/menu.h"
#include "driver/eeprom.h"
#include "driver/st7565.h"

// 自定义 putchar 函数，用于 UART 发送字符
void _putchar(char c) {
    // 如果启用 UART，则发送字符
#ifdef ENABLE_UART
    UART_Send((uint8_t *) &c, 1);
#endif
}

// 主函数
void Main(void) {
    // 启用所需的时钟门控
    SYSCON_DEV_CLK_GATE = 0
                          | SYSCON_DEV_CLK_GATE_GPIOA_BITS_ENABLE // 启用 GPIOA 时钟
                          | SYSCON_DEV_CLK_GATE_GPIOB_BITS_ENABLE // 启用 GPIOB 时钟
                          | SYSCON_DEV_CLK_GATE_GPIOC_BITS_ENABLE // 启用 GPIOC 时钟
                          | SYSCON_DEV_CLK_GATE_UART1_BITS_ENABLE // 启用 UART1 时钟
                          | SYSCON_DEV_CLK_GATE_SPI0_BITS_ENABLE // 启用 SPI0 时钟
                          | SYSCON_DEV_CLK_GATE_SARADC_BITS_ENABLE // 启用 SARADC 时钟
                          | SYSCON_DEV_CLK_GATE_CRC_BITS_ENABLE // 启用 CRC 时钟
                          | SYSCON_DEV_CLK_GATE_AES_BITS_ENABLE // 启用 AES 时钟
                          | SYSCON_DEV_CLK_GATE_PWM_PLUS0_BITS_ENABLE // 启用 PWM0 时钟
#ifdef ENABLE_DOPPLER
        | (1 << 22) // 启用 Doppler 相关时钟
#endif
            ;

    // 初始化系统滴答定时器
    SYSTICK_Init();

    // 初始化板级支持包
    BOARD_Init();

    //修复Bug,确保i2c可以使用
    while (Int_EEPROM_OK()==false);

#ifdef ENABLE_UART
    // 初始化 UART
    UART_Init();
#endif

    // 初始化 DTMF 字符串缓冲区
    memset(gDTMF_String, '-', sizeof(gDTMF_String));
    gDTMF_String[sizeof(gDTMF_String) - 1] = 0;

    // 初始化 BK4819 模块
    BK4819_Init();

    //修改增加 关闭加密
    BK4819_DisableScramble();

    // 获取电池信息
    BOARD_ADC_GetBatteryInfo(&gBatteryCurrentVoltage, &gBatteryCurrent);

    // 初始化 EEPROM 中的设置
    SETTINGS_InitEEPROM();

    // 加载校准数据
    SETTINGS_LoadCalibration();

#ifdef ENABLE_MESSENGER
    // 初始化消息
    MSG_Init();
#endif

#ifdef ENABLE_MDC1200
    // 初始化 MDC1200
    MDC1200_init();
#endif

#ifdef ENABLE_DOPPLER
    // 初始化实时时钟和 Doppler 数据
    RTC_INIT();
    INIT_DOPPLER_DATA();
#endif

    //mic BUG修复
    BK4819_set_mic_gain(gEeprom.MIC_SENSITIVITY_TUNING);

    // 配置无线电通道
    RADIO_ConfigureChannel(0, VFO_CONFIGURE_RELOAD);
    RADIO_ConfigureChannel(1, VFO_CONFIGURE_RELOAD);

    // 选择 VFO
    RADIO_SelectVfos();

    // 设置无线电寄存器
    RADIO_SetupRegisters(true);

    // 读取电池电压数据
    for (uint32_t i = 0; i < ARRAY_SIZE(gBatteryVoltages); i++) {
        BOARD_ADC_GetBatteryInfo(&gBatteryVoltages[i], &gBatteryCurrent);
    }
    // 获取电池读数，不显示
    BATTERY_GetReadings(false);
    // 初始化 AM 修复
#ifdef ENABLE_AM_FIX
    AM_fix_init();
#endif
//菜单数量-------------------------------------------------
#if ENABLE_CHINESE_FULL == 0
    // 设置菜单列表计数为 52
    gMenuListCount = 52;
#else
    // 设置菜单列表计数为 53
    gMenuListCount = 53;
#endif

    // 初始化按键读取为无效
    gKeyReading0 = KEY_INVALID;
    gKeyReading1 = KEY_INVALID;
    // 初始化消抖计数器
    gDebounceCounter = 0;

#ifdef ENABLE_TIMER
    // 初始化端口配置
    BOARD_PORTCON_Init();
    // 初始化 GPIO
    BOARD_GPIO_Init();
    // 初始化 ST7565 控制器
    ST7565_Init();
    // 初始化定时器 0
    TIM0_INIT();
    // 清空状态行
    memset(gStatusLine, 0, sizeof(gStatusLine));
    // 清空显示
    UI_DisplayClear();
    // 清空状态行
    ST7565_BlitStatusLine();
    // 全屏更新
    ST7565_BlitFullScreen();
#endif

    // 显示欢迎界面
    UI_DisplayWelcome();

#ifdef ENABLE_BOOTLOADER
    // 如果按键是菜单键，则进入引导加载程序
    if(KEYBOARD_Poll() == KEY_MENU) {
        for (int i = 0; i < 10*1024; i += 4) {
            uint32_t c;
            EEPROM_ReadBuffer(0x41000 + i, (uint8_t *) &c, 4);
            write_to_memory(0x20001000 + i, c);
        }
        // 跳转到引导加载程序的入口点
        JUMP_TO_FLASH(0x2000110a, 0x20003ff0);
    }
#endif

//    // 初始化启动计数器
//    boot_counter_10ms = 10;
//
//    // 如果启动计数器大于 0 或者有按键按下，则循环
//    while (boot_counter_10ms > 0 ) {
//        // 如果按键是退出键，则停止启动蜂鸣声
//        if (KEYBOARD_Poll() == KEY_EXIT
//#if ENABLE_CHINESE_FULL == 4
//            || gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_NONE
//#endif
//           )
//        {
//            boot_counter_10ms = 0;
//            break;
//        }
//
#ifdef ENABLE_BOOT_BEEPS
            AUDIO_PlayBeep(BEEP_880HZ_40MS_OPTIONAL);
#endif
//}


    // 选择下一个显示界面为主界面
    GUI_SelectNextDisplay(DISPLAY_MAIN);

    // 清除语音 0 的 GPIO
    GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_VOICE_0);

    // 设置更新状态为真
    gUpdateStatus = true;

#ifdef ENABLE_VOICE
    {
        uint8_t Channel;
        // 设置语音 ID 和播放欢迎语音
        AUDIO_SetVoiceID(0, VOICE_ID_WELCOME);

        Channel = gEeprom.ScreenChannel[gEeprom.TX_VFO];
        if (IS_MR_CHANNEL(Channel))
        {
            AUDIO_SetVoiceID(1, VOICE_ID_CHANNEL_MODE);
            AUDIO_SetDigitVoice(2, Channel + 1);
        }
        else if (IS_FREQ_CHANNEL(Channel))
            AUDIO_SetVoiceID(1, VOICE_ID_FREQUENCY_MODE);

        AUDIO_PlaySingleVoice(0);
    }
#endif

#ifdef ENABLE_NOAA
    // 配置 NOAA 模式
    RADIO_ConfigureNOAA();
#endif

    while (true) {
        // 更新应用程序状态
        APP_Update();

        // 执行 10 毫秒时间片
        if (gNextTimeslice) {
            APP_TimeSlice10ms();
        }

        // 执行 500 毫秒时间片
        if (gNextTimeslice_500ms) {
            APP_TimeSlice500ms();
        }
    }
}
