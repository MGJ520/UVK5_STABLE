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
#include <stdint.h>
#include <string.h>
#include "app/mdc1200.h"
#include "app/dtmf.h"

#if defined(ENABLE_FMRADIO)
#include "app/fm.h"
#endif

#include "audio.h"
#include "bsp/dp32g030/gpio.h"
#include "dcs.h"
#include "driver/backlight.h"
#ifdef ENABLE_MESSENGER
#include "app/messenger.h"
#endif
#ifdef ENABLE_DOPPLER
#include "app/doppler.h"
#endif
#if defined(ENABLE_FMRADIO)
#include "driver/bk1080.h"
#endif

#include "driver/bk4819.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/st7565.h"
#include "frequencies.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/status.h"
#include "ui/ui.h"

FUNCTION_Type_t gCurrentFunction;


bool FUNCTION_IsRx()
{
    return gCurrentFunction == FUNCTION_MONITOR ||
           gCurrentFunction == FUNCTION_INCOMING ||
           gCurrentFunction == FUNCTION_RECEIVE;
}

void FUNCTION_Init(void)
{
    g_CxCSS_TAIL_Found = false;
    g_CDCSS_Lost       = false;
    g_CTCSS_Lost       = false;

    g_SquelchLost      = false;

    gFlagTailToneEliminationComplete   = false;
    gTailToneEliminationCountdown_10ms = 0;
    gFoundCTCSS                        = false;
    gFoundCDCSS                        = false;
    gFoundCTCSSCountdown_10ms          = 0;
    gFoundCDCSSCountdown_10ms          = 0;
    gEndOfRxDetectedMaybe              = false;

    gCurrentCodeType = (gRxVfo->Modulation != MODULATION_FM) ? CODE_TYPE_OFF : gRxVfo->pRX->CodeType;

#ifdef ENABLE_VOX
    g_VOX_Lost     = false;
#endif

#ifdef ENABLE_DTMF_CALLING
    DTMF_clear_RX();
#endif

#ifdef ENABLE_NOAA
    gNOAACountdown_10ms = 0;

	if (IS_NOAA_CHANNEL(gRxVfo->CHANNEL_SAVE)) {
		gCurrentCodeType = CODE_TYPE_CONTINUOUS_TONE;
	}
#endif

    gUpdateStatus = true;
}

void FUNCTION_Foreground(const FUNCTION_Type_t PreviousFunction) {
#ifdef ENABLE_DTMF_CALLING
    if (gDTMF_ReplyState != DTMF_REPLY_NONE)
        RADIO_PrepareCssTX();
#endif
    if (PreviousFunction == FUNCTION_TRANSMIT) {
        ST7565_FixInterfGlitch();
        gVFO_RSSI_bar_level[0] = 0;
        gVFO_RSSI_bar_level[1] = 0;
    } else if (PreviousFunction != FUNCTION_RECEIVE) {
        return;
    }

#if defined(ENABLE_FMRADIO)
    if (gFmRadioMode)
        gFM_RestoreCountdown_10ms = fm_restore_countdown_10ms;
#endif

#ifdef ENABLE_DTMF_CALLING
    if (gDTMF_CallState == DTMF_CALL_STATE_CALL_OUT ||
        gDTMF_CallState == DTMF_CALL_STATE_RECEIVED ||
        gDTMF_CallState == DTMF_CALL_STATE_RECEIVED_STAY)
    {
        gDTMF_auto_reset_time_500ms = gEeprom.DTMF_auto_reset_time * 2;
    }
#endif
    gUpdateStatus = true;
}

void FUNCTION_PowerSave(void) {
    // 设置电源节省的10毫秒倒计时
    gPowerSave_10ms = gEeprom.BATTERY_SAVE * 10;
    // 设置电源节省倒计时已过期为false
    gPowerSaveCountdownExpired = false;
    // 设置接收器处于空闲模式
    gRxIdleMode = true;
    // 关闭监视器
    gMonitor = false;
    // 禁用Vox（声控）功能
    BK4819_DisableVox();
    // 进入BK4819芯片的睡眠模式
    BK4819_Sleep();
    // 关闭接收使能的GPIO输出，即不启用接收功能
    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, false);
    // 设置需要更新状态显示
    gUpdateStatus = true;
    // 如果当前不是显示菜单界面
    if (gScreenToDisplay != DISPLAY_MENU) {
        //  don't close the menu，不关闭菜单界面
        GUI_SelectNextDisplay(DISPLAY_MAIN);
    }
}


//发射信号
void FUNCTION_Transmit() {
    // if DTMF is enabled when TX'ing, it changes the TX audio filtering !! .. 1of11

#if defined(ENABLE_MESSENGER) || defined(ENABLE_MDC1200)
    enable_msg_rx(false);
#endif


    BK4819_DisableDTMF();

#ifdef ENABLE_DTMF_CALLING
    // clear the DTMF RX buffer
    DTMF_clear_RX();
#endif

    // clear the DTMF RX live decoder buffer
    gDTMF_RX_live_timeout = 0;
    memset(gDTMF_RX_live, 0, sizeof(gDTMF_RX_live));

#if defined(ENABLE_FMRADIO)
    if (gFmRadioMode)
        BK1080_Init(0, false);
#endif

#ifdef ENABLE_ALARM
    if (gAlarmState == ALARM_STATE_SITE_ALARM)
    {
        GUI_DisplayScreen();

        AUDIO_AudioPathOff();

        SYSTEM_DelayMs(20);
        BK4819_PlayTone(500, 0);
        SYSTEM_DelayMs(2);

        AUDIO_AudioPathOn();

        gEnableSpeaker = true;

        SYSTEM_DelayMs(60);
        BK4819_ExitTxMute();

        gAlarmToneCounter = 0;
        return;
    }
#endif

    gUpdateStatus = true;

    GUI_DisplayScreen();

//    if (gCurrentVfo->SCRAMBLING_TYPE > 0 && gSetting_ScrambleEnable)
//        BK4819_EnableScramble(gCurrentVfo->SCRAMBLING_TYPE - 1);
//    else
//        BK4819_DisableScramble();

    //发射信号
    RADIO_SetTxParameters();

    //打开红灯
    // turn the RED LED on
    BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, true);

    DTMF_Reply();
#ifdef ENABLE_MDC1200
#ifdef ENABLE_MESSENGER
    if(!stop_mdc_flag){
#endif
    if ((gEeprom.ROGER == ROGER_MODE_MDC_HEAD || gEeprom.ROGER == ROGER_MODE_MDC_BOTH ||gEeprom.ROGER == ROGER_MODE_MDC_HEAD_ROGER)


        ) {
        BK4819_send_MDC1200(1, 0x80, gEeprom.MDC1200_ID, true);

#ifdef ENABLE_MDC1200_SIDE_BEEP
        BK4819_start_tone(880, 10, true, true);
                                    SYSTEM_DelayMs(120);
                                    BK4819_stop_tones(true);
#endif
    } else
#endif
    if (gCurrentVfo->DTMF_PTT_ID_TX_MODE == PTT_ID_APOLLO)
        BK4819_PlaySingleTone(2525, 250, 0, gEeprom.DTMF_SIDE_TONE);
#ifdef ENABLE_MESSENGER
    #ifdef ENABLE_MDC1200

    }
    #endif

#endif
#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
    if (gAlarmState != ALARM_STATE_OFF) {
#ifdef ENABLE_TX1750
        if (gAlarmState == ALARM_STATE_TX1750)
            BK4819_TransmitTone(true, 1750);
#endif

#ifdef ENABLE_ALARM
        if (gAlarmState == ALARM_STATE_TXALARM)
            BK4819_TransmitTone(true, 500);

        gAlarmToneCounter = 0;
#endif

        SYSTEM_DelayMs(2);
        AUDIO_AudioPathOn();
        gEnableSpeaker = true;

        return;
    }
#endif



//    if (gSetting_backlight_on_tx_rx & BACKLIGHT_ON_TR_TX) {
         BACKLIGHT_TurnOn();
//    }
}


void FUNCTION_Select(FUNCTION_Type_t Function) {
    const FUNCTION_Type_t PreviousFunction = gCurrentFunction;
    const bool bWasPowerSave = PreviousFunction == FUNCTION_POWER_SAVE;

    gCurrentFunction = Function;

    if (bWasPowerSave && Function != FUNCTION_POWER_SAVE) {
        BK4819_Conditional_RX_TurnOn_and_GPIO6_Enable();
        gRxIdleMode = false;
        UI_DisplayStatus();
    }

    switch (Function) {
        case FUNCTION_FOREGROUND:
            FUNCTION_Foreground(PreviousFunction);
            return;

        case FUNCTION_POWER_SAVE:
            FUNCTION_PowerSave();
            return;

        case FUNCTION_TRANSMIT:
            FUNCTION_Transmit();
            break;

        case FUNCTION_MONITOR:
            gMonitor = true;
            break;

        case FUNCTION_INCOMING:
        case FUNCTION_RECEIVE:
        case FUNCTION_BAND_SCOPE:
        default:
            break;
    }

    gBatterySaveCountdown_10ms = battery_save_count_10ms;
    gSchedulePowerSave = false;

#if defined(ENABLE_FMRADIO)
    gFM_RestoreCountdown_10ms = 0;
#endif
}
