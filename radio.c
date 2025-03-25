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
#include "driver/bk4819-regs.h"
#include "driver/bk4819.h"
#include <stdint.h>
#include "app/mdc1200.h"
#include <string.h>
#include "am_fix.h"
#include "app/dtmf.h"

#ifdef ENABLE_FMRADIO

#include "app/fm.h"

#endif

#include "audio.h"
#include "bsp/dp32g030/gpio.h"
#include "dcs.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "frequencies.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/menu.h"

#ifdef ENABLE_MESSENGER
#include "app/messenger.h"
#endif
#ifdef ENABLE_DOPPLER

#include "app/doppler.h"

#endif
VFO_Info_t *gTxVfo;
VFO_Info_t *gRxVfo;
VFO_Info_t *gCurrentVfo;
DCS_CodeType_t gCurrentCodeType;
VfoState_t VfoState[2];
const char gModulationStr[MODULATION_UKNOWN][4] = {
        [MODULATION_FM]="FM",
        [MODULATION_AM]="AM",
        [MODULATION_USB]="USB",

#ifdef ENABLE_BYP_RAW_DEMODULATORS
        [MODULATION_BYP]="BYP",
        [MODULATION_RAW]="RAW"
#endif
};

//发送传输结束
void RADIO_SendEndOfTransmission(void) {
    BK4819_PlayRoger();
    DTMF_SendEndOfTransmission();
    // send the CTCSS/DCS tail tone - allows the receivers to mute the usual FM squelch tail/crash
    if (gEeprom.TAIL_TONE_ELIMINATION)
        RADIO_SendCssTail();
    RADIO_SetupRegisters(false);
}

// 检查通道是否有效
// 参数:
// channel - 要检查的通道号
// checkScanList - 是否检查扫描列表
// scanList - 要检查的扫描列表编号
bool RADIO_CheckValidChannel(uint16_t channel, bool checkScanList, uint8_t scanList) {
    // return true if the channel appears valid
    if (!IS_MR_CHANNEL(channel))
        return false;

    const ChannelAttributes_t att = gMR_ChannelAttributes[channel];

    if (att.band > BAND7_470MHz)
        return false;

    if (!checkScanList || scanList > 1)
        return true;

    if (scanList ? !att.scanlist2 : !att.scanlist1)
        return false;

    const uint8_t PriorityCh1 = gEeprom.SCANLIST_PRIORITY_CH1[scanList];
    const uint8_t PriorityCh2 = gEeprom.SCANLIST_PRIORITY_CH2[scanList];

    return PriorityCh1 != channel && PriorityCh2 != channel;
}

uint8_t RADIO_FindNextChannel(uint8_t Channel, int8_t Direction, bool bCheckScanList, uint8_t VFO) {
    unsigned int i;

    for (i = 0; IS_MR_CHANNEL(i); i++) {
        if (Channel == 0xFF)
            Channel = MR_CHANNEL_LAST;
        else if (!IS_MR_CHANNEL(Channel))
            Channel = MR_CHANNEL_FIRST;

        if (RADIO_CheckValidChannel(Channel, bCheckScanList, VFO))
            return Channel;

        Channel += Direction;
    }

    return 0xFF;
}

void RADIO_InitInfo(VFO_Info_t *pInfo, const uint8_t ChannelSave, const uint32_t Frequency) {
    memset(pInfo, 0, sizeof(*pInfo));

    pInfo->Band = FREQUENCY_GetBand(Frequency);
    pInfo->SCANLIST1_PARTICIPATION = false;
    pInfo->SCANLIST2_PARTICIPATION = false;
    pInfo->STEP_SETTING = STEP_12_5kHz;
    pInfo->StepFrequency = gStepFrequencyTable[pInfo->STEP_SETTING];
    pInfo->CHANNEL_SAVE = ChannelSave;
    pInfo->FrequencyReverse = false;
    pInfo->OUTPUT_POWER = OUTPUT_POWER_LOW;
    pInfo->freq_config_RX.Frequency = Frequency;
    pInfo->freq_config_TX.Frequency = Frequency;
    pInfo->pRX = &pInfo->freq_config_RX;
    pInfo->pTX = &pInfo->freq_config_TX;
    pInfo->Compander = 0;  // off

    if (ChannelSave == (FREQ_CHANNEL_FIRST + BAND2_108MHz))
        pInfo->Modulation = MODULATION_AM;
    else
        pInfo->Modulation = MODULATION_FM;

    RADIO_ConfigureSquelchAndOutputPower(pInfo);
}

void RADIO_ConfigureChannel(const unsigned int VFO, const unsigned int configure) {
    VFO_Info_t *pVfo = &gEeprom.VfoInfo[VFO];

//    if (!gSetting_350EN) {
//        if (gEeprom.FreqChannel[VFO] == FREQ_CHANNEL_FIRST + BAND5_350MHz)
//            gEeprom.FreqChannel[VFO] = FREQ_CHANNEL_FIRST + BAND6_400MHz;
//
//        if (gEeprom.ScreenChannel[VFO] == FREQ_CHANNEL_FIRST + BAND5_350MHz)
//            gEeprom.ScreenChannel[VFO] = FREQ_CHANNEL_FIRST + BAND6_400MHz;
//    }

    uint8_t channel = gEeprom.ScreenChannel[VFO];

    if (IS_VALID_CHANNEL(channel)) {
#ifdef ENABLE_NOAA
        if (IS_NOAA_CHANNEL(channel))
        {
            RADIO_InitInfo(pVfo, gEeprom.ScreenChannel[VFO], NoaaFrequencyTable[channel - NOAA_CHANNEL_FIRST]);

            if (gEeprom.CROSS_BAND_RX_TX == CROSS_BAND_OFF)
                return;

            gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_OFF;

            gUpdateStatus = true;
            return;
        }
#endif

        if (IS_MR_CHANNEL(channel)) {
            channel = RADIO_FindNextChannel(channel, RADIO_CHANNEL_UP, false, VFO);
            if (channel == 0xFF) {
                channel = gEeprom.FreqChannel[VFO];
                gEeprom.ScreenChannel[VFO] = gEeprom.FreqChannel[VFO];
            } else {
                gEeprom.ScreenChannel[VFO] = channel;
                gEeprom.MrChannel[VFO] = channel;
            }
        }
    } else
        channel = FREQ_CHANNEL_LAST - 1;

    ChannelAttributes_t att = gMR_ChannelAttributes[channel];
    if (att.__val == 0xFF) { // invalid/unused channel
        if (IS_MR_CHANNEL(channel)) {
            channel = gEeprom.FreqChannel[VFO];
            gEeprom.ScreenChannel[VFO] = channel;
        }

        uint8_t bandIdx = channel - FREQ_CHANNEL_FIRST;
        RADIO_InitInfo(pVfo, channel, frequencyBandTable[bandIdx].lower);
        return;
    }

    uint8_t band = att.band;
    if (band > BAND7_470MHz) {
        band = BAND6_400MHz;
    }

    bool bParticipation1;
    bool bParticipation2;
    if (IS_MR_CHANNEL(channel)) {
        bParticipation1 = att.scanlist1;
        bParticipation2 = att.scanlist2;
    } else {
        band = channel - FREQ_CHANNEL_FIRST;
        bParticipation1 = true;
        bParticipation2 = true;
    }

    pVfo->Band = band;
    pVfo->SCANLIST1_PARTICIPATION = bParticipation1;
    pVfo->SCANLIST2_PARTICIPATION = bParticipation2;
    pVfo->CHANNEL_SAVE = channel;

    uint16_t base;
    if (IS_MR_CHANNEL(channel))
        base = channel * 16;
    else
        base = 0x0C80 + ((channel - FREQ_CHANNEL_FIRST) * 32) + (VFO * 16);

    if (configure == VFO_CONFIGURE_RELOAD || IS_FREQ_CHANNEL(channel)) {
        uint8_t tmp;
        uint8_t data[8];

        // ***************

        EEPROM_ReadBuffer(base + 8, data, sizeof(data));

        tmp = data[3] & 0x0F;
        if (tmp > TX_OFFSET_FREQUENCY_DIRECTION_SUB)
            tmp = 0;
        pVfo->TX_OFFSET_FREQUENCY_DIRECTION = tmp;
        tmp = data[3] >> 4;
        if (tmp >= MODULATION_UKNOWN)
            tmp = MODULATION_FM;
        pVfo->Modulation = tmp;

        tmp = data[6];
        if (tmp >= STEP_N_ELEM)
            tmp = STEP_12_5kHz;
        pVfo->STEP_SETTING = tmp;
        pVfo->StepFrequency = gStepFrequencyTable[tmp];

        tmp = data[7];
        if (tmp > (ARRAY_SIZE(gSubMenu_SCRAMBLER) - 1))
            tmp = 0;
        pVfo->SCRAMBLING_TYPE = tmp;

        pVfo->freq_config_RX.CodeType = (data[2] >> 0) & 0x0F;
        pVfo->freq_config_TX.CodeType = (data[2] >> 4) & 0x0F;

        tmp = data[0];
        switch (pVfo->freq_config_RX.CodeType) {
            default:
            case CODE_TYPE_OFF:
                pVfo->freq_config_RX.CodeType = CODE_TYPE_OFF;
                tmp = 0;
                break;

            case CODE_TYPE_CONTINUOUS_TONE:
                if (tmp > (50 - 1))
                    tmp = 0;
                break;

            case CODE_TYPE_DIGITAL:
            case CODE_TYPE_REVERSE_DIGITAL:
                if (tmp > (104 - 1))
                    tmp = 0;
                break;
        }
        pVfo->freq_config_RX.Code = tmp;

        tmp = data[1];
        switch (pVfo->freq_config_TX.CodeType) {
            default:
            case CODE_TYPE_OFF:
                pVfo->freq_config_TX.CodeType = CODE_TYPE_OFF;
                tmp = 0;
                break;

            case CODE_TYPE_CONTINUOUS_TONE:
                if (tmp > (50 - 1))
                    tmp = 0;
                break;

            case CODE_TYPE_DIGITAL:
            case CODE_TYPE_REVERSE_DIGITAL:
                if (tmp > (104 - 1))
                    tmp = 0;
                break;
        }
        pVfo->freq_config_TX.Code = tmp;

        if (data[4] == 0xFF) {
            pVfo->FrequencyReverse = false;
            pVfo->CHANNEL_BANDWIDTH = BK4819_FILTER_BW_WIDE;
            pVfo->OUTPUT_POWER = OUTPUT_POWER_LOW;
            pVfo->BUSY_CHANNEL_LOCK = false;
        } else {
            const uint8_t d4 = data[4];
            pVfo->FrequencyReverse = !!((d4 >> 0) & 1u);
            pVfo->CHANNEL_BANDWIDTH = !!((d4 >> 1) & 1u);
            pVfo->OUTPUT_POWER = ((d4 >> 2) & 3u);
            pVfo->BUSY_CHANNEL_LOCK = !!((d4 >> 4) & 1u);
        }

        if (data[5] == 0xFF) {
#ifdef ENABLE_DTMF_CALLING
            pVfo->DTMF_DECODING_ENABLE = false;
#endif
            pVfo->DTMF_PTT_ID_TX_MODE = PTT_ID_OFF;
        } else {
#ifdef ENABLE_DTMF_CALLING
            pVfo->DTMF_DECODING_ENABLE = ((data[5] >> 0) & 1u) ? true : false;
#endif
            uint8_t pttId = ((data[5] >> 1) & 7u);
            pVfo->DTMF_PTT_ID_TX_MODE = pttId < ARRAY_SIZE(gSubMenu_PTT_ID) ? pttId : PTT_ID_OFF;
        }

        // ***************

        struct {
            uint32_t Frequency;
            uint32_t Offset;
        } __attribute__((packed)) info;
        EEPROM_ReadBuffer(base, &info, sizeof(info));
        if (info.Frequency == 0xFFFFFFFF)
            pVfo->freq_config_RX.Frequency = frequencyBandTable[band].lower;
        else
            pVfo->freq_config_RX.Frequency = info.Frequency;

        if (info.Offset >= _1GHz_in_KHz)
            info.Offset = _1GHz_in_KHz / 100;
        pVfo->TX_OFFSET_FREQUENCY = info.Offset;

        // ***************
    }

    uint32_t frequency = pVfo->freq_config_RX.Frequency;

    // fix previously set incorrect band
    band = FREQUENCY_GetBand(frequency);

    if (frequency < frequencyBandTable[band].lower)
        frequency = frequencyBandTable[band].lower;
    else if (frequency > frequencyBandTable[band].upper)
        frequency = frequencyBandTable[band].upper;
    else if (channel >= FREQ_CHANNEL_FIRST)
        frequency = FREQUENCY_RoundToStep(frequency, pVfo->StepFrequency);

    pVfo->freq_config_RX.Frequency = frequency;

    if (frequency >= frequencyBandTable[BAND2_108MHz].upper && frequency < frequencyBandTable[BAND2_108MHz].upper)
        pVfo->TX_OFFSET_FREQUENCY_DIRECTION = TX_OFFSET_FREQUENCY_DIRECTION_OFF;
    else if (!IS_MR_CHANNEL(channel))
        pVfo->TX_OFFSET_FREQUENCY = FREQUENCY_RoundToStep(pVfo->TX_OFFSET_FREQUENCY, pVfo->StepFrequency);

    RADIO_ApplyOffset(pVfo);

    if (IS_MR_CHANNEL(channel)) {    // 16 bytes allocated to the channel name but only 10 used, the rest are 0's


        SETTINGS_FetchChannelName(pVfo->Name, channel);
    }

    if (!pVfo->FrequencyReverse) {
        pVfo->pRX = &pVfo->freq_config_RX;
        pVfo->pTX = &pVfo->freq_config_TX;
    } else {
        pVfo->pRX = &pVfo->freq_config_TX;
        pVfo->pTX = &pVfo->freq_config_RX;
    }

//    if (!gSetting_350EN)
//    {
//        FREQ_Config_t *pConfig = pVfo->pRX;
//        if (pConfig->Frequency >= 35000000 && pConfig->Frequency < 40000000)
//            pConfig->Frequency = 43300000;
//    }


//    else{
//
//         FREQ_Config_t *pConfig =  gEeprom.VfoInfo[1].pRX;
//         unsigned int code_type = pConfig->CodeType;
//
//
//        pVfo->freq_config_RX.CodeType=  code_type;
//        pVfo->freq_config_TX.CodeType=   code_type;
//
//    }

    pVfo->Compander = att.compander;

    RADIO_ConfigureSquelchAndOutputPower(pVfo);
}

void RADIO_ConfigureSquelchAndOutputPower(VFO_Info_t *pInfo) {


    // *******************************
    // squelch

    FREQUENCY_Band_t Band = FREQUENCY_GetBand(pInfo->pRX->Frequency);
    uint16_t Base = (Band < BAND4_174MHz) ? 0x1E60 : 0x1E00;

    if (gEeprom.SQUELCH_LEVEL == 0) {
        // squelch == 0 (off)
        pInfo->SquelchOpenRSSIThresh = 0;      // 0 ~ 255
        pInfo->SquelchOpenNoiseThresh = 127;   // 127 ~ 0
        pInfo->SquelchOpenGlitchThresh = 255;  // 255 ~ 0


        pInfo->SquelchCloseRSSIThresh = 0;     // 0 ~ 255
        pInfo->SquelchCloseNoiseThresh = 127;   // 127 ~ 0
        pInfo->SquelchCloseGlitchThresh = 255;   // 255 ~ 0

    } else {
        // squelch >= 1
        Base += gEeprom.SQUELCH_LEVEL;  // my eeprom squelch-1
        // VHF   UHF
        EEPROM_ReadBuffer(Base + 0x00, &pInfo->SquelchOpenRSSIThresh, 1);  //  50    10
        EEPROM_ReadBuffer(Base + 0x10, &pInfo->SquelchCloseRSSIThresh, 1);  //  40     5

        EEPROM_ReadBuffer(Base + 0x20, &pInfo->SquelchOpenNoiseThresh, 1);  //  65    90
        EEPROM_ReadBuffer(Base + 0x30, &pInfo->SquelchCloseNoiseThresh, 1);  //  70   100

        EEPROM_ReadBuffer(Base + 0x40, &pInfo->SquelchCloseGlitchThresh, 1);  //  90    90
        EEPROM_ReadBuffer(Base + 0x50, &pInfo->SquelchOpenGlitchThresh, 1);  // 100   100



        //当静噪门打开时----------------------------------------------
        // 噪声门限值
        uint16_t noise_open = pInfo->SquelchOpenNoiseThresh;
        // RSSI门限值
        uint16_t rssi_open = pInfo->SquelchOpenRSSIThresh;
        // 脉冲干扰门限值
        uint16_t glitch_open = pInfo->SquelchOpenGlitchThresh;

        //当静噪门关闭时-----------------------------------------------
        // 噪声门限值
        uint16_t noise_close = pInfo->SquelchCloseNoiseThresh;
        // RSSI门限值
        uint16_t rssi_close = pInfo->SquelchCloseRSSIThresh;
        // 脉冲干扰门限值
        uint16_t glitch_close = pInfo->SquelchCloseGlitchThresh;

        // 如果启用了更敏感的静噪功能
#if ENABLE_SQUELCH_MORE_SENSITIVE
        //类型   范   围  灵敏
        //RSSI  0 ~ 255  变大
        //Noise 127 ~ 0  变小
        //Glitc 255 ~ 0  变小

        // 注意 '噪声' 和 '脉冲' 值与 'RSSI' 比例相反，这里提高4/1倍,在RX带宽固定时使用（无弱信号自动调整）
        // 在这里找到最佳的一般设置是按实际情况
        rssi_open = (rssi_open * 1) / 4;
        noise_open = (noise_open * 4) / 1;
        glitch_open = (glitch_open * 4) / 1;

        rssi_close = (rssi_close * 1) / 4;
        noise_close = (noise_close * 4) / 1;
        glitch_close = (glitch_close * 4) / 1;
//       确保 '关闭' 阈值小于 '开启' 阈值，保持一定滞后水平
        if (rssi_close + 4 >= rssi_open)
            if (rssi_open >= 4)
                rssi_close = rssi_open - 4;
            else
                rssi_open = rssi_close + 4;
        if (noise_close - 2 <= noise_open)
            if (noise_open <= 125)
                noise_close = noise_open + 2;
            else
                noise_open = noise_close - 2;
        if (glitch_close - 2 <= glitch_open)
            if (glitch_open <= 253)
                glitch_close = glitch_open + 2;
            else
                glitch_open = glitch_close - 2;

 //确保 '关闭' 阈值小于 '开启' 阈值，保持一定滞后水平
//        rssi_open   = (rssi_open   * 1) / 2;
//        noise_open  = (noise_open  * 2) / 1;
//        glitch_open = (glitch_open * 2) / 1;
//
//        rssi_close   = (rssi_open   *  11) / 15;
//        noise_close  = (noise_open  * 15) / 11;
//        glitch_close = (glitch_open * 15) / 11;
//        if (rssi_open < 8)
//            rssi_open = 8;
//        if (rssi_close > (rssi_open - 8))
//            rssi_close = rssi_open - 8;
//        if (noise_open > (127 - 4))
//            noise_open = 127 - 4;
//        if (noise_close < (noise_open + 4))
//            noise_close = noise_open + 4;
//        if (glitch_open > (255 - 8))
//            glitch_open = 255 - 8;
//        if (glitch_close < (glitch_open + 8))
//            glitch_close = glitch_open + 8;


#endif
        //RSSI  范围限制0 ~ 255
        //Noise 范围限制127 ~ 0
        //Glitc 范围限制255 ~ 0
        pInfo->SquelchOpenRSSIThresh = (rssi_open > 255) ? 255 : rssi_open;
        pInfo->SquelchCloseRSSIThresh = (rssi_close > 255) ? 255 : rssi_close;

        pInfo->SquelchOpenGlitchThresh = (glitch_open > 255) ? 255 : glitch_open;
        pInfo->SquelchCloseGlitchThresh = (glitch_close > 255) ? 255 : glitch_close;

        pInfo->SquelchOpenNoiseThresh = (noise_open > 127) ? 127 : noise_open;
        pInfo->SquelchCloseNoiseThresh = (noise_close > 127) ? 127 : noise_close;
    }

    // *******************************
    // output power

    Band = FREQUENCY_GetBand(pInfo->pTX->Frequency);
    uint8_t Txp[3];
    EEPROM_ReadBuffer(0x1ED0 + (Band * 16) + (pInfo->OUTPUT_POWER * 3), Txp, 3);


#ifdef ENABLE_REDUCE_LOW_MID_TX_POWER
    // make low and mid even lower
    if (pInfo->OUTPUT_POWER == OUTPUT_POWER_LOW) {
        Txp[0] /= 5;
        Txp[1] /= 5;
        Txp[2] /= 5;
    }
    else if (pInfo->OUTPUT_POWER == OUTPUT_POWER_MID){
        Txp[0] /= 3;
        Txp[1] /= 3;
        Txp[2] /= 3;
    }
#endif

    pInfo->TXP_CalculatedSetting = FREQUENCY_CalculateOutputPower(
            Txp[0],
            Txp[1],
            Txp[2],
            frequencyBandTable[Band].lower,
            (frequencyBandTable[Band].lower + frequencyBandTable[Band].upper) / 2,
            frequencyBandTable[Band].upper,
            pInfo->pTX->Frequency);

    // *******************************
}

void RADIO_ApplyOffset(VFO_Info_t *pInfo) {
    uint32_t Frequency = pInfo->freq_config_RX.Frequency;

    switch (pInfo->TX_OFFSET_FREQUENCY_DIRECTION) {
        case TX_OFFSET_FREQUENCY_DIRECTION_OFF:
            break;
        case TX_OFFSET_FREQUENCY_DIRECTION_ADD:
            Frequency += pInfo->TX_OFFSET_FREQUENCY;
            break;
        case TX_OFFSET_FREQUENCY_DIRECTION_SUB:
            Frequency -= pInfo->TX_OFFSET_FREQUENCY;
            break;
    }


    pInfo->freq_config_TX.Frequency = Frequency;
}

static void RADIO_SelectCurrentVfo(void) {
    // 如果跨波段功能激活，并且双波段监视（DW）不是当前VFO（gCurrentVfo），那么gCurrentVfo是gTxVfo（gTxVfo/TX_VFO只能由用户更改）
    // 否则，它被设置为gRxVfo，在RADIO_SelectVfos中gRxVfo被设置为gTxVfo
    // 因此，除非在接收传输时双波段监视更改了它（这只能在跨波段功能关闭时发生），否则gCurrentVfo最终等于gTxVfo
    // 注意：它只在特定情况下被调用，所以可能不是最新的
    gCurrentVfo = (gEeprom.CROSS_BAND_RX_TX == CROSS_BAND_OFF || gEeprom.DUAL_WATCH != DUAL_WATCH_OFF) ? gRxVfo
                                                                                                       : gTxVfo;
}

void RADIO_SelectVfos(void) {
    // if crossband without DW is used then RX_VFO is the opposite to the TX_VFO
    gEeprom.RX_VFO = (gEeprom.CROSS_BAND_RX_TX == CROSS_BAND_OFF || gEeprom.DUAL_WATCH != DUAL_WATCH_OFF)
                     ? gEeprom.TX_VFO : !gEeprom.TX_VFO;

    gTxVfo = &gEeprom.VfoInfo[gEeprom.TX_VFO];
    gRxVfo = &gEeprom.VfoInfo[gEeprom.RX_VFO];

    RADIO_SelectCurrentVfo();
}

void RADIO_SetupRegisters(bool switchToForeground) {
    BK4819_FilterBandwidth_t Bandwidth = gRxVfo->CHANNEL_BANDWIDTH;
    uint16_t InterruptMask;
    uint32_t Frequency;
#if ENABLE_CHINESE_FULL == 4 && !defined(ENABLE_ENGLISH)
    uint8_t read_tmp[2];
#endif
    AUDIO_AudioPathOff();

    gEnableSpeaker = false;

    BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);

    switch (Bandwidth) {
        default:
            Bandwidth = BK4819_FILTER_BW_WIDE;
            [[fallthrough]];
        case BK4819_FILTER_BW_WIDE:
        case BK4819_FILTER_BW_NARROW:
#ifdef ENABLE_AM_FIX
            //				BK4819_SetFilterBandwidth(Bandwidth, gRxVfo->Modulation == MODULATION_AM && gSetting_AM_fix);
            BK4819_SetFilterBandwidth(Bandwidth, true);
#else
            BK4819_SetFilterBandwidth(Bandwidth, false);
#endif
            break;
    }

    BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);

    BK4819_SetupPowerAmplifier(0, 0);

    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);

    while (1) {
        const uint16_t Status = BK4819_ReadRegister(BK4819_REG_0C);
        if ((Status & 1u) == 0) // INTERRUPT REQUEST
            break;

        BK4819_WriteRegister(BK4819_REG_02, 0);
        SYSTEM_DelayMs(1);
    }
    BK4819_WriteRegister(BK4819_REG_3F, 0);

    //设置灵敏度
    // mic gain 0.5dB/step 0 to 31
    // BK4819_WriteRegister(BK4819_REG_7D, 0xE940 | (gEeprom.MIC_SENSITIVITY_TUNING & 0x1f));

#ifdef ENABLE_NOAA
    if (!IS_NOAA_CHANNEL(gRxVfo->CHANNEL_SAVE) || !gIsNoaaMode)
            Frequency = gRxVfo->pRX->Frequency;
        else
            Frequency = NoaaFrequencyTable[gNoaaChannel];
#else
    Frequency = gRxVfo->pRX->Frequency;
#endif
    BK4819_SetFrequency(Frequency);

    BK4819_SetupSquelch(
            gRxVfo->SquelchOpenRSSIThresh, gRxVfo->SquelchCloseRSSIThresh,
            gRxVfo->SquelchOpenNoiseThresh, gRxVfo->SquelchCloseNoiseThresh,
            gRxVfo->SquelchCloseGlitchThresh, gRxVfo->SquelchOpenGlitchThresh);

    BK4819_PickRXFilterPathBasedOnFrequency(Frequency);

    // what does this in do ?
    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);

    // AF RX Gain and DAC
    //BK4819_WriteRegister(BK4819_REG_48, 0xB3A8);  // 1011 00 111010 1000
    BK4819_WriteRegister(BK4819_REG_48,
                         (11u << 12) |     // ??? .. 0 ~ 15, doesn't seem to make any difference
                         (0u << 10) |     // AF Rx Gain-1
                         (gEeprom.VOLUME_GAIN << 4) |     // AF Rx Gain-2
                         (gEeprom.DAC_GAIN << 0));     // AF DAC Gain (after Gain-1 and Gain-2)


    InterruptMask = BK4819_REG_3F_SQUELCH_FOUND | BK4819_REG_3F_SQUELCH_LOST;

#ifdef ENABLE_NOAA
    if (!IS_NOAA_CHANNEL(gRxVfo->CHANNEL_SAVE))
#endif
    {
        if (gRxVfo->Modulation == MODULATION_FM) {    // FM
            uint8_t CodeType = gRxVfo->pRX->CodeType;
            uint8_t Code = gRxVfo->pRX->Code;

            switch (CodeType) {
                default:
                case CODE_TYPE_OFF:
                    BK4819_SetCTCSSFrequency(670);

                    //#ifndef ENABLE_CTCSS_TAIL_PHASE_SHIFT
                    BK4819_SetTailDetection(550);        // QS's 55Hz tone method
                    //#else
                    //	BK4819_SetTailDetection(670);       // 67Hz
                    //#endif

                    InterruptMask = BK4819_REG_3F_CxCSS_TAIL | BK4819_REG_3F_SQUELCH_FOUND | BK4819_REG_3F_SQUELCH_LOST;
                    break;

                case CODE_TYPE_CONTINUOUS_TONE:

#if ENABLE_CHINESE_FULL == 0 || defined(ENABLE_ENGLISH)
                    BK4819_SetCTCSSFrequency(CTCSS_Options[Code]);
#else
                    EEPROM_ReadBuffer(0x02C00 + (Code) * 2, read_tmp, 2);
                    uint16_t CTCSS_Options_read = read_tmp[0] | (read_tmp[1] << 8);
                    BK4819_SetCTCSSFrequency(CTCSS_Options_read);

#endif
                    //#ifndef ENABLE_CTCSS_TAIL_PHASE_SHIFT
                    BK4819_SetTailDetection(550);        // QS's 55Hz tone method
                    //#else
                    //	BK4819_SetTailDetection(CTCSS_Options[Code]);
                    //#endif

                    InterruptMask = 0
                                    | BK4819_REG_3F_CxCSS_TAIL
                                    | BK4819_REG_3F_CTCSS_FOUND
                                    | BK4819_REG_3F_CTCSS_LOST
                                    | BK4819_REG_3F_SQUELCH_FOUND
                                    | BK4819_REG_3F_SQUELCH_LOST;

                    break;

                case CODE_TYPE_DIGITAL:
                case CODE_TYPE_REVERSE_DIGITAL:
                    BK4819_SetCDCSSCodeWord(DCS_GetGolayCodeWord(CodeType, Code));
                    InterruptMask = 0
                                    | BK4819_REG_3F_CxCSS_TAIL
                                    | BK4819_REG_3F_CDCSS_FOUND
                                    | BK4819_REG_3F_CDCSS_LOST
                                    | BK4819_REG_3F_SQUELCH_FOUND
                                    | BK4819_REG_3F_SQUELCH_LOST;
                    break;
            }
//修改
//            if (gRxVfo->SCRAMBLING_TYPE > 0 && gSetting_ScrambleEnable)
//                BK4819_EnableScramble(gRxVfo->SCRAMBLING_TYPE - 1);
//            else
 //            BK4819_DisableScramble();
        }
    }
#ifdef ENABLE_NOAA
    else
        {
            BK4819_SetCTCSSFrequency(2625);
            InterruptMask = 0
                | BK4819_REG_3F_CTCSS_FOUND
                | BK4819_REG_3F_CTCSS_LOST
                | BK4819_REG_3F_SQUELCH_FOUND
                | BK4819_REG_3F_SQUELCH_LOST;
        }
#endif

#ifdef ENABLE_VOX
    if (gEeprom.VOX_SWITCH && gCurrentVfo->Modulation == MODULATION_FM
        #ifdef ENABLE_NOAA
        && !IS_NOAA_CHANNEL(gCurrentVfo->CHANNEL_SAVE)
        #endif
        #ifdef ENABLE_FMRADIO
        && !gFmRadioMode
#endif
            ) {
        BK4819_EnableVox(gEeprom.VOX1_THRESHOLD, gEeprom.VOX0_THRESHOLD);
        InterruptMask |= BK4819_REG_3F_VOX_FOUND | BK4819_REG_3F_VOX_LOST;
    } else
#endif
    {
        BK4819_DisableVox();
    }
    // RX expander
    BK4819_SetCompander((gRxVfo->Modulation == MODULATION_FM && gRxVfo->Compander >= 2) ? gRxVfo->Compander : 0);

    BK4819_EnableDTMF();
    InterruptMask |= BK4819_REG_3F_DTMF_5TONE_FOUND;
    RADIO_SetupAGC(gRxVfo->Modulation == MODULATION_AM, false);
    // enable/disable BK4819 selected interrupts

    //OK?
#if defined(ENABLE_MESSENGER) || defined(ENABLE_MDC1200)
    enable_msg_rx(true);
#endif
#ifdef ENABLE_MESSENGER
    InterruptMask |= BK4819_REG_3F_FSK_RX_SYNC | BK4819_REG_3F_FSK_RX_FINISHED | BK4819_REG_3F_FSK_FIFO_ALMOST_FULL | BK4819_REG_3F_FSK_TX_FINISHED;

#elif defined(ENABLE_MDC1200)
    InterruptMask |= BK4819_REG_3F_FSK_RX_SYNC | BK4819_REG_3F_FSK_RX_FINISHED | BK4819_REG_3F_FSK_FIFO_ALMOST_FULL;
#endif
    BK4819_WriteRegister(BK4819_REG_3F, InterruptMask);

    FUNCTION_Init();

    if (switchToForeground)
        FUNCTION_Select(FUNCTION_FOREGROUND);
}

#ifdef ENABLE_NOAA
void RADIO_ConfigureNOAA(void)
    {
        uint8_t ChanAB;

        gUpdateStatus = true;

        if (gEeprom.NOAA_AUTO_SCAN)
        {
            if (gEeprom.DUAL_WATCH != DUAL_WATCH_OFF)
            {
                if (!IS_NOAA_CHANNEL(gEeprom.ScreenChannel[0]))
                {
                    if (!IS_NOAA_CHANNEL(gEeprom.ScreenChannel[1]))
                    {
                        gIsNoaaMode = false;
                        return;
                    }
                    ChanAB = 1;
                }
                else
                    ChanAB = 0;

                if (!gIsNoaaMode)
                    gNoaaChannel = gEeprom.VfoInfo[ChanAB].CHANNEL_SAVE - NOAA_CHANNEL_FIRST;

                gIsNoaaMode = true;
                return;
            }

            if (IS_NOAA_CHANNEL(gRxVfo->CHANNEL_SAVE))
            {
                gIsNoaaMode          = true;
                gNoaaChannel         = gRxVfo->CHANNEL_SAVE - NOAA_CHANNEL_FIRST;
                gNOAA_Countdown_10ms = NOAA_countdown_2_10ms;
                gScheduleNOAA        = false;
            }
            else
                gIsNoaaMode = false;
        }
        else
            gIsNoaaMode = false;
    }
#endif


//发送信号
void RADIO_SetTxParameters(void) {
    // 如果启用了中文全功能版本4，并且没有定义英文版本，则定义一个临时数组
#if ENABLE_CHINESE_FULL == 4 && !defined(ENABLE_ENGLISH)
    uint8_t read_tmp[2]; // 定义一个长度为2的uint8_t类型的数组，用于从EEPROM读取数据
#endif

    // 获取当前VFO的频道带宽
    BK4819_FilterBandwidth_t Bandwidth = gCurrentVfo->CHANNEL_BANDWIDTH;

    // 关闭音频路径，即不从音频路径输出声音
    AUDIO_AudioPathOff();

    // 关闭扬声器，即不通过扬声器输出声音
    gEnableSpeaker = false;

    // 关闭接收使能的GPIO输出，即不启用接收功能
    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, false);

    // 根据带宽选择操作
    switch (Bandwidth) {
        // 如果带宽未定义，则默认使用宽带
        default:
            Bandwidth = BK4819_FILTER_BW_WIDE;
            [[fallthrough]]; // 继续执行下一个case

        case BK4819_FILTER_BW_WIDE:
        case BK4819_FILTER_BW_NARROW:
            // 如果启用了AM修复，则设置滤波器带宽，否则普通设置
#ifdef ENABLE_AM_FIX
            // BK4819_SetFilterBandwidth(Bandwidth, gCurrentVfo->Modulation == MODULATION_AM && gSetting_AM_fix);
            BK4819_SetFilterBandwidth(Bandwidth, true); // 假设这里总是设置为true
#else
            BK4819_SetFilterBandwidth(Bandwidth, false);
#endif
            break;

    }


    // 如果DTMF在发射时被启用，它会改变发射音频的过滤器设置
    // 因此，确保在需要之前禁用DTMF
    BK4819_DisableDTMF();

    // 设置发射压缩器
    // 如果接收调制是FM，并且接收压缩器设置为1或3，则设置发射压缩器为接收压缩器的值；否则设置为0
    BK4819_SetCompander((gRxVfo->Modulation == MODULATION_FM && (gRxVfo->Compander == 1 || gRxVfo->Compander >= 3))
                        ? gRxVfo->Compander : 0);

    // 设置发射频率
    BK4819_SetFrequency(gCurrentVfo->pTX->Frequency);
    // 设置无线电设备的发射频率

    // 根据发射频率选择接收滤波器路径
    BK4819_PickRXFilterPathBasedOnFrequency(gCurrentVfo->pTX->Frequency);
    // 根据发射频率选择最合适的接收滤波器路径

    // 准备发射
    BK4819_PrepareTransmit();
    // 准备无线电设备进行发射

    // 系统延迟10毫秒
    SYSTEM_DelayMs(10);
    // 等待10毫秒，以便给硬件足够的时间稳定

    // 设置功率放大器
    BK4819_SetupPowerAmplifier(gCurrentVfo->TXP_CalculatedSetting, gCurrentVfo->pTX->Frequency);
    // 根据当前VFO的发射功率计算设置功率放大器

    // 系统延迟10毫秒
    SYSTEM_DelayMs(10); // 等待10毫秒，以便给硬件足够的时间稳定

    // 打开PA使能的GPIO输出
    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, true);
    // 打开功率放大器的使能引脚，准备发射

    // 系统延迟5毫秒
    SYSTEM_DelayMs(5); // 等待5毫秒，以便给硬件足够的时间稳定

    switch (gCurrentVfo->pTX->CodeType) {
        // 如果代码类型未定义，则退出子音频单元（Sub Audio Unit）
        default:
            BK4819_ExitSubAu(); // 退出子音频单元，即不使用任何子音频编码
            break;

            // 如果代码类型是连续音调，设置CTCSS频率
        case CODE_TYPE_CONTINUOUS_TONE:
            // 如果启用了中文全功能版本0或定义了英文版本，则直接从CTCSS选项数组中设置频率
#if ENABLE_CHINESE_FULL == 0 || defined(ENABLE_ENGLISH)
            BK4819_SetCTCSSFrequency(CTCSS_Options[gCurrentVfo->pTX->Code]);
#else
            // 否则，从EEPROM中读取CTCSS频率配置
            EEPROM_ReadBuffer(0x02C00 + (gCurrentVfo->pTX->Code) * 2, read_tmp, 2);
            uint16_t CTCSS_Options_read = read_tmp[0] | (read_tmp[1] << 8);
            BK4819_SetCTCSSFrequency(CTCSS_Options_read);
#endif
            break;

            // 如果代码类型是数字或反向数字，设置CDCSS码字
        case CODE_TYPE_DIGITAL:
        case CODE_TYPE_REVERSE_DIGITAL:
            BK4819_SetCDCSSCodeWord(DCS_GetGolayCodeWord(gCurrentVfo->pTX->CodeType, gCurrentVfo->pTX->Code));
            break;
    }
}

void RADIO_SetModulation(ModulationMode_t modulation) {
    BK4819_AF_Type_t mod;
    switch (modulation) {
        default:
        case MODULATION_FM:
            mod = BK4819_AF_FM;
            break;
        case MODULATION_AM:
            mod = BK4819_AF_AM;
            break;
        case MODULATION_USB:
            mod = BK4819_AF_BASEBAND2;
            break;

#ifdef ENABLE_BYP_RAW_DEMODULATORS
            case MODULATION_BYP:
            mod = BK4819_AF_UNKNOWN3;
            break;
        case MODULATION_RAW:
            mod = BK4819_AF_BASEBAND1;
            break;
#endif
    }

    BK4819_SetAF(mod);


    BK4819_SetRegValue(afDacGainRegSpec, 0xF);
    BK4819_WriteRegister(BK4819_REG_3D, modulation == MODULATION_USB ? 0 : 0x2AAB);
    BK4819_SetRegValue(afcDisableRegSpec, modulation != MODULATION_FM);

    RADIO_SetupAGC(modulation == MODULATION_AM, false);
}

void RADIO_SetupAGC(bool listeningAM, bool disable) {
    static uint8_t lastSettings;
    uint8_t newSettings = (listeningAM << 1) | (disable << 1);
    if (lastSettings == newSettings)
        return;
    lastSettings = newSettings;


    if (!listeningAM) { // if not actively listening AM we don't need any AM specific regulation
        BK4819_SetAGC(!disable);
        BK4819_InitAGC(false);
    } else {
#ifdef ENABLE_AM_FIX
        if (gSetting_AM_fix) { // if AM fix active lock AGC so AM-fix can do it's job
            BK4819_SetAGC(0);
            AM_fix_enable(!disable);
        } else
#endif
        {
            BK4819_SetAGC(!disable);
            BK4819_InitAGC(true);
        }
    }
}

void RADIO_SetVfoState(VfoState_t State) {
    if (State == VFO_STATE_NORMAL) {
        VfoState[0] = VFO_STATE_NORMAL;
        VfoState[1] = VFO_STATE_NORMAL;
    } else if (State == VFO_STATE_VOLTAGE_HIGH) {
        VfoState[0] = VFO_STATE_VOLTAGE_HIGH;
        VfoState[1] = VFO_STATE_TX_DISABLE;
    } else {
        // 1of11
        const unsigned int vfo = (gEeprom.CROSS_BAND_RX_TX == CROSS_BAND_OFF) ? gEeprom.RX_VFO : gEeprom.TX_VFO;
        VfoState[vfo] = State;
    }

    gVFOStateResumeCountdown_500ms = (State == VFO_STATE_NORMAL) ? 0 : vfo_state_resume_countdown_500ms;
    gUpdateDisplay = true;
}

void RADIO_PrepareTX(void) {
    VfoState_t State = VFO_STATE_NORMAL;  // default to OK to TX

    if (gEeprom.DUAL_WATCH != DUAL_WATCH_OFF) {    // dual-RX is enabled

        gDualWatchCountdown_10ms = dual_watch_count_after_tx_10ms;
        gScheduleDualWatch = false;

        if (!gRxVfoIsActive) {    // use the current RX vfo
            gEeprom.RX_VFO = gEeprom.TX_VFO;
            gRxVfo = gTxVfo;
            gRxVfoIsActive = true;
        }

        // let the user see that DW is not active
        gDualWatchActive = false;
        gUpdateStatus = true;
    }

    RADIO_SelectCurrentVfo();
    if (TX_freq_check(gCurrentVfo->pTX->Frequency) != 0
#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
        && gAlarmState != ALARM_STATE_SITE_ALARM
#endif
            ) {
        // TX frequency not allowed
        State = VFO_STATE_TX_DISABLE;
    } else if (SerialConfigInProgress()) {
        // TX is disabled or config upload/download in progress
        State = VFO_STATE_TX_DISABLE;
    } else if (gCurrentVfo->BUSY_CHANNEL_LOCK && gCurrentFunction == FUNCTION_RECEIVE) {
        // busy RX'ing a station
        State = VFO_STATE_BUSY;
    } else if (gBatteryDisplayLevel == 0) {
        // charge your battery !git co
        State = VFO_STATE_BAT_LOW;
    } else if (gBatteryDisplayLevel > 6) {
        // over voltage .. this is being a pain
        State = VFO_STATE_VOLTAGE_HIGH;
    }
#ifndef ENABLE_TX_WHEN_AM
    else if (gCurrentVfo->Modulation != MODULATION_FM) {
        // not allowed to TX if in AM mode
        State = VFO_STATE_TX_DISABLE;
    }
#endif

    if (State != VFO_STATE_NORMAL) {
        // TX not allowed
        RADIO_SetVfoState(State);

#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
        gAlarmState = ALARM_STATE_OFF;
#endif

#ifdef ENABLE_DTMF_CALLING
        gDTMF_ReplyState = DTMF_REPLY_NONE;
#endif

#ifdef    ENABLE_WARNING
        AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
#endif
        return;
    }

    // TX is allowed

#ifdef ENABLE_DTMF_CALLING
    if (gDTMF_ReplyState == DTMF_REPLY_ANI) {
        gDTMF_IsTx = gDTMF_CallMode == DTMF_CALL_MODE_DTMF;

        if (gDTMF_IsTx) {
            gDTMF_CallState = DTMF_CALL_STATE_NONE;
            gDTMF_TxStopCountdown_500ms = DTMF_txstop_countdown_500ms;
        } else {
            gDTMF_CallState = DTMF_CALL_STATE_CALL_OUT;
        }
    }
#endif

    FUNCTION_Select(FUNCTION_TRANSMIT);

    gTxTimerCountdown_500ms = 0;            // no timeout

#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
    if (gAlarmState == ALARM_STATE_OFF)
#endif
    {
        if (gEeprom.TX_TIMEOUT_TIMER == 0)
            gTxTimerCountdown_500ms = 60;   // 30 sec
        else if (gEeprom.TX_TIMEOUT_TIMER < (ARRAY_SIZE(gSubMenu_TOT) - 1))
            gTxTimerCountdown_500ms = 120 * gEeprom.TX_TIMEOUT_TIMER;  // minutes
        else
            gTxTimerCountdown_500ms = 120 * 15;  // 15 minutes
    }
    gTxTimeoutReached = false;

    gRTTECountdown_10ms = 0;

#ifdef ENABLE_DTMF_CALLING
    gDTMF_ReplyState = DTMF_REPLY_NONE;
#endif
}

// 发送CSS（连续相位信号）尾音
void RADIO_SendCssTail(void) {
    switch (gCurrentVfo->pTX->CodeType) {
        case CODE_TYPE_DIGITAL:
        case CODE_TYPE_REVERSE_DIGITAL:
            BK4819_PlayCDCSSTail();// 播放数字CSS尾音
            break;
        default:
            BK4819_PlayCTCSSTail();// 播放CTCSS尾音
            break;
    }
    SYSTEM_DelayMs(200);
}

void RADIO_PrepareCssTX(void) {
    // 准备无线电设备进行传输，设置必要的传输参数
    RADIO_PrepareTX();

    // 程序暂停200毫秒，确保传输前的准备工作完成或满足特定的时序要求
    SYSTEM_DelayMs(200);

    // 检查EEPROM中的配置，是否启用了尾音消除功能
    if (gEeprom.TAIL_TONE_ELIMINATION)
        // 如果启用了尾音消除，发送CSS尾音信号以减少传输结束时的尾音
        RADIO_SendCssTail();
    // 设置无线电设备寄存器以准备CSS传输，参数true表示传输前的设置
    RADIO_SetupRegisters(true);
}
