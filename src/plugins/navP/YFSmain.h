/*
 * YFSmain.h
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

#ifndef YFS_MAIN_H
#define YFS_MAIN_H

#include <stdint.h>

#include "Widgets/XPWidgets.h"
#include "XPLM/XPLMDataAccess.h"
#include "XPLM/XPLMMenus.h"
#include "XPLM/XPLMProcessing.h"
#include "XPLM/XPLMUtilities.h"

#include "lib/flightplan.h"
#include "lib/navdata.h"

// TODO: when new aircraft loaded, reset YFMS automatically

#define YFS_DISPLAY_NUMC 24 // number of positions per row
#define YFS_DISPLAY_NUMR 14 // number of rows

typedef void (*YFS_SPC_f)(void *yfms                             );
typedef void (*YFS_LSK_f)(void *yfms, int key[2], intptr_t refcon);

typedef struct
{
    // callbacks for each special key: ensure we can switch to page, then do it
    struct
    {
        YFS_SPC_f cback_dirt;
        YFS_SPC_f cback_prog;
        YFS_SPC_f cback_perf;
        YFS_SPC_f cback_init;
        YFS_SPC_f cback_data;
        YFS_SPC_f cback_fpln;
        YFS_SPC_f cback_radn;
        YFS_SPC_f cback_fuel;
        YFS_SPC_f cback_sflp;
        YFS_SPC_f cback_atcc;
        YFS_SPC_f cback_menu;
        YFS_SPC_f cback_arpt;
        YFS_SPC_f cback_null;
        YFS_SPC_f cback_left;
        YFS_SPC_f cback_lnup;
        YFS_SPC_f cback_rigt;
        YFS_SPC_f cback_lndn;
    }
    spcs;

    // callbacks for all line select keys (must be implemented by each page)
    struct
    {
        YFS_LSK_f cback;
        intptr_t refcon;
    }
    lsks[2][6];

    struct
    {
        XPWidgetID id;
        int win_state;
        int ks_rgstrd;
        enum
        {
            YFS_KSM_OFF,
            YFS_KSM_DSP,
            YFS_KSM_WIN,
            YFS_KSM_ALL,
        }
        ks_mode;
        struct
        {
            // line select keys
            XPWidgetID keyid_lsk[6];
            XPWidgetID keyid_rsk[6];
            // alphanumeric keys
            XPWidgetID keyid_num0;
            XPWidgetID keyid_num1;
            XPWidgetID keyid_num2;
            XPWidgetID keyid_num3;
            XPWidgetID keyid_num4;
            XPWidgetID keyid_num5;
            XPWidgetID keyid_num6;
            XPWidgetID keyid_num7;
            XPWidgetID keyid_num8;
            XPWidgetID keyid_num9;
            XPWidgetID keyid_al_a;
            XPWidgetID keyid_al_b;
            XPWidgetID keyid_al_c;
            XPWidgetID keyid_al_d;
            XPWidgetID keyid_al_e;
            XPWidgetID keyid_al_f;
            XPWidgetID keyid_al_g;
            XPWidgetID keyid_al_h;
            XPWidgetID keyid_al_i;
            XPWidgetID keyid_al_j;
            XPWidgetID keyid_al_k;
            XPWidgetID keyid_al_l;
            XPWidgetID keyid_al_m;
            XPWidgetID keyid_al_n;
            XPWidgetID keyid_al_o;
            XPWidgetID keyid_al_p;
            XPWidgetID keyid_al_q;
            XPWidgetID keyid_al_r;
            XPWidgetID keyid_al_s;
            XPWidgetID keyid_al_t;
            XPWidgetID keyid_al_u;
            XPWidgetID keyid_al_v;
            XPWidgetID keyid_al_w;
            XPWidgetID keyid_al_x;
            XPWidgetID keyid_al_y;
            XPWidgetID keyid_al_z;
            // special keys
            XPWidgetID keyid_clir;
            XPWidgetID keyid_ovfy;
            XPWidgetID keyid_plus;
            XPWidgetID keyid_pird;
            XPWidgetID keyid_spce;
            XPWidgetID keyid_slsh;
            // FMS pages
            XPWidgetID keyid_dirt;
            XPWidgetID keyid_prog;
            XPWidgetID keyid_perf;
            XPWidgetID keyid_init;
            XPWidgetID keyid_data;
            XPWidgetID keyid_fpln;
            XPWidgetID keyid_radn;
            XPWidgetID keyid_fuel;
            XPWidgetID keyid_sfpl;
            XPWidgetID keyid_atcc;
            XPWidgetID keyid_menu;
            XPWidgetID keyid_arpt;
            XPWidgetID keyid_null;
            XPWidgetID keyid_left;
            XPWidgetID keyid_rigt;
            XPWidgetID keyid_lnup;
            XPWidgetID keyid_lndn;
        }
        keys;

        struct
        {
            XPWidgetID bgrd_id;
            XPWidgetID subw_id;
            int        sw_inBM;
            int        sw_inLT;
            int        sw_inTP;
            int        sw_inRT;
            XPWidgetID line_id[YFS_DISPLAY_NUMR];
            int        ln_inBM[YFS_DISPLAY_NUMR];
            int        ln_inLT[YFS_DISPLAY_NUMR];
            int        ln_inTP[YFS_DISPLAY_NUMR];
            int        ln_inRT[YFS_DISPLAY_NUMR];
            int           colr[YFS_DISPLAY_NUMR][YFS_DISPLAY_NUMC];
            char          text[YFS_DISPLAY_NUMR][YFS_DISPLAY_NUMC + 1];
            enum
            {
                COLR_IDX_BLACK   = 0,
                COLR_IDX_BLUE    = 1,
                COLR_IDX_GREEN   = 2,
                COLR_IDX_MAGENTA = 3,
                COLR_IDX_RED     = 4,
                COLR_IDX_WHITE   = 5,
                COLR_IDX_ORANGE  = 6,
                COLR_IDX_YELLOW  = 7,
            };
            int  spad_reset;
            int  spad_backup;
            char spad_bupbuf[YFS_DISPLAY_NUMC + 1];
        }
        screen;

        enum
        {
            PAGE0     = 0,
            PAGE_IDNT = 1,
            PAGE_MENU = 2,
            PAGE_RAD1 = 3,
            PAGE_RAD2 = 4,
        }
        current_page;
    }
    mwindow;

    struct
    {
        int                    before;
        void                  *refcon;
        XPLMCommandRef        command;
        XPLMCommandCallback_f handler;
    }
    toggle;

    struct
    {
        XPLMMenuID id;
        struct
        {
            int toggle;
        }
        items;
    }
    menu;

    struct
    {
        char xsystem_pth[513];
        ndt_navdatabase  *ndb;
        struct
        {
            ndt_flightplan *arr;
            ndt_flightplan *dep;
            ndt_flightplan *iac;
            ndt_flightplan *rte;
        } flp;
        struct
        {
            int unit;
        } alt;
    }
    ndt;

    struct
    {
        XPLMDataRef acf_ICAO;
        XPLMDataRef acf_en_type;
        XPLMDataRef acf_num_engines;
        XPLMDataRef transponder_id;
        XPLMDataRef transponder_mode;
        XPLMDataRef transponder_code;
        XPLMDataRef com1_frequency_Mhz;
        XPLMDataRef com1_standby_frequency_Mhz;
        XPLMDataRef com1_frequency_khz;
        XPLMDataRef com1_standby_frequency_khz;
        XPLMDataRef com2_frequency_Mhz;
        XPLMDataRef com2_standby_frequency_Mhz;
        XPLMDataRef com2_frequency_khz;
        XPLMDataRef com2_standby_frequency_khz;
        XPLMDataRef barometer_setting_in_hg_pilot;
        XPLMDataRef barometer_setting_in_hg_copilot;
        XPLMCommandRef transponder_ident;
        XPLMCommandRef com1_standy_flip;
        XPLMCommandRef com2_standy_flip;
        XPLMFlightLoop_f fl_callback;
        enum
        {
            YFS_ATYP_NSET = 0,
            YFS_ATYP_XPLN = 1,
            YFS_ATYP_QPAC = 2,
            YFS_ATYP_IXEG = 3,
            YFS_ATYP_FB76 = 6,
        }
        atyp;
        struct
        {
            XPLMDataRef xpdr_mode_act;
            XPLMDataRef xpdr_stby_act;
        } ixeg;
        struct
        {
            XPLMDataRef XPDRPower;
            XPLMDataRef XPDRAltitude;
            XPLMDataRef BaroStdCapt;
            XPLMDataRef BaroStdFO;
            XPLMCommandRef VHF2Co;
            XPLMCommandRef VHF1Capt;
            XPLMCommandRef RMPSwapCo;
            XPLMCommandRef RMPSwapCapt;
            XPLMCommandRef RMP1FreqUpLrg;
            XPLMCommandRef RMP2FreqUpLrg;
            XPLMCommandRef RMP1FreqUpSml;
            XPLMCommandRef RMP2FreqUpSml;
            XPLMCommandRef RMP1FreqDownLrg;
            XPLMCommandRef RMP2FreqDownLrg;
            XPLMCommandRef RMP1FreqDownSml;
            XPLMCommandRef RMP2FreqDownSml;
        }
        qpac;
        struct
        {
            XPLMDataRef systemMode;
            XPLMDataRef baroRotary_stby;
            XPLMDataRef baroRotary_left;
            XPLMDataRef baroRotary_right;
        }
        fb76;
    }
    xpl;
}
yfms_context;

void* yfs_main_init (void                       );
void  yfs_main_toggl(yfms_context*              );
void  yfs_main_rline(yfms_context*,     int, int);
int   yfs_main_close(yfms_context**             );
void  yfs_printf_lft(void*, int, int, int, char*);
void  yfs_printf_rgt(void*, int, int, int, char*);

#endif /* YFS_MAIN_H */
