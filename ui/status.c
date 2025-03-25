#ifdef ENABLE_MESSENGER
#include "app/messenger.h"
#endif
#ifdef ENABLE_DOPPLER
#include "app/doppler.h"
#endif

#include <string.h>

#include "app/chFrScanner.h"

#ifdef ENABLE_FMRADIO
#include "app/fm.h"
#endif

#include "app/scanner.h"
#include "bitmaps.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"
#include "ui/battery.h"
#include "ui/helper.h"
#include "ui/ui.h"
#include "ui/status.h"

//状态栏显示函数
void UI_DisplayStatus() {
    gUpdateStatus = false;
    memset(gStatusLine, 0, sizeof(gStatusLine));

    uint8_t *line = gStatusLine;
    unsigned int x = 0;

    if (gCurrentFunction == FUNCTION_TRANSMIT&&gScreenToDisplay!=DISPLAY_MAIN) {
        //画发射
        memcpy(line + x+1, BITMAP_TX, sizeof(BITMAP_TX));
    }
    else if (FUNCTION_IsRx()) {
        //画监听
        memcpy(line + x+1, BITMAP_RX, sizeof(BITMAP_RX));
    }
    else if (gCurrentFunction == FUNCTION_POWER_SAVE) {
        //画省电
        memcpy(line + x+4, BITMAP_POWERSAVE, sizeof(BITMAP_POWERSAVE));
    }
    x += 8;
    unsigned int x1 = x;

#ifdef ENABLE_NOAA
    if (gIsNoaaMode) { // NOASS SCAN indicator
        memcpy(line + x, BITMAP_NOAA, sizeof(BITMAP_NOAA));
        x1 = x + sizeof(BITMAP_NOAA);
    }
    x += sizeof(BITMAP_NOAA);
#endif
#ifdef ENABLE_MESSENGER
    if (hasNewMessage > 0) { // New Message indicator
        if (hasNewMessage == 1)
            memcpy(line + x+1, BITMAP_NEWMSG, sizeof(BITMAP_NEWMSG));
        x1 = x + sizeof(BITMAP_NEWMSG);
    }
    x += sizeof(BITMAP_NEWMSG);
#endif
#ifdef ENABLE_DTMF_CALLING
    if (gSetting_KILLED) {
        memset(line + x, 0xFF, 10);
        x1 = x + 10;
    }
    else
#endif
#ifdef ENABLE_FMRADIO
        //画 FM 图标
    if (gFmRadioMode) {
        memcpy(line + x+1, BITMAP_FM, sizeof(BITMAP_FM));
        x1 = x+ 1 + sizeof(BITMAP_FM);
    }
    else
#endif
    { // SCAN indicator
        if (gScanStateDir != SCAN_OFF || SCANNER_IsScanning()) {
            if (IS_MR_CHANNEL(gNextMrChannel) && !SCANNER_IsScanning()) { // channel mode

                char *s = "";
                switch (gEeprom.SCAN_LIST_DEFAULT) {
                    case 0:
                        s = "1";
                        break;
                    case 1:
                        s = "2";
                        break;
                    case 2:
                        s = "*";
                        break;
                }
                UI_PrintStringSmallBuffer(s, line + x + 1);

            } else {
                // 扫描显示图片
                memcpy(line + x, BITMAP_SCAN, sizeof(BITMAP_SCAN));
            }
            x1 = x + 10;

        }
    }
    x += 10;  // font character width

#ifdef ENABLE_VOICE
    // VOICE indicator
    if (gEeprom.VOICE_PROMPT != VOICE_PROMPT_OFF){
        memcpy(line + x, BITMAP_VoicePrompt, sizeof(BITMAP_VoicePrompt));
        x1 = x + sizeof(BITMAP_VoicePrompt);
    }
    x += sizeof(BITMAP_VoicePrompt);
#endif

    if (!SCANNER_IsScanning()) {
        uint8_t dw = (gEeprom.DUAL_WATCH != DUAL_WATCH_OFF) + (gEeprom.CROSS_BAND_RX_TX != CROSS_BAND_OFF) * 2;
        if (dw == 1 || dw == 3) { // DWR - dual watch + respond
            if (gDualWatchActive)
                memcpy(line + x + (dw == 1 ? 0 : 2), BITMAP_TDR1, sizeof(BITMAP_TDR1) - (dw == 1 ? 0 : 5));
            else
                memcpy(line + x + 3 + 1, BITMAP_TDR2, sizeof(BITMAP_TDR2));
        } else if (dw == 2) { // XB - crossband
            memcpy(line + x + 2, BITMAP_XB, sizeof(BITMAP_XB));
        }
    }
    x += sizeof(BITMAP_TDR1) + 1;

#ifdef ENABLE_VOX
    // VOX indicator
    if (gEeprom.VOX_SWITCH) {
        //修改
        memcpy(line + x+5, BITMAP_VOX, sizeof(BITMAP_VOX));
        x1 = x + sizeof(BITMAP_VOX) + 1;
    }
    x += sizeof(BITMAP_VOX) + 1;
#endif

    x = MAX(x1, 61u);

    // KEY-LOCK indicator
    if (gEeprom.KEY_LOCK) {
        memcpy(line + x, BITMAP_KeyLock, sizeof(BITMAP_KeyLock));
        x += sizeof(BITMAP_KeyLock);
        x1 = x;
    }
    else if (gWasFKeyPressed)
    {
        //画F
        memcpy(line + x, BITMAP_F_Key, sizeof(BITMAP_F_Key));
        x += sizeof(BITMAP_F_Key);
        x1 = x;
    }

    {    // 电池电压或百分比
        char s[8] = "";
        unsigned int x2 = LCD_WIDTH - sizeof(BITMAP_BatteryLevel1) - 0;

        if (gChargingWithTypeC)
            x2 -= sizeof(BITMAP_USB_C);  // the radio is on charge

//        switch (gSetting_battery_text) {
//            default:
//            case 0:
//                break;
//
//            case 1:	{	// voltage
//                const uint16_t voltage = (gBatteryVoltageAverage <= 999) ? gBatteryVoltageAverage : 999; // limit to 9.99V
//                sprintf(s, "%u.%02uV", voltage / 100, voltage % 100);
//                break;
//            }
//
//            case 2:		// percentage
       //修改
      //  sprintf(s, "%u%%", BATTERY_VoltsToPercent(gBatteryVoltageAverage));
        sprintf(s, "%u%%",  currentPercent);

//                break;
//        }

        unsigned int space_needed = (7 * strlen(s));
        if (x2 >= (x1 + space_needed))
            UI_PrintStringSmallBuffer(s, line + x2 - space_needed);
    }

    // move to right side of the screen
    x = LCD_WIDTH - sizeof(BITMAP_BatteryLevel1) - sizeof(BITMAP_USB_C);

    // USB-C charge indicator
    if (gChargingWithTypeC)
        memcpy(line + x, BITMAP_USB_C, sizeof(BITMAP_USB_C));
    x += sizeof(BITMAP_USB_C);

    // BATTERY LEVEL indicator
    UI_DrawBattery(line + x, gBatteryDisplayLevel, gLowBatteryBlink);

    // **************

    ST7565_BlitStatusLine();
}