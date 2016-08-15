/*
 * YFSrdio.c
 *
 * This file is part of the navdtools source code.
 *
 * (C) Copyright 2016 Timothy D. Walker and others.
 *
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of the GNU General Public License (GPL) version 2
 * which accompanies this distribution (LICENSE file), and is also available at
 * http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * Contributors:
 *     Timothy D. Walker
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "XPLM/XPLMDataAccess.h"
#include "XPLM/XPLMUtilities.h"

#include "YFSmain.h"
#include "YFSrdio.h"
#include "YFSspad.h"

#define FB76_BARO_MIN (26.966629f) // rotary at 0.0f
#define FB76_BARO_MAX (32.873302f) // rotary at 1.0f
/*
 * check if a given barometric pressure is 29.92 InHg (or 1013 hPa, rounded)
 *                                           10_12.97                10_13.51 */
#define BPRESS_IS_STD(bpress) ((((bpress) >= 29.913) && ((bpress) <= 29.929)))

static void yfs_rad1_pageupdt    (yfms_context *yfms);
static void yfs_rad2_pageupdt    (yfms_context *yfms);
static void yfs_lsk_callback_rad1(yfms_context *yfms, int key[2], intptr_t refcon);

void yfs_rdio_pageopen(yfms_context *yfms)
{
    if (yfms == NULL)
    {
        return; // no error
    }
    int page = yfms->mwindow.current_page;
    if (page == PAGE_MENU)
    {
        return; // special page, FMGC disabled
    }
    if (page == PAGE_RAD1)
    {
        yfms->mwindow.current_page = PAGE_RAD2;
        //fixme callbacks
    }
    else
    {
        if (yfms->xpl.atyp == YFS_ATYP_QPAC)
        {
            XPLMCommandOnce(yfms->xpl.qpac.VHF1Capt);
            XPLMCommandOnce(yfms->xpl.qpac.VHF2Co);
        }
        yfms->mwindow.current_page = PAGE_RAD1;
        yfms->lsks[0][0].cback = yfms->lsks[1][0].cback = (YFS_LSK_f)&yfs_lsk_callback_rad1;
        yfms->lsks[0][1].cback = yfms->lsks[1][1].cback = (YFS_LSK_f)&yfs_lsk_callback_rad1;
        yfms->lsks[0][3].cback = yfms->lsks[1][3].cback = (YFS_LSK_f)&yfs_lsk_callback_rad1;
        yfms->lsks[0][4].cback = yfms->lsks[1][4].cback = (YFS_LSK_f)&yfs_lsk_callback_rad1;
        yfms->lsks[0][5].cback = yfms->lsks[1][5].cback = (YFS_LSK_f)&yfs_lsk_callback_rad1;
    }
    yfs_rdio_pageupdt(yfms);
}

void yfs_rdio_pageupdt(yfms_context *yfms)
{
    if (!yfms)
    {
        return; // no error
    }
    if (yfms->mwindow.current_page == PAGE_RAD1)
    {
        yfs_rad1_pageupdt(yfms); return;
    }
    if (yfms->mwindow.current_page == PAGE_RAD2)
    {
        yfs_rad2_pageupdt(yfms); return;
    }
}

static void yfs_rad1_pageupdt(yfms_context *yfms)
{
    /* reset lines before drawing */
    for (int i = 0; i < YFS_DISPLAY_NUMR - 1; i++)
    {
        yfs_main_rline(yfms, i, -1);
    }

    /* row buffer */
    char buf[YFS_DISPLAY_NUMC + 1];

    /* line 0: main header (white, centered) */
    snprintf(buf, sizeof(buf), "%*s", (YFS_DISPLAY_NUMC + 10) / 2, "COM RADIO");
    yfs_printf_lft(yfms,  0, 0, COLR_IDX_WHITE,    buf);
    yfs_printf_rgt(yfms,  0, 0, COLR_IDX_WHITE, "<-> ");

    /* line 1: headers (white) */
    yfs_printf_lft(yfms,  1, 0, COLR_IDX_WHITE, "COM 1");
    yfs_printf_rgt(yfms,  1, 0, COLR_IDX_WHITE, "COM 2");

    /* line 2: active frequencies (green) */
    snprintf(buf, sizeof(buf), "%03d.%03d", XPLMGetDatai(yfms->xpl.com1_frequency_Mhz),         XPLMGetDatai(yfms->xpl.com1_frequency_khz));
    yfs_printf_lft(yfms,  2, 0, COLR_IDX_GREEN, buf);
    snprintf(buf, sizeof(buf), "%03d.%03d", XPLMGetDatai(yfms->xpl.com2_frequency_Mhz),         XPLMGetDatai(yfms->xpl.com2_frequency_khz));
    yfs_printf_rgt(yfms,  2, 0, COLR_IDX_GREEN, buf);

    /* line 3: headers (white) */
    yfs_printf_lft(yfms,  3, 0, COLR_IDX_WHITE, "STANDBY");
    yfs_printf_rgt(yfms,  3, 0, COLR_IDX_WHITE, "STANDBY");

    /* line 4: standby frequencies (blue) */
    snprintf(buf, sizeof(buf), "%03d.%03d", XPLMGetDatai(yfms->xpl.com1_standby_frequency_Mhz), XPLMGetDatai(yfms->xpl.com1_standby_frequency_khz));
    yfs_printf_lft(yfms,  4, 0, COLR_IDX_BLUE,  buf);
    snprintf(buf, sizeof(buf), "%03d.%03d", XPLMGetDatai(yfms->xpl.com2_standby_frequency_Mhz), XPLMGetDatai(yfms->xpl.com2_standby_frequency_khz));
    yfs_printf_rgt(yfms,  4, 0, COLR_IDX_BLUE,  buf);

    /* line 7: headers (white) */
    snprintf(buf, sizeof(buf), "%*s", (YFS_DISPLAY_NUMC + 5) / 2, "XPDR");
    yfs_printf_lft(yfms,  7, 0, COLR_IDX_WHITE,    buf);
    yfs_printf_lft(yfms,  7, 0, COLR_IDX_WHITE, "CODE");
    yfs_printf_rgt(yfms,  7, 0, COLR_IDX_WHITE, "MODE");

    /* line 8: XPDR information */
    if (yfms->xpl.atyp == YFS_ATYP_IXEG)
    {
        if ((int)roundf(XPLMGetDataf(yfms->xpl.ixeg.xpdr_mode_act)) == 0)
        {
            yfs_printf_lft(yfms,  8, 0, COLR_IDX_WHITE,  "----");
            yfs_printf_rgt(yfms,  8, 0, COLR_IDX_WHITE,   "OFF");
        }
        else switch ((int)roundf(XPLMGetDataf(yfms->xpl.ixeg.xpdr_stby_act)))
        {
            case 0:
                snprintf(buf, sizeof(buf), "%04d", XPLMGetDatai(yfms->xpl.transponder_code));
                yfs_printf_lft(yfms,  8, 0, COLR_IDX_BLUE,      buf);
                yfs_printf_rgt(yfms,  8, 0, COLR_IDX_BLUE,   "STBY");
                break;
            case 2:
                snprintf(buf, sizeof(buf), "%04d", XPLMGetDatai(yfms->xpl.transponder_code));
                yfs_printf_lft(yfms,  8, 0, COLR_IDX_BLUE,      buf);
                yfs_printf_rgt(yfms,  8, 0, COLR_IDX_BLUE,  "TA/RA");
                break;
            default:
                snprintf(buf, sizeof(buf), "%04d", XPLMGetDatai(yfms->xpl.transponder_code));
                yfs_printf_lft(yfms,  8, 0, COLR_IDX_BLUE,      buf);
                yfs_printf_rgt(yfms,  8, 0, COLR_IDX_BLUE,   "AUTO");
                break;
        }
    }
    else if (yfms->xpl.atyp == YFS_ATYP_QPAC)
    {
        switch (XPLMGetDatai(yfms->xpl.qpac.XPDRPower))
        {
            case 0:
                snprintf(buf, sizeof(buf), "%04d", XPLMGetDatai(yfms->xpl.transponder_code));
                yfs_printf_lft(yfms,  8, 0, COLR_IDX_BLUE,      buf);
                yfs_printf_rgt(yfms,  8, 0, COLR_IDX_BLUE,   "STBY");
                break;
            case 2:
                snprintf(buf, sizeof(buf), "%04d", XPLMGetDatai(yfms->xpl.transponder_code));
                yfs_printf_lft(yfms,  8, 0, COLR_IDX_BLUE,      buf);
                yfs_printf_rgt(yfms,  8, 0, COLR_IDX_BLUE,  "TA/RA");
                break;
            default:
                snprintf(buf, sizeof(buf), "%04d", XPLMGetDatai(yfms->xpl.transponder_code));
                yfs_printf_lft(yfms,  8, 0, COLR_IDX_BLUE,      buf);
                yfs_printf_rgt(yfms,  8, 0, COLR_IDX_BLUE,   "AUTO");
                break;
        }
    }
    else switch (XPLMGetDatai(yfms->xpl.transponder_mode))
    {
        case 0:
            yfs_printf_lft(yfms,  8, 0, COLR_IDX_WHITE,  "----");
            yfs_printf_rgt(yfms,  8, 0, COLR_IDX_WHITE,   "OFF");
            break;
        case 1:
            snprintf(buf, sizeof(buf), "%04d", XPLMGetDatai(yfms->xpl.transponder_code));
            yfs_printf_lft(yfms,  8, 0, COLR_IDX_BLUE,      buf);
            yfs_printf_rgt(yfms,  8, 0, COLR_IDX_BLUE,   "STBY");
            break;
        default:
            snprintf(buf, sizeof(buf), "%04d", XPLMGetDatai(yfms->xpl.transponder_code));
            yfs_printf_lft(yfms,  8, 0, COLR_IDX_BLUE,      buf);
            yfs_printf_rgt(yfms,  8, 0, COLR_IDX_BLUE,  "TA/RA");
            break;
    }

    /* line 9: headers (white) */
    snprintf(buf, sizeof(buf), "%*s", (YFS_DISPLAY_NUMC + 10) / 2, "ALTIMETER");
    yfs_printf_lft(yfms,  9, 0, COLR_IDX_WHITE,    buf);
    yfs_printf_lft(yfms,  9, 0, COLR_IDX_WHITE,  "BARO");
    yfs_printf_rgt(yfms,  9, 0, COLR_IDX_WHITE,  "UNIT");

    /* line 10: barometric altimeter information */
    float alt, alt_inhg = XPLMGetDataf(yfms->xpl.barometer_setting_in_hg_pilot);
    switch (yfms->ndt.alt.unit)
    {
        case 1: // hectoPascals
            alt = roundf(alt_inhg * 33.86389f);
            snprintf(buf, sizeof(buf), "%04.0f", alt);
            yfs_printf_rgt(yfms, 10, 0, COLR_IDX_BLUE,  "hPa");
            break;
        default: // inches of mercury
            alt = roundf(alt_inhg * 100.0f);
            snprintf(buf, sizeof(buf), "%05.2f", alt / 100.0f);
            yfs_printf_rgt(yfms, 10, 0, COLR_IDX_BLUE, "InHg");
            break;
    }
    if ((yfms->xpl.atyp == YFS_ATYP_QPAC && XPLMGetDatai(yfms->xpl.qpac.BaroStdCapt)) ||
        (yfms->xpl.atyp == YFS_ATYP_Q380 && XPLMGetDatai(yfms->xpl.q380.BaroStdCapt)) ||
        (yfms->xpl.atyp == YFS_ATYP_Q350 && XPLMGetDatai(yfms->xpl.q350.pressLeftButton)))
    {
        yfs_printf_lft(yfms, 10, 0, COLR_IDX_BLUE, "STD");
    }
    else
    {
        yfs_printf_lft(yfms, 10, 0, COLR_IDX_BLUE, buf);
    }

    /* line 12: switches (white) */
    if (yfms->xpl.atyp == YFS_ATYP_QPAC)
    {
        if (!XPLMGetDatai(yfms->xpl.qpac.BaroStdCapt))
        {
            yfs_printf_lft(yfms, 12, 0, COLR_IDX_WHITE, "<ALT STD");
            yfms->lsks[0][5].cback = (YFS_LSK_f)&yfs_lsk_callback_rad1;
        }
        else
        {
            yfms->lsks[0][5].cback = (YFS_LSK_f)NULL;
        }
    }
    else if (yfms->xpl.atyp == YFS_ATYP_Q350)
    {
        if (!XPLMGetDatai(yfms->xpl.q350.pressLeftButton))
        {
            yfs_printf_lft(yfms, 12, 0, COLR_IDX_WHITE, "<ALT STD");
            yfms->lsks[0][5].cback = (YFS_LSK_f)&yfs_lsk_callback_rad1;
        }
        else
        {
            yfms->lsks[0][5].cback = (YFS_LSK_f)NULL;
        }
    }
    else if (yfms->xpl.atyp == YFS_ATYP_Q380)
    {
        if (!XPLMGetDatai(yfms->xpl.q380.BaroStdCapt))
        {
            yfs_printf_lft(yfms, 12, 0, COLR_IDX_WHITE, "<ALT STD");
            yfms->lsks[0][5].cback = (YFS_LSK_f)&yfs_lsk_callback_rad1;
        }
        else
        {
            yfms->lsks[0][5].cback = (YFS_LSK_f)NULL;
        }
    }
    else if (BPRESS_IS_STD(alt_inhg) == 0)
    {
        yfs_printf_lft(yfms, 12, 0, COLR_IDX_WHITE, "<ALT STD");
        yfms->lsks[0][5].cback = (YFS_LSK_f)&yfs_lsk_callback_rad1;
    }
    else
    {
        yfms->lsks[0][5].cback = (YFS_LSK_f)NULL;
    }
    if (XPLMGetDatai(yfms->xpl.transponder_mode) >= 2)
    {
        switch (XPLMGetDatai(yfms->xpl.transponder_id))
        {
            case 1:
                yfs_printf_rgt(yfms, 12, 0, COLR_IDX_GREEN,  "IDENT");
                break;
            default:
                yfs_printf_rgt(yfms, 12, 0, COLR_IDX_WHITE, "XPDR ID>");
                break;
        }
    }
}

static void yfs_rad2_pageupdt(yfms_context *yfms)
{
    //fixme
}

static void yfs_lsk_callback_rad1(yfms_context *yfms, int key[2], intptr_t refcon)
{
    if (yfms == NULL || yfms->mwindow.current_page != PAGE_RAD1)
    {
        return; // callback not applicable to current page
    }
    if (key[1] == 0 && yfms->xpl.atyp == YFS_ATYP_QPAC)
    {
        switch (key[0])
        {
            case 0:
                XPLMCommandOnce(yfms->xpl.qpac.VHF1Capt);
                XPLMCommandOnce(yfms->xpl.qpac.RMPSwapCapt);
                break;
            default:
                XPLMCommandOnce(yfms->xpl.qpac.VHF2Co);
                XPLMCommandOnce(yfms->xpl.qpac.RMPSwapCo);
                break;
        }
        yfs_rad1_pageupdt(yfms); return;
    }
    if (key[0] == 0 && key[1] == 0)
    {
        XPLMCommandOnce(yfms->xpl.com1_standy_flip); yfs_rad1_pageupdt(yfms); return;
    }
    if (key[0] == 1 && key[1] == 0)
    {
        XPLMCommandOnce(yfms->xpl.com2_standy_flip); yfs_rad1_pageupdt(yfms); return;
    }
    if (key[1] == 1)
    {
        int mhz, khz, len, dot; char buf[YFS_DISPLAY_NUMC + 1]; yfs_spad_copy2(yfms, buf);
        if (buf[0] == 0)
        {
            if (key[0] == 0)
            {
                snprintf(buf, sizeof(buf), "%.7s", yfms->mwindow.screen.text[4]);
                buf[7] = 0; yfs_spad_reset(yfms, buf, -1); return; // c1 sb to scratchpad
            }
            if (key[0] == 1)
            {
                snprintf(buf, sizeof(buf), "%.7s", yfms->mwindow.screen.text[4] + YFS_DISPLAY_NUMC - 7);
                buf[7] = 0; yfs_spad_reset(yfms, buf, -1); return; // c2 sb to scratchpad
            }
        }
        if ((len = strnlen(buf, 8)) != 7 && len != 6 && len != 5 && len !=4)
        {
            yfs_spad_reset(yfms, "FORMAT ERROR", -1); return;
        }
        if (sscanf(buf, "%d.%d", &mhz, &khz) == 2)
        {
            dot = 1;
        }
        else if (sscanf(buf, "%3d%d", &mhz, &khz) == 2)
        {
            dot = 0;
        }
        else
        {
            yfs_spad_reset(yfms, "FORMAT ERROR", -1); return;
        }
        if (mhz < 118 || mhz > 136)
        {
            yfs_spad_reset(yfms, "FORMAT ERROR", -1); return;
        }
        if (6 + dot - len >= 1)
        {
            if (6 + dot - len >= 2)
            {
                khz *= 100; // 122.8 -> 122.800, 1228 -> 122800
            }
            else
            {
                khz *= 10; // 123.45 -> 123.450, 12345 -> 123450
            }
        }
        // 8.33 kHz spacing
        {
            // 0.6 is the minimum to adjust .520 (62.4) -> .525 (63.0)
            // we then round to the nearest mod5 khz value for display
            double ticks_833 = floor(((double)khz / 8.33) + 0.6);
            khz = (((int)floor(ticks_833 * 8.34 + 2.5)) / 5 * 5);
        }
        if (key[0] == 0)
        {
            if (yfms->xpl.atyp == YFS_ATYP_QPAC)
            {
                XPLMCommandOnce(yfms->xpl.qpac.VHF1Capt);
                khz = khz / 25 * 25; // QPAC radios only have 25 kHz precision
                int qpac_mhz = XPLMGetDatai(yfms->xpl.com1_standby_frequency_Mhz);
                int qpac_khz = XPLMGetDatai(yfms->xpl.com1_standby_frequency_khz);
                if (qpac_mhz < mhz)
                {
                    for (int i = 0; i < mhz - qpac_mhz; i++)
                    {
                        XPLMCommandOnce(yfms->xpl.qpac.RMP1FreqUpLrg);
                    }
                }
                if (qpac_mhz > mhz)
                {
                    for (int i = 0; i < qpac_mhz - mhz; i++)
                    {
                        XPLMCommandOnce(yfms->xpl.qpac.RMP1FreqDownLrg);
                    }
                }
                if (qpac_khz < khz)
                {
                    for (int i = 0; i < khz - qpac_khz; i += 25)
                    {
                        XPLMCommandOnce(yfms->xpl.qpac.RMP1FreqUpSml);
                    }
                }
                if (qpac_khz > khz)
                {
                    for (int i = 0; i < qpac_khz - khz; i += 25)
                    {
                        XPLMCommandOnce(yfms->xpl.qpac.RMP1FreqDownSml);
                    }
                }
                yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
            }
            XPLMSetDatai(yfms->xpl.com1_standby_frequency_Mhz, mhz);
            XPLMSetDatai(yfms->xpl.com1_standby_frequency_khz, khz);
            yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
        }
        if (key[0] == 1)
        {
            if (yfms->xpl.atyp == YFS_ATYP_QPAC)
            {
                XPLMCommandOnce(yfms->xpl.qpac.VHF2Co);
                khz = khz / 25 * 25; // QPAC radios only have 25 kHz precision
                int qpac_mhz = XPLMGetDatai(yfms->xpl.com2_standby_frequency_Mhz);
                int qpac_khz = XPLMGetDatai(yfms->xpl.com2_standby_frequency_khz);
                if (qpac_mhz < mhz)
                {
                    for (int i = 0; i < mhz - qpac_mhz; i++)
                    {
                        XPLMCommandOnce(yfms->xpl.qpac.RMP2FreqUpLrg);
                    }
                }
                if (qpac_mhz > mhz)
                {
                    for (int i = 0; i < qpac_mhz - mhz; i++)
                    {
                        XPLMCommandOnce(yfms->xpl.qpac.RMP2FreqDownLrg);
                    }
                }
                if (qpac_khz < khz)
                {
                    for (int i = 0; i < khz - qpac_khz; i += 25)
                    {
                        XPLMCommandOnce(yfms->xpl.qpac.RMP2FreqUpSml);
                    }
                }
                if (qpac_khz > khz)
                {
                    for (int i = 0; i < qpac_khz - khz; i += 25)
                    {
                        XPLMCommandOnce(yfms->xpl.qpac.RMP2FreqDownSml);
                    }
                }
                yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
            }
            XPLMSetDatai(yfms->xpl.com2_standby_frequency_Mhz, mhz);
            XPLMSetDatai(yfms->xpl.com2_standby_frequency_khz, khz);
            yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
        }
    }
    if (key[0] == 0 && key[1] == 3)
    {
        if (XPLMGetDatai(yfms->xpl.transponder_mode) <= 0)
        {
            return; // transponder off
        }
        char buf[YFS_DISPLAY_NUMC + 1]; yfs_spad_copy2(yfms, buf);
        if (buf[0] == 0)
        {
            snprintf(buf, sizeof(buf), "%04d", XPLMGetDatai(yfms->xpl.transponder_code));
            yfs_spad_reset(yfms, buf, -1); return; // current code to scratchpad
        }
        if (strnlen(buf, 5) != 4)
        {
            yfs_spad_reset(yfms, "FORMAT ERROR", -1); return;
        }
        for (int i = 0; i < 4; i++)
        {
            if (buf[i] < '0' || buf[i] > '7')
            {
                yfs_spad_reset(yfms, "FORMAT ERROR", -1); return;
            }
        }
        XPLMSetDatai(yfms->xpl.transponder_code, atoi(buf));
        yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
    }
    if (key[0] == 1 && key[1] == 3)
    {
        char buf[YFS_DISPLAY_NUMC + 1]; yfs_spad_copy2(yfms, buf);
        if  (strnlen(buf, 1) && !yfms->mwindow.screen.spad_reset)
        {
            if (!strcmp(buf, "OFF"))
            {
                if (yfms->xpl.atyp == YFS_ATYP_IXEG)
                {
                    XPLMSetDataf(yfms->xpl.ixeg.xpdr_mode_act, 0.0f);
                    XPLMSetDataf(yfms->xpl.ixeg.xpdr_stby_act, 1.0f);
                    yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
                }
                XPLMSetDatai(yfms->xpl.transponder_mode, 0);
                yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
            }
            if (!strcmp(buf, "AUTO"))
            {
                if (yfms->xpl.atyp == YFS_ATYP_IXEG)
                {
                    XPLMSetDataf(yfms->xpl.ixeg.xpdr_mode_act, 2.0f);
                    XPLMSetDataf(yfms->xpl.ixeg.xpdr_stby_act, 1.0f);
                    yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
                }
                if (yfms->xpl.atyp == YFS_ATYP_QPAC)
                {
                    XPLMSetDatai(yfms->xpl.qpac.XPDRAltitude, 1);
                    XPLMSetDatai(yfms->xpl.qpac.XPDRPower,    1);
                    yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
                }
                sprintf(buf, "%s", "STBY"); // fall through
            }
            if (!strcmp(buf, "STBY"))
            {
                if (yfms->xpl.atyp == YFS_ATYP_IXEG)
                {
                    XPLMSetDataf(yfms->xpl.ixeg.xpdr_mode_act, 2.0f);
                    XPLMSetDataf(yfms->xpl.ixeg.xpdr_stby_act, 0.0f);
                    yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
                }
                if (yfms->xpl.atyp == YFS_ATYP_QPAC)
                {
                    XPLMSetDatai(yfms->xpl.qpac.XPDRPower,    0);
                    yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
                }
                if (yfms->xpl.atyp == YFS_ATYP_FB76)
                {
                    XPLMSetDataf(yfms->xpl.fb76.systemMode, 1.0f);
                    yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
                }
                if (yfms->xpl.atyp == YFS_ATYP_FB77)
                {
                    XPLMSetDatai(yfms->xpl.fb77.anim_85_switch, 0);
                    yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
                }
                XPLMSetDatai(yfms->xpl.transponder_mode, 1);
                yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
            }
            if (!strcmp(buf, "TA/RA"))
            {
                if (yfms->xpl.atyp == YFS_ATYP_IXEG)
                {
                    XPLMSetDataf(yfms->xpl.ixeg.xpdr_mode_act, 2.0f);
                    XPLMSetDataf(yfms->xpl.ixeg.xpdr_stby_act, 2.0f);
                    yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
                }
                if (yfms->xpl.atyp == YFS_ATYP_QPAC)
                {
                    XPLMSetDatai(yfms->xpl.qpac.XPDRAltitude, 1);
                    XPLMSetDatai(yfms->xpl.qpac.XPDRPower,    2);
                    yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
                }
                if (yfms->xpl.atyp == YFS_ATYP_FB76)
                {
                    XPLMSetDataf(yfms->xpl.fb76.systemMode, 5.0f);
                    yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
                }
                if (yfms->xpl.atyp == YFS_ATYP_FB77)
                {
                    XPLMSetDatai(yfms->xpl.fb77.anim_85_switch, 4);
                    yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
                }
                XPLMSetDatai(yfms->xpl.transponder_mode, 3);
                yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
            }
            yfs_spad_reset(yfms, "FORMAT ERROR", -1); return;
        }
        else if (yfms->xpl.atyp == YFS_ATYP_IXEG)
        {
            if ((int)roundf(XPLMGetDataf(yfms->xpl.ixeg.xpdr_mode_act)) == 0 ||
                (int)roundf(XPLMGetDataf(yfms->xpl.ixeg.xpdr_stby_act)) != 1)
            {
                XPLMSetDataf(yfms->xpl.ixeg.xpdr_mode_act, 2.0f);
                XPLMSetDataf(yfms->xpl.ixeg.xpdr_stby_act, 1.0f); // AUTO
            }
            else
            {
                XPLMSetDataf(yfms->xpl.ixeg.xpdr_mode_act, 2.0f);
                XPLMSetDataf(yfms->xpl.ixeg.xpdr_stby_act, 2.0f); // TA/RA
            }
            yfs_rad1_pageupdt(yfms); return;
        }
        else if (yfms->xpl.atyp == YFS_ATYP_QPAC)
        {
            if (XPLMGetDatai(yfms->xpl.qpac.XPDRPower) != 1)
            {
                XPLMSetDatai(yfms->xpl.qpac.XPDRAltitude, 1);
                XPLMSetDatai(yfms->xpl.qpac.XPDRPower,    1); // AUTO
            }
            else
            {
                XPLMSetDatai(yfms->xpl.qpac.XPDRAltitude, 1);
                XPLMSetDatai(yfms->xpl.qpac.XPDRPower,    2); // TA/RA
            }
            yfs_rad1_pageupdt(yfms); return;
        }
        else if (yfms->xpl.atyp == YFS_ATYP_FB76)
        {
            if ((int)roundf(XPLMGetDataf(yfms->xpl.fb76.systemMode)) != 1)
            {
                XPLMSetDataf(yfms->xpl.fb76.systemMode, 1.0f); // STBY
            }
            else
            {
                XPLMSetDataf(yfms->xpl.fb76.systemMode, 5.0f); // TA/RA
            }
            yfs_rad1_pageupdt(yfms); return;
        }
        else if (yfms->xpl.atyp == YFS_ATYP_FB77)
        {
            if (XPLMGetDatai(yfms->xpl.fb77.anim_85_switch) != 0)
            {
                XPLMSetDatai(yfms->xpl.fb77.anim_85_switch, 0); // STBY
            }
            else
            {
                XPLMSetDatai(yfms->xpl.fb77.anim_85_switch, 4); // TA/RA
            }
            yfs_rad1_pageupdt(yfms); return;
        }
        else if (XPLMGetDatai(yfms->xpl.transponder_mode) == 1)
        {
            XPLMSetDatai(yfms->xpl.transponder_mode, 3);
            yfs_rad1_pageupdt(yfms); return;
        }
        else
        {
            XPLMSetDatai(yfms->xpl.transponder_mode, 1);
            yfs_rad1_pageupdt(yfms); return;
        }
    }
    if (key[0] == 0 && key[1] == 4)
    {
        float alt; char buf[YFS_DISPLAY_NUMC + 1]; yfs_spad_copy2(yfms, buf);
        if (buf[0] == 0)
        {
            snprintf(buf, sizeof(buf), "%.5s", yfms->mwindow.screen.text[10]);
            buf[4 + !yfms->ndt.alt.unit] = 0; yfs_spad_reset(yfms, buf, -1);
            return; // current baro to scratchpad
        }
        if (strnlen(buf, 6) != 4 + !yfms->ndt.alt.unit)
        {
            yfs_spad_reset(yfms, "FORMAT ERROR", -1); return;
        }
        if (sscanf(buf, "%f", &alt) != 1)
        {
            yfs_spad_reset(yfms, "FORMAT ERROR", -1); return;
        }
        switch (yfms->ndt.alt.unit)
        {
            case 1: // STD: 1013.00 -> 1013.04 -> 2991.51 -> 2992.00 -> 29.92
                alt = alt + 0.04f;
                alt = roundf(alt / 0.3386389f) / 100.0f;
                break;
            default:
                alt = roundf(alt * 100.00000f) / 100.0f;
                break;
        }
        if (alt > 40.0f || alt < 0.0f)
        {
            yfs_spad_reset(yfms, "FORMAT ERROR", -1); return;
        }
        if (yfms->xpl.atyp == YFS_ATYP_Q350)
        {
            float offset        = roundf(100.0f * (alt - 29.92f));
            XPLMSetDatai(yfms->xpl.q350.pressLeftButton,       0);
            XPLMSetDatai(yfms->xpl.q350.pressRightButton,      0);
            XPLMSetDataf(yfms->xpl.q350.pressLeftRotary,  offset);
            XPLMSetDataf(yfms->xpl.q350.pressRightRotary, offset);
            yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
        }
        if (yfms->xpl.atyp == YFS_ATYP_FB76)
        {
            if (alt < FB76_BARO_MIN)     alt = FB76_BARO_MIN;
            if (alt > FB76_BARO_MAX)     alt = FB76_BARO_MAX;
            float baro_range = alt           - FB76_BARO_MIN;
            float full_range = FB76_BARO_MAX - FB76_BARO_MIN;
            float alt_rotary_value = baro_range / full_range;
            XPLMSetDataf(yfms->xpl.fb76.baroRotary_stby,  alt_rotary_value);
            XPLMSetDataf(yfms->xpl.fb76.baroRotary_left,  alt_rotary_value);
            XPLMSetDataf(yfms->xpl.fb76.baroRotary_right, alt_rotary_value);
            yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
        }
        if (yfms->xpl.atyp == YFS_ATYP_FB77)
        {
            float alt_inhg = XPLMGetDataf(yfms->xpl.barometer_setting_in_hg_pilot);
            float rot_posn = XPLMGetDataf(yfms->xpl.fb77.anim_25_rotery);
            int   rot_indx = (int)roundf(100.0f * rot_posn);
            if   (rot_indx != 50 && BPRESS_IS_STD(alt_inhg))
            {
                // altimeter standard but rotary not halfway
                // we're in "STD" mode, switch to manual mode
                XPLMSetDatai(yfms->xpl.fb77.anim_175_button, 1);
            }
            // very limited rng. (29.42 -> 30.42)
            float rot_newp = alt - 29.92f + 0.5f;
            if (rot_newp < 0.0f) rot_newp = 0.0f;
            if (rot_newp > 1.0f) rot_newp = 1.0f;
            XPLMSetDataf(yfms->xpl.fb77.anim_25_rotery, rot_newp);
            yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
        }
        if (yfms->xpl.atyp == YFS_ATYP_QPAC)
        {
            XPLMSetDatai(yfms->xpl.qpac.BaroStdCapt, 0);
            XPLMSetDatai(yfms->xpl.qpac.BaroStdFO,   0);
        }
        if (yfms->xpl.atyp == YFS_ATYP_Q380)
        {
            XPLMSetDatai(yfms->xpl.q380.BaroStdCapt, 0);
            XPLMSetDatai(yfms->xpl.q380.BaroStdFO,   0);
        }
        if (yfms->xpl.atyp == YFS_ATYP_IXEG)
        {
            XPLMSetDataf(yfms->xpl.ixeg.baro_inhg_sby_0001_ind, alt);
        }
        XPLMSetDataf(yfms->xpl.barometer_setting_in_hg_copilot, alt);
        XPLMSetDataf(yfms->xpl.barometer_setting_in_hg_pilot,   alt);
        yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
    }
    if (key[0] == 1 && key[1] == 4)
    {
        char buf[YFS_DISPLAY_NUMC + 1]; yfs_spad_copy2(yfms, buf);
        if  (strnlen(buf, 1) && !yfms->mwindow.screen.spad_reset)
        {
            if (!strcasecmp(buf, "InHg"))
            {
                yfms->ndt.alt.unit = 0; yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
            }
            if (!strcasecmp(buf, "HPa"))
            {
                yfms->ndt.alt.unit = 1; yfs_spad_clear(yfms); yfs_rad1_pageupdt(yfms); return;
            }
            yfs_spad_reset(yfms, "FORMAT ERROR", -1); return;
        }
        else
        {
            yfms->ndt.alt.unit = !yfms->ndt.alt.unit; yfs_rad1_pageupdt(yfms); return;
        }
    }
    if (key[0] == 0 && key[1] == 5)
    {
        if (yfms->xpl.atyp == YFS_ATYP_Q350)
        {
            XPLMSetDatai(yfms->xpl.q350.pressLeftButton,  1);
            XPLMSetDatai(yfms->xpl.q350.pressRightButton, 1);
            yfs_rad1_pageupdt(yfms); return;
        }
        if (yfms->xpl.atyp == YFS_ATYP_FB76)
        {
            XPLMSetDataf(yfms->xpl.fb76.baroRotary_stby,  0.5f);
            XPLMSetDataf(yfms->xpl.fb76.baroRotary_left,  0.5f);
            XPLMSetDataf(yfms->xpl.fb76.baroRotary_right, 0.5f);
            yfs_rad1_pageupdt(yfms); return;
        }
        if (yfms->xpl.atyp == YFS_ATYP_FB77)
        {
            // only reached w/altimeter in manual mode
            // switch altimeter to "STD" mode instead
            XPLMSetDatai(yfms->xpl.fb77.anim_175_button, 1);
            yfs_rad1_pageupdt(yfms); return;
        }
        if (yfms->xpl.atyp == YFS_ATYP_IXEG)
        {
            XPLMSetDataf(yfms->xpl.ixeg.baro_inhg_sby_0001_ind, 29.92f);
        }
        XPLMSetDataf(yfms->xpl.barometer_setting_in_hg_copilot, 29.92f);
        XPLMSetDataf(yfms->xpl.barometer_setting_in_hg_pilot,   29.92f);
        if (yfms->xpl.atyp == YFS_ATYP_QPAC)
        {
            XPLMSetDatai(yfms->xpl.qpac.BaroStdCapt, 1);
            XPLMSetDatai(yfms->xpl.qpac.BaroStdFO,   1);
        }
        if (yfms->xpl.atyp == YFS_ATYP_Q380)
        {
            XPLMSetDatai(yfms->xpl.q380.BaroStdCapt, 1);
            XPLMSetDatai(yfms->xpl.q380.BaroStdFO,   1);
        }
        yfs_rad1_pageupdt(yfms); return;
    }
    if (key[0] == 1 && key[1] == 5)
    {
        if (XPLMGetDatai(yfms->xpl.transponder_mode) >= 2 &&
            XPLMGetDatai(yfms->xpl.transponder_id  ) == 0)
        {
            XPLMCommandOnce(yfms->xpl.transponder_ident);
        }
        yfs_rad1_pageupdt  (yfms); return;
    }
}
