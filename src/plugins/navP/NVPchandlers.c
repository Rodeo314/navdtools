/*
 * NVPchandlers.c
 *
 * This file is part of the navdtools source code.
 *
 * (C) Copyright 2015 Timothy D. Walker and others.
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

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "XPLM/XPLMDataAccess.h"
#include "XPLM/XPLMDisplay.h"
#include "XPLM/XPLMMenus.h"
#include "XPLM/XPLMPlanes.h"
#include "XPLM/XPLMPlugin.h"
#include "XPLM/XPLMProcessing.h"
#include "XPLM/XPLMUtilities.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-attributes"
#endif
#include <stdbool.h>
#include "assert/sharedvalue.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "common/common.h"

#include "NVPchandlers.h"
#include "NVPmenu.h"

typedef struct
{
    int                    before;
    void                  *refcon;
    XPLMCommandRef        command;
    XPLMCommandCallback_f handler;
} chandler_callback;

typedef struct
{
    const char    *name;
    XPLMCommandRef xpcr;
} chandler_command;

typedef struct
{
    int           initialized;
    XPLMPluginID    plugin_id;
    SharedValuesInterface api;
    chandler_callback ap_conn;
    chandler_callback ap_disc;
    chandler_callback at_disc;
    struct
    {
        XPLMCommandRef p_brk_toggle;
        XPLMDataRef ldg_gears_lever;
        int id_s32_click_autopilot1;
        int id_s32_click_ss_tkovr_l;
        int id_s32_click_thr_disc_l;
        int id_flt_pedal_brake_left;
        int id_flt_pedal_brake_rigt;
        int id_u32_emer_lights_mode;
        int id_u32_efis_nav_rng_lft;
        int id_u32_efis_nav_rng_rgt;
        int id_u32_efis_nav_mod_lft;
        int id_u32_efis_nav_mod_rgt;
        int id_u32_fcu_tgt_alt_step;
    } dat;
} refcon_assert1;

typedef struct
{
    void        *assert;
    float       rtio[3];
    float       flt_var;
    int         int_var;
    XPLMDataRef p_b_flt;
    XPLMDataRef p_b_int;
    XPLMDataRef p_b_rat;
    XPLMDataRef protate;
    XPLMDataRef g_speed;
    chandler_command rg;
    chandler_command mx;
    chandler_command ro;
} refcon_braking;

typedef struct
{
    int              ready;
    XPLMDataRef    pkb_ref;
    XPLMCommandRef h_b_max;
    XPLMCommandRef h_b_reg;
} refcon_qpacfbw;

typedef struct
{
    int        ready;
    XPLMDataRef slat;
} refcon_ixeg733;

typedef struct
{
    int            ready;
    XPLMCommandRef sparm;
    XPLMCommandRef spret;
    XPLMCommandRef pt_up;
    XPLMCommandRef pt_dn;
} refcon_eadt738;

typedef struct
{
    const char          *auth;
    const char          *desc;
    int                  atyp;
    int            i_disabled;
    int            i_value[2];
    XPLMDataRef    dataref[3];
    XPLMCommandRef command[4];
} refcon_cdu_pop;

typedef struct
{
    float   nominal_roll_c;
    XPLMDataRef acf_roll_c;
    XPLMDataRef ground_spd;
    XPLMFlightLoop_f flc_g;
    struct
    {
        chandler_callback preset;
        XPLMDataRef throttle_all;
        XPLMDataRef thrott_array;
        XPLMDataRef park_b_ratio;
        float r_t[2];
        float r_taxi;
        float r_idle;
        int minimums;
    } idle;
} refcon_ground;

typedef struct
{
    XPLMPluginID pe;
    XPLMDataRef pe1;
    XPLMDataRef atc;
    XPLMDataRef cvr;
    XPLMDataRef evr;
    XPLMDataRef fvr;
    XPLMDataRef gvr;
    XPLMDataRef pvr;
    XPLMDataRef wvr;
    XPLMDataRef wxr;
    XPLMDataRef tmp;
    XPLMDataRef spc;
    XPLMDataRef snd;
    XPLMDataRef txt;
} refcon_volumes;

typedef struct
{
    chandler_callback landing_gear_toggle;
    chandler_callback   landing_gear_down;
    chandler_callback     landing_gear_up;
    XPLMDataRef          acf_gear_retract;
    XPLMDataRef          gear_handle_down;
    int              has_retractable_gear;
    void                          *assert;
    struct
    {
        XPLMDataRef ref;
        int       atype;
    } callouts;
} refcon_gear;

typedef struct
{
    int initialized;
    int first_fcall;
    int kill_daniel;

    void *menu_context;

    char auth[41]; // addon's PlaneMaker author information
    char desc[41]; // addon's PlaneMaker description string
    char icao[ 5]; // addon's ICAO aircraft type designator
    enum
    {
        NVP_ACF_GENERIC = 0x0000000,
        NVP_ACF_A320_JD = 0x0000001,
        NVP_ACF_A320_QP = 0x0000002,
        NVP_ACF_A330_JD = 0x0000004,
        NVP_ACF_A330_RW = 0x0000008,
        NVP_ACF_A350_FF = 0x0000010,
        NVP_ACF_A380_PH = 0x0000020,
        NVP_ACF_B727_FJ = 0x0000040,
        NVP_ACF_B737_EA = 0x0000080,
        NVP_ACF_B737_FJ = 0x0000100,
        NVP_ACF_B737_XG = 0x0000200,
        NVP_ACF_B757_FF = 0x0001000,
        NVP_ACF_B767_FF = 0x0002000,
        NVP_ACF_B777_FF = 0x0004000,
        NVP_ACF_A320ULT = 0x0008000,
        NVP_ACF_DH8D_FJ = 0x0010000,
        NVP_ACF_EMBE_SS = 0x0020000,
        NVP_ACF_EMBE_XC = 0x0040000,
        NVP_ACF_ERJ1_4D = 0x0080000,
        NVP_ACF_SSJ1_RZ = 0x0100000,
        NVP_ACF_HA4T_RW = 0x0200000,
        NVP_ACF_MD80_RO = 0x0400000,
    } atyp;
#define NVP_ACF_MASK_FFR  0x010F010 // all FlightFactor addons
#define NVP_ACF_MASK_FJS  0x0010140 // all of FlyJSim's addons
#define NVP_ACF_MASK_JDN  0x0000005 // all J.A.R.Design addons
#define NVP_ACF_MASK_QPC  0x000003A // all QPAC-powered addons
#define NVP_ACF_MASK_SSG  0x0020000 // all SSGroup/FJCC addons
#define NVP_ACF_MASK_XCR  0x0040000 // all X-Crafts/S.W addons
#define NVP_ACF_MASK_320  0x0008003 // all A320 series aircraft
#define NVP_ACF_MASK_330  0x000000C // all A330 series aircraft
#define NVP_ACF_MASK_350  0x0000010 // all A350 series aircraft
#define NVP_ACF_MASK_380  0x0000020 // all A380 series aircraft
#define NVP_ACF_MASK_727  0x0000040 // all B727 series aircraft
#define NVP_ACF_MASK_737  0x0000380 // all B737 series aircraft
#define NVP_ACF_MASK_757  0x0001000 // all B757 series aircraft
#define NVP_ACF_MASK_767  0x0002000 // all B767 series aircraft
#define NVP_ACF_MASK_777  0x0004000 // all B777 series aircraft
#define NVP_ACF_MASK_EMB  0x00E0000 // all EMB* series aircraft
#define NVP_ACF_MASK_MD8  0x0400000 // all MD80 series aircraft

    /*
     * Note to self: the QPAC plugin (at least A320) seems to overwrite radio
     * frequency datarefs with its own; I found the datarefs but they're not
     * writable. There are custom commands though:
     *
     * - AirbusFBW/RMP3FreqUpLrg
     * - AirbusFBW/RMP3FreqUpSml
     * - AirbusFBW/RMP3FreqDownLrg
     * - AirbusFBW/RMP3FreqDownSml
     * - AirbusFBW/RMP3Swap etc.
     *
     * A plugin could control radio frequencies manually via said commands.
     *
     * It's worth noting that RMP1 != COM1, RMP2 != COM2; each RMP may control
     * either of COM1 or 2; before changing frequencies via an RMP, we must set
     * the RMP to the COM radio we want to change frequency for using the
     * relevant custom command:
     *
     * - AirbusFBW/VHF1RMP3
     * - AirbusFBW/VHF2RMP3
     *
     * RMP3 should be the backup radio panel located overhead the pilots.
     */
    refcon_volumes volumes;

    refcon_assert1 assert;

    refcon_ground ground;

    refcon_gear gear;

    struct
    {
        int         var_park_brake;
        XPLMDataRef ref_park_brake;
        int         var_speedbrake;
        XPLMDataRef ref_speedbrake;
        int         var_flap_lever;
        XPLMDataRef ref_flap_lever;
        int         var_gear_lever;
        XPLMDataRef ref_gear_lever;

        // named, plane-specific (hardcoded) flap callouts
        chandler_callback cb_flapu;
        chandler_callback cb_flapd;
        XPLMDataRef ref_flap_ratio;
        XPLMFlightLoop_f flc_flaps;
    } callouts;

    struct
    {
        refcon_qpacfbw qpac;
        refcon_ixeg733 i733;
        refcon_eadt738 x738;
    } acfspec;

    struct
    {
        struct
        {
            chandler_callback cb;
        } tur;

        struct
        {
            chandler_callback cb;
        } prk;

        struct
        {
            chandler_callback cb;
        } off;

        struct
        {
            chandler_callback cb;
        } max;

        struct
        {
            chandler_callback cb;
        } reg;

        refcon_braking rc_brk;
    } bking;

    struct
    {
        struct
        {
            chandler_callback cb;
        } ext;

        struct
        {
            chandler_callback cb;
        } ret;

        XPLMDataRef    ha4t;
        XPLMDataRef    srat;
        XPLMCommandRef sext;
        XPLMCommandRef sret;
    } spbrk;

    struct
    {
        struct
        {
            struct
            {
                XPLMCommandRef    cd;
                chandler_callback cb;
            } up;

            struct
            {
                XPLMCommandRef    cd;
                chandler_callback cb;
            } dn;
        } pch;

        struct
        {
            struct
            {
                XPLMCommandRef    cd;
                chandler_callback cb;
            } lt;

            struct
            {
                XPLMCommandRef    cd;
                chandler_callback cb;
            } rt;
        } ail;

        struct
        {
            struct
            {
                XPLMCommandRef    cd;
                chandler_callback cb;
            } lt;

            struct
            {
                XPLMCommandRef    cd;
                chandler_callback cb;
            } rt;
        } rud;
    } trims;

    struct
    {
        struct
        {
            chandler_callback cb;
        } fwd;

        struct
        {
            chandler_callback cb;
        } rev;

        int         n_engines;
        XPLMDataRef acf_numng;
        XPLMDataRef acf_ngtyp;
        XPLMDataRef prop_mode;
    } revrs;

    struct
    {
        struct
        {
            chandler_callback cb;
        } prev;

        struct
        {
            chandler_callback cb;
        } next;

        int               idx[10];
        chandler_callback cbs[10];
    } views;

    struct
    {
        struct
        {
            chandler_callback cb;
            XPLMDataRef      *dr;
        } ffst;

        struct
        {
            chandler_callback cb;
            chandler_command  cc;
        } conn;

        struct
        {
            chandler_callback cb;
            chandler_command  cc;
        } disc;
    } otto;

    struct
    {
        struct
        {
            chandler_callback cb;
            chandler_command  cc;
        } disc;

        struct
        {
            chandler_callback cb;
            chandler_command  cc;
        } toga;
    } athr;

    struct
    {
        chandler_callback cb;
        refcon_cdu_pop    rc;
    } mcdu;

    struct
    {
        XPLMDataRef dataref[8];
        XPLMCommandRef comm[8];
    } debug;
} chandler_context;

/* Callout default values */
#define CALLOUT_PARKBRAKE (-1)
#define CALLOUT_SPEEDBRAK (-1)
#define CALLOUT_FLAPLEVER (-1)
#define CALLOUT_GEARLEVER (-1)

/* Flap lever position name constants */
static       char  _flap_callout_st[11];
static const char* _flap_names_2POS[10] = {    "up",  "half",  "full",   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_3POS[10] = {    "up",     "1",     "2", "full",   NULL,   NULL,   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_A10W[10] = {    "up",    "15",    "30",   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_AIB1[10] = {    "up",     "1",     "2",    "3", "full",   NULL,   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_B190[10] = {    "up",    "17",    "35",   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_B52G[10] = {    "up",    "10",    "20",   "30",   "40",   "55",   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_BD5J[10] = {    "up",    "10",    "20",   "30",   NULL,   NULL,   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_BNG1[10] = {    "up",     "1",     "2",    "5",   "10",   "15",   "25",   "30",   "40",   NULL, };
static const char* _flap_names_BNG2[10] = {    "up",     "1",     "5",   "15",   "20",   "25",   "30",   NULL,   NULL,   NULL, };
static const char* _flap_names_BOM1[10] = {    "up",    "10",    "20",   "30",   NULL,   NULL,   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_BOM2[10] = {    "up",     "5",    "10",   "15",   "35",   NULL,   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_C130[10] = {    "up",     "1",     "2",    "3",    "4",    "5",    "6", "full",   NULL,   NULL, };
static const char* _flap_names_C340[10] = {    "up",    "15",    "45",   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_CSNA[10] = {    "up",    "10",    "20", "full",   NULL,   NULL,   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_DC10[10] = {    "up",     "5",    "15",   "25",   "30",   "35",   "40",   NULL,   NULL,   NULL, };
static const char* _flap_names_EMB1[10] = {    "up",     "9",    "18",   "22",   "45",   NULL,   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_EMB2[10] = {    "up",     "1",     "2",    "3",    "4",    "5", "full",   NULL,   NULL,   NULL, };
static const char* _flap_names_FA7X[10] = {    "up",     "1",     "2", "full",   NULL,   NULL,   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_HA4T[10] = {    "up",    "12",    "20",   "35",   NULL,   NULL,   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_MD80[10] = {    "up",     "0",     "5",   "11",   "15",   "28",   "40",   NULL,   NULL,   NULL, };
static const char* _flap_names_PC12[10] = {    "up",    "15",    "30",   "40",   NULL,   NULL,   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_PIPA[10] = {    "up",    "15",    "30",   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_PIPR[10] = {    "up",    "10",    "25",   "40",   NULL,   NULL,   NULL,   NULL,   NULL,   NULL, };
static const char* _flap_names_TBM8[10] = { "8 5 0",   "up",  "half",  "full",   NULL,   NULL,   NULL,   NULL,   NULL,   NULL, };
static       void   flap_callout_setst(const char *names[], int index)
{
    if (index < 0) index = 0; else if (index > 9) index = 9;
    if (names[index])
    {
        snprintf(_flap_callout_st, sizeof(_flap_callout_st), "Flaps %s", names[index]);
    }
    else
    {
        snprintf(_flap_callout_st, sizeof(_flap_callout_st), "%s", "");
    }
}
static void flap_callout_speak(void)
{
    if (strnlen(_flap_callout_st, 1))
    {
        XPLMSpeakString(_flap_callout_st);
    }
}

/* thrust reverser mode constants */
static int _PROPMODE_FWD[8] = { 1, 1, 1, 1, 1, 1, 1, 1, };
static int _PROPMODE_REV[8] = { 3, 3, 3, 3, 3, 3, 3, 3, };
static int* propmode_fwd_get(void)
{
    return _PROPMODE_FWD;
}
static int* propmode_rev_get(void)
{
    return _PROPMODE_REV;
}

/* Quicklook view index and accessors */
static int         _var_ql_idx = 0; // default view minus 1
static int*         var_ql_idx_get(void)
{
    return &_var_ql_idx;
}
static XPLMDataRef _ref_ql_idx = NULL;
static void         ref_ql_idx_set(XPLMDataRef xdr)
{
    _ref_ql_idx = xdr;
}
static XPLMDataRef  ref_ql_idx_get(void)
{
    return _ref_ql_idx;
}
static int          ref_ql_idx_val(void)
{
    return XPLMGetDatai(_ref_ql_idx);
}

/* Readability macros to (un)register custom command handlers */
#define REGISTER_CHANDLER(_callback, _handler, _before, _refcon)                \
{                                                                               \
    XPLMRegisterCommandHandler(((_callback).command),                           \
                               ((_callback).handler = (&(_handler))),           \
                               ((_callback).before  = (((_before)))),           \
                               ((_callback).refcon  = (((_refcon)))));          \
}
#define UNREGSTR_CHANDLER(_callback)                                            \
{                                                                               \
    if ((_callback).handler)                                                    \
    {                                                                           \
        XPLMUnregisterCommandHandler((_callback).command,                       \
                                     (_callback).handler,                       \
                                     (_callback).before,                        \
                                     (_callback).refcon);                       \
        (_callback).handler = NULL;                                             \
    }                                                                           \
}

/* Convenience macro to automatically check and assign a dataref */
#define _DO(_verbose, _func, _val, _name) { if ((d_ref = XPLMFindDataRef(_name))) _func(d_ref, _val); else if (_verbose) ndt_log("navP [warning]: dataref not found: \"%s\"\n", _name); }

static int  dataref_read_string(XPLMDataRef dataref, char *string_buffer,  size_t buffer_size);
static int  dataref_wrte_string(XPLMDataRef dataref, char *string_buffer,  size_t buffer_size);
static int chandler_turna(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_p_max(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_p_off(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_b_max(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_b_reg(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_swtch(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_sp_ex(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_sp_re(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_pt_up(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_pt_dn(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_at_lt(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_at_rt(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_rt_lt(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_rt_rt(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_r_fwd(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_r_rev(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_sview(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_qlprv(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_qlnxt(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_flchg(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_mcdup(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_ffap1(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_32apc(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_32apd(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_32atd(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_ghndl(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int chandler_idleb(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static float flc_flap_func (                                        float, float, int, void*);
static float gnd_stab_hdlr (                                        float, float, int, void*);
static int   first_fcall_do(                                           chandler_context *ctx);
static int   aibus_fbw_init(                                             refcon_qpacfbw *fbw);
static int   boing_733_init(                                             refcon_ixeg733 *i33);
static int   boing_738_init(                                             refcon_eadt738 *x38);
static int   ff_assert_init(                                             refcon_assert1 *ffa);
static int   priv_getdata_i(                                   void *inRefcon               );
static void  priv_setdata_i(                                   void *inRefcon, int   inValue);
static float priv_getdata_f(                                   void *inRefcon               );
static void  priv_setdata_f(                                   void *inRefcon, float inValue);

void* nvp_chandlers_init(void)
{
    chandler_context *ctx = calloc(1, sizeof(chandler_context));
    if (!ctx)
    {
        return NULL;
    }

    /*
     * Private datarefs: callouts.
     *
     * Defaults are set by the menu code, but we
     * also initialize the variables here anyway.
     *
     * Note: XPLMGet/SetDatai don't seem to work from XPluginStart().
     */
    ctx->callouts.var_park_brake = CALLOUT_PARKBRAKE;
    ctx->callouts.ref_park_brake = XPLMRegisterDataAccessor("navP/callouts/park_brake",
                                                            xplmType_Int, 1,
                                                            &priv_getdata_i, &priv_setdata_i,
                                                            NULL, NULL,
                                                            NULL, NULL,
                                                            NULL, NULL,
                                                            NULL, NULL,
                                                            NULL, NULL,
                                                            &ctx->callouts.var_park_brake,
                                                            &ctx->callouts.var_park_brake);
    ctx->callouts.var_speedbrake = CALLOUT_SPEEDBRAK;
    ctx->callouts.ref_speedbrake = XPLMRegisterDataAccessor("navP/callouts/speedbrake",
                                                            xplmType_Int, 1,
                                                            &priv_getdata_i, &priv_setdata_i,
                                                            NULL, NULL,
                                                            NULL, NULL,
                                                            NULL, NULL,
                                                            NULL, NULL,
                                                            NULL, NULL,
                                                            &ctx->callouts.var_speedbrake,
                                                            &ctx->callouts.var_speedbrake);
    ctx->callouts.var_flap_lever = CALLOUT_FLAPLEVER;
    ctx->callouts.ref_flap_lever = XPLMRegisterDataAccessor("navP/callouts/flap_lever",
                                                            xplmType_Int, 1,
                                                            &priv_getdata_i, &priv_setdata_i,
                                                            NULL, NULL,
                                                            NULL, NULL,
                                                            NULL, NULL,
                                                            NULL, NULL,
                                                            NULL, NULL,
                                                            &ctx->callouts.var_flap_lever,
                                                            &ctx->callouts.var_flap_lever);
    ctx->callouts.var_gear_lever = CALLOUT_GEARLEVER;
    ctx->callouts.ref_gear_lever = XPLMRegisterDataAccessor("navP/callouts/gear_lever",
                                                            xplmType_Int, 1,
                                                            &priv_getdata_i, &priv_setdata_i,
                                                            NULL, NULL,
                                                            NULL, NULL,
                                                            NULL, NULL,
                                                            NULL, NULL,
                                                            NULL, NULL,
                                                            &ctx->callouts.var_gear_lever,
                                                            &ctx->callouts.var_gear_lever);
    if (!ctx->callouts.ref_park_brake ||
        !ctx->callouts.ref_speedbrake ||
        !ctx->callouts.ref_flap_lever ||
        !ctx->callouts.ref_gear_lever)
    {
        goto fail;
    }

    /* Volume-related datarefs */
    if ((ctx->volumes.wxr = XPLMFindDataRef("sim/operation/sound/weather_volume_ratio")) == NULL ||
        (ctx->volumes.wvr = XPLMFindDataRef("sim/operation/sound/warning_volume_ratio")) == NULL ||
        (ctx->volumes.evr = XPLMFindDataRef( "sim/operation/sound/engine_volume_ratio")) == NULL ||
        (ctx->volumes.gvr = XPLMFindDataRef( "sim/operation/sound/ground_volume_ratio")) == NULL ||
        (ctx->volumes.atc = XPLMFindDataRef(  "sim/operation/sound/radio_volume_ratio")) == NULL ||
        (ctx->volumes.pvr = XPLMFindDataRef(   "sim/operation/sound/prop_volume_ratio")) == NULL ||
        (ctx->volumes.fvr = XPLMFindDataRef(    "sim/operation/sound/fan_volume_ratio")) == NULL ||
        (ctx->volumes.spc = XPLMFindDataRef(           "sim/operation/sound/speech_on")) == NULL ||
        (ctx->volumes.snd = XPLMFindDataRef(            "sim/operation/sound/sound_on")) == NULL ||
        (ctx->volumes.txt = XPLMFindDataRef(            "sim/operation/prefs/text_out")) == NULL)
    {
        goto fail;
    }

    /* Private datarefs: braking */
    ctx->bking.rc_brk.p_b_int = XPLMRegisterDataAccessor("private/temp/integer/refcon_braking",
                                                         xplmType_Int, 1,
                                                         &priv_getdata_i, &priv_setdata_i,
                                                         NULL, NULL,
                                                         NULL, NULL,
                                                         NULL, NULL,
                                                         NULL, NULL,
                                                         NULL, NULL,
                                                         &ctx->bking.rc_brk.int_var,
                                                         &ctx->bking.rc_brk.int_var);
    ctx->bking.rc_brk.p_b_flt = XPLMRegisterDataAccessor("private/temp/floatpt/refcon_braking",
                                                         xplmType_Float, 1,
                                                         NULL, NULL,
                                                         &priv_getdata_f, &priv_setdata_f,
                                                         NULL, NULL,
                                                         NULL, NULL,
                                                         NULL, NULL,
                                                         NULL, NULL,
                                                         &ctx->bking.rc_brk.flt_var,
                                                         &ctx->bking.rc_brk.flt_var);
    if (!ctx->bking.rc_brk.p_b_int || !ctx->bking.rc_brk.p_b_flt)
    {
        goto fail;
    }

    /* Custom commands: braking */
    ctx->bking.tur.cb.command = XPLMCreateCommand("navP/turnaround_set", "friendly cold & dark");
    ctx->bking.prk.cb.command = XPLMCreateCommand("navP/brakes/parking", "apply max. park brake");
    ctx->bking.off.cb.command = XPLMCreateCommand("navP/brakes/release", "release parking brake");
    ctx->bking.max.cb.command = XPLMCreateCommand("navP/brakes/maximum", "maximum braking action");
    ctx->bking.reg.cb.command = XPLMCreateCommand("navP/brakes/regular", "regular braking action");
    ctx->bking.rc_brk.p_b_rat = XPLMFindDataRef  ("sim/cockpit2/controls/parking_brake_ratio");
    if (!ctx->bking.tur.cb.command ||
        !ctx->bking.prk.cb.command || !ctx->bking.off.cb.command ||
        !ctx->bking.max.cb.command || !ctx->bking.reg.cb.command ||
        !ctx->bking.rc_brk.p_b_rat || !XPLMCanWriteDataRef(ctx->bking.rc_brk.p_b_rat))
    {
        goto fail;
    }
    else
    {
        REGISTER_CHANDLER(ctx->bking.tur.cb, chandler_turna, 0, ctx);
        REGISTER_CHANDLER(ctx->bking.prk.cb, chandler_p_max, 0, ctx);
        REGISTER_CHANDLER(ctx->bking.off.cb, chandler_p_off, 0, ctx);
        REGISTER_CHANDLER(ctx->bking.max.cb, chandler_b_max, 0, ctx);
        REGISTER_CHANDLER(ctx->bking.reg.cb, chandler_b_reg, 0, ctx);
        ctx->ground.idle.park_b_ratio =    ctx->bking.rc_brk.p_b_rat;
    }

    /* Custom commands: speedbrakes/spoilers */
    ctx->spbrk.ext.cb.command = XPLMCreateCommand("navP/spoilers/extend",  "speedbrakes extend one");
    ctx->spbrk.ret.cb.command = XPLMCreateCommand("navP/spoilers/retract", "speedbrakes retract one");
    ctx->spbrk.sext           = XPLMFindCommand  ("sim/flight_controls/speed_brakes_down_one");
    ctx->spbrk.sret           = XPLMFindCommand  ("sim/flight_controls/speed_brakes_up_one");
    ctx->spbrk.srat           = XPLMFindDataRef  ("sim/cockpit2/controls/speedbrake_ratio");
    if (!ctx->spbrk.ext.cb.command || !ctx->spbrk.ret.cb.command ||
        !ctx->spbrk.sext || !ctx->spbrk.sret || !ctx->spbrk.srat)
    {
        goto fail;
    }
    else
    {
        REGISTER_CHANDLER(ctx->spbrk.ext.cb, chandler_sp_ex, 0, ctx);
        REGISTER_CHANDLER(ctx->spbrk.ret.cb, chandler_sp_re, 0, ctx);
    }

    /* Custom commands: trims */
    ctx->trims.pch.up.cb.command = XPLMCreateCommand("navP/trims/pitch_up",      "pitch trim up one");
    ctx->trims.pch.up.cd         = XPLMFindCommand  ("sim/flight_controls/pitch_trim_up");
    ctx->trims.pch.dn.cb.command = XPLMCreateCommand("navP/trims/pitch_down",    "pitch trim down one");
    ctx->trims.pch.dn.cd         = XPLMFindCommand  ("sim/flight_controls/pitch_trim_down");
    ctx->trims.ail.lt.cb.command = XPLMCreateCommand("navP/trims/aileron_left",  "aileron trim left one");
    ctx->trims.ail.lt.cd         = XPLMFindCommand  ("sim/flight_controls/aileron_trim_left");
    ctx->trims.ail.rt.cb.command = XPLMCreateCommand("navP/trims/aileron_right", "aileron trim right one");
    ctx->trims.ail.rt.cd         = XPLMFindCommand  ("sim/flight_controls/aileron_trim_right");
    ctx->trims.rud.lt.cb.command = XPLMCreateCommand("navP/trims/rudder_left",   "rudder trim left one");
    ctx->trims.rud.lt.cd         = XPLMFindCommand  ("sim/flight_controls/rudder_trim_left");
    ctx->trims.rud.rt.cb.command = XPLMCreateCommand("navP/trims/rudder_right",  "rudder trim right one");
    ctx->trims.rud.rt.cd         = XPLMFindCommand  ("sim/flight_controls/rudder_trim_right");
    if (!ctx->trims.pch.up.cb.command || !ctx->trims.pch.up.cd ||
        !ctx->trims.pch.dn.cb.command || !ctx->trims.pch.dn.cd ||
        !ctx->trims.ail.lt.cb.command || !ctx->trims.ail.lt.cd ||
        !ctx->trims.ail.rt.cb.command || !ctx->trims.ail.rt.cd ||
        !ctx->trims.rud.lt.cb.command || !ctx->trims.rud.lt.cd ||
        !ctx->trims.rud.rt.cb.command || !ctx->trims.rud.rt.cd)
    {
        goto fail;
    }
    else
    {
        REGISTER_CHANDLER(ctx->trims.pch.up.cb, chandler_pt_up, 0, ctx);
        REGISTER_CHANDLER(ctx->trims.pch.dn.cb, chandler_pt_dn, 0, ctx);
        REGISTER_CHANDLER(ctx->trims.ail.lt.cb, chandler_at_lt, 0, ctx);
        REGISTER_CHANDLER(ctx->trims.ail.rt.cb, chandler_at_rt, 0, ctx);
        REGISTER_CHANDLER(ctx->trims.rud.lt.cb, chandler_rt_lt, 0, ctx);
        REGISTER_CHANDLER(ctx->trims.rud.rt.cb, chandler_rt_rt, 0, ctx);
    }

    /* Custom commands: reverser control */
    ctx->revrs.fwd.cb.command = XPLMCreateCommand("navP/thrust/forward", "stow thrust reversers");
    ctx->revrs.rev.cb.command = XPLMCreateCommand("navP/thrust/reverse", "deploy thrust reversers");
    ctx->revrs.acf_ngtyp      = XPLMFindDataRef  ("sim/aircraft/prop/acf_en_type");
    ctx->revrs.acf_numng      = XPLMFindDataRef  ("sim/aircraft/engine/acf_num_engines");
    ctx->revrs.prop_mode      = XPLMFindDataRef  ("sim/cockpit2/engine/actuators/prop_mode");
    if (!ctx->revrs.fwd.cb.command || !ctx->revrs.rev.cb.command ||
        !ctx->revrs.acf_numng      || !ctx->revrs.acf_ngtyp      ||
        !ctx->revrs.prop_mode)
    {
        goto fail;
    }
    else
    {
        REGISTER_CHANDLER(ctx->revrs.fwd.cb, chandler_r_fwd, 0, ctx);
        REGISTER_CHANDLER(ctx->revrs.rev.cb, chandler_r_rev, 0, ctx);
    }

    /* Custom commands: quick look views */
    ctx->views.cbs[0].command = XPLMFindCommand("sim/view/quick_look_0");
    ctx->views.cbs[1].command = XPLMFindCommand("sim/view/quick_look_1");
    ctx->views.cbs[2].command = XPLMFindCommand("sim/view/quick_look_2");
    ctx->views.cbs[3].command = XPLMFindCommand("sim/view/quick_look_3");
    ctx->views.cbs[4].command = XPLMFindCommand("sim/view/quick_look_4");
    ctx->views.cbs[5].command = XPLMFindCommand("sim/view/quick_look_5");
    ctx->views.cbs[6].command = XPLMFindCommand("sim/view/quick_look_6");
    ctx->views.cbs[7].command = XPLMFindCommand("sim/view/quick_look_7");
    ctx->views.cbs[8].command = XPLMFindCommand("sim/view/quick_look_8");
    ctx->views.cbs[9].command = XPLMFindCommand("sim/view/quick_look_9");
    for (int i = 0; i < 10; i++)
    {
        if (!ctx->views.cbs[i].command)
        {
            goto fail;
        }
        else
        {
            ctx->views.idx[i] = i;
            REGISTER_CHANDLER(ctx->views.cbs[i], chandler_sview, 0, &ctx->views.idx[i]);
        }
    }
    XPLMDataRef ref_ql_idx = XPLMRegisterDataAccessor("navP/views/quicklook_index",
                                                      xplmType_Int, 1,
                                                      &priv_getdata_i,
                                                      &priv_setdata_i,
                                                      NULL, NULL, NULL, NULL, NULL,
                                                      NULL, NULL, NULL, NULL, NULL,
                                                      var_ql_idx_get(), var_ql_idx_get());
    if (!ref_ql_idx)
    {
        goto fail;
    }
    else
    {
        ref_ql_idx_set(ref_ql_idx);
    }
    ctx->views.prev.cb.command = XPLMCreateCommand("navP/views/quick_look_prev", "previous quick look view preset");
    ctx->views.next.cb.command = XPLMCreateCommand("navP/views/quick_look_next",     "next quick look view preset");
    if (!ctx->views.prev.cb.command || !ctx->views.next.cb.command)
    {
        goto fail;
    }
    else
    {
        REGISTER_CHANDLER(ctx->views.prev.cb, chandler_qlprv, 0, ctx);
        REGISTER_CHANDLER(ctx->views.next.cb, chandler_qlnxt, 0, ctx);
    }

    /* Custom commands: autopilot and autothrottle */
    ctx->assert.ap_conn.command = XPLMCreateCommand("private/ff320/ap_conn", "NOT TO BE USED");
    ctx->assert.ap_disc.command = XPLMCreateCommand("private/ff320/ap_disc", "NOT TO BE USED");
    ctx->assert.at_disc.command = XPLMCreateCommand("private/ff320/at_disc", "NOT TO BE USED");
    ctx->otto.ffst.cb.  command = XPLMCreateCommand("private/ffsts/ap_cmdl", "NOT TO BE USED");
    ctx->otto.conn.cb.  command = XPLMCreateCommand("navP/switches/ap_conn", "A/P engagement");
    ctx->otto.disc.cb.  command = XPLMCreateCommand("navP/switches/ap_disc", "A/P disconnect");
    ctx->athr.disc.cb.  command = XPLMCreateCommand("navP/switches/at_disc", "A/T disconnect");
    ctx->athr.toga.cb.  command = XPLMCreateCommand("navP/switches/at_toga", "A/T takeoff/GA");
    if (!ctx->assert.ap_conn.command ||
        !ctx->assert.ap_disc.command ||
        !ctx->assert.at_disc.command ||
        !ctx->otto.ffst.cb.  command ||
        !ctx->otto.conn.cb.  command ||
        !ctx->otto.disc.cb.  command ||
        !ctx->athr.disc.cb.  command ||
        !ctx->athr.toga.cb.  command)
    {
        goto fail;
    }
    else
    {
        REGISTER_CHANDLER(ctx->assert.ap_conn, chandler_32apc, 0, &ctx->assert);
        REGISTER_CHANDLER(ctx->assert.ap_disc, chandler_32apd, 0, &ctx->assert);
        REGISTER_CHANDLER(ctx->assert.at_disc, chandler_32atd, 0, &ctx->assert);
        REGISTER_CHANDLER(ctx->otto.ffst.cb,   chandler_ffap1, 0, &ctx->otto.ffst.dr);
        REGISTER_CHANDLER(ctx->otto.conn.cb,   chandler_swtch, 0, &ctx->otto.conn.cc);
        REGISTER_CHANDLER(ctx->otto.disc.cb,   chandler_swtch, 0, &ctx->otto.disc.cc);
        REGISTER_CHANDLER(ctx->athr.disc.cb,   chandler_swtch, 0, &ctx->athr.disc.cc);
        REGISTER_CHANDLER(ctx->athr.toga.cb,   chandler_swtch, 0, &ctx->athr.toga.cc);
    }

    /* Default commands' handlers: flaps up or down */
    ctx->callouts.cb_flapu.command = XPLMFindCommand("sim/flight_controls/flaps_up");
    ctx->callouts.cb_flapd.command = XPLMFindCommand("sim/flight_controls/flaps_down");
    ctx->callouts.ref_flap_ratio   = XPLMFindDataRef("sim/cockpit2/controls/flap_ratio");
    if (!ctx->callouts.cb_flapu.command ||
        !ctx->callouts.cb_flapd.command ||
        !ctx->callouts.ref_flap_ratio)
    {
        goto fail;
    }
    else
    {
        REGISTER_CHANDLER(ctx->callouts.cb_flapu, chandler_flchg, 0, ctx);
        REGISTER_CHANDLER(ctx->callouts.cb_flapd, chandler_flchg, 0, ctx);
    }
    XPLMRegisterFlightLoopCallback((ctx->callouts.flc_flaps = &flc_flap_func), 0, NULL);

    /* Default commands' handlers: gear up, down, toggle */
    ctx->gear.acf_gear_retract            = XPLMFindDataRef("sim/aircraft/gear/acf_gear_retract"     );
    ctx->gear.gear_handle_down            = XPLMFindDataRef("sim/cockpit2/controls/gear_handle_down" );
    ctx->gear.landing_gear_up.command     = XPLMFindCommand("sim/flight_controls/landing_gear_up"    );
    ctx->gear.landing_gear_down.command   = XPLMFindCommand("sim/flight_controls/landing_gear_down"  );
    ctx->gear.landing_gear_toggle.command = XPLMFindCommand("sim/flight_controls/landing_gear_toggle");
    if (!ctx->gear.landing_gear_toggle.command ||
        !ctx->gear.landing_gear_down.command   ||
        !ctx->gear.landing_gear_up.command     ||
        !ctx->gear.gear_handle_down            ||
        !ctx->gear.acf_gear_retract)
    {
        goto fail;
    }
    else
    {
        ctx->gear.callouts.ref = ctx->callouts.ref_gear_lever;
        REGISTER_CHANDLER(ctx->gear.landing_gear_up,     chandler_ghndl, 1 /* before X-Plane */, &ctx->gear);
        REGISTER_CHANDLER(ctx->gear.landing_gear_down,   chandler_ghndl, 1 /* before X-Plane */, &ctx->gear);
        REGISTER_CHANDLER(ctx->gear.landing_gear_toggle, chandler_ghndl, 1 /* before X-Plane */, &ctx->gear);
    }

    /* Custom command: pop-up Control Display Unit */
    ctx->mcdu.cb.command = XPLMCreateCommand("navP/switches/cdu_toggle", "CDU pop-up/down");
    if (!ctx->mcdu.cb.command)
    {
        goto fail;
    }
    else
    {
        REGISTER_CHANDLER(ctx->mcdu.cb, chandler_mcdup, 0, &ctx->mcdu.rc);
    }

    /* Custom ground stabilization system (via flight loop callback) */
    ctx->ground.acf_roll_c          = XPLMFindDataRef  ("sim/aircraft/overflow/acf_roll_co");
    ctx->ground.ground_spd          = XPLMFindDataRef  ("sim/flightmodel/position/groundspeed");
    ctx->ground.idle.throttle_all   = XPLMFindDataRef  ("sim/cockpit2/engine/actuators/throttle_ratio_all");
    ctx->ground.idle.preset.command = XPLMCreateCommand("navP/thrust/idle_boost", "apply just a bit of throttle");
    if (!ctx->ground.acf_roll_c        ||
        !ctx->ground.ground_spd        ||
        !ctx->ground.idle.throttle_all ||
        !ctx->ground.idle.preset.command)
    {
        goto fail;
    }
    else
    {
        REGISTER_CHANDLER(ctx->ground.idle.preset, chandler_idleb, 0, &ctx->ground);
        ctx->bking.rc_brk.g_speed = ctx->ground.ground_spd;
    }

    /* all good */
    return ctx;

fail:
    nvp_chandlers_close((void**)&ctx);
    return NULL;
}

int nvp_chandlers_close(void **_chandler_context)
{
    chandler_context *ctx = *_chandler_context;
    *_chandler_context = NULL;
    if (!ctx)
    {
        return -1;
    }

    /* unregister all handlers… */
    UNREGSTR_CHANDLER(ctx->bking.   tur.cb);
    UNREGSTR_CHANDLER(ctx->bking.   prk.cb);
    UNREGSTR_CHANDLER(ctx->bking.   off.cb);
    UNREGSTR_CHANDLER(ctx->bking.   max.cb);
    UNREGSTR_CHANDLER(ctx->bking.   reg.cb);
    UNREGSTR_CHANDLER(ctx->spbrk.   ext.cb);
    UNREGSTR_CHANDLER(ctx->spbrk.   ret.cb);
    UNREGSTR_CHANDLER(ctx->trims.pch.up.cb);
    UNREGSTR_CHANDLER(ctx->trims.pch.dn.cb);
    UNREGSTR_CHANDLER(ctx->trims.ail.lt.cb);
    UNREGSTR_CHANDLER(ctx->trims.ail.rt.cb);
    UNREGSTR_CHANDLER(ctx->trims.rud.lt.cb);
    UNREGSTR_CHANDLER(ctx->trims.rud.rt.cb);
    UNREGSTR_CHANDLER(ctx->revrs.   fwd.cb);
    UNREGSTR_CHANDLER(ctx->revrs.   rev.cb);
    UNREGSTR_CHANDLER(ctx->assert. ap_conn);
    UNREGSTR_CHANDLER(ctx->assert. ap_disc);
    UNREGSTR_CHANDLER(ctx->assert. at_disc);
    UNREGSTR_CHANDLER(ctx->otto.   ffst.cb);
    UNREGSTR_CHANDLER(ctx->otto.   conn.cb);
    UNREGSTR_CHANDLER(ctx->otto.   disc.cb);
    UNREGSTR_CHANDLER(ctx->athr.   disc.cb);
    UNREGSTR_CHANDLER(ctx->athr.   toga.cb);
    UNREGSTR_CHANDLER(ctx->views.  prev.cb);
    UNREGSTR_CHANDLER(ctx->views.  next.cb);
    for (int i = 0; i < 10; i++)
    {
        UNREGSTR_CHANDLER(ctx->views.cbs[i]);
    }
    UNREGSTR_CHANDLER(ctx->gear.landing_gear_toggle);
    UNREGSTR_CHANDLER(ctx->gear.  landing_gear_down);
    UNREGSTR_CHANDLER(ctx->gear.    landing_gear_up);
    UNREGSTR_CHANDLER(ctx->ground.      idle.preset);
    UNREGSTR_CHANDLER(ctx->callouts.       cb_flapu);
    UNREGSTR_CHANDLER(ctx->callouts.       cb_flapd);
    UNREGSTR_CHANDLER(ctx->mcdu.                 cb);

    /* …and all datarefs */
    if (ctx->bking.rc_brk.p_b_int)
    {
        XPLMUnregisterDataAccessor(ctx->bking.rc_brk.p_b_int);
        ctx->bking.rc_brk.p_b_int = NULL;
    }
    if (ctx->bking.rc_brk.p_b_flt)
    {
        XPLMUnregisterDataAccessor(ctx->bking.rc_brk.p_b_flt);
        ctx->bking.rc_brk.p_b_flt = NULL;
    }
    if (ctx->callouts.ref_park_brake)
    {
        XPLMUnregisterDataAccessor(ctx->callouts.ref_park_brake);
        ctx->callouts.ref_park_brake = NULL;
    }
    if (ctx->callouts.ref_speedbrake)
    {
        XPLMUnregisterDataAccessor(ctx->callouts.ref_speedbrake);
        ctx->callouts.ref_speedbrake = NULL;
    }
    if (ctx->callouts.ref_flap_lever)
    {
        XPLMUnregisterDataAccessor(ctx->callouts.ref_flap_lever);
        ctx->callouts.ref_flap_lever = NULL;
    }
    if (ctx->callouts.ref_gear_lever)
    {
        XPLMUnregisterDataAccessor(ctx->callouts.ref_gear_lever);
        ctx->callouts.ref_gear_lever = NULL;
    }
    if (ref_ql_idx_get())
    {
        XPLMUnregisterDataAccessor(ref_ql_idx_get());
        ref_ql_idx_set            (NULL);
    }

    /* …and all callbacks */
    if (ctx->callouts.flc_flaps) XPLMUnregisterFlightLoopCallback(ctx->callouts.flc_flaps,   NULL);
    if (ctx->ground.flc_g)       XPLMUnregisterFlightLoopCallback(ctx->ground.flc_g, &ctx->ground);

    /* all good */
    free(ctx);
    return 0;
}

int nvp_chandlers_reset(void *inContext)
{
    chandler_context *ctx = inContext;
    XPLMDataRef d_ref;
    if (!ctx)
    {
        return -1;
    }

    /* Reset aircraft properties (type, engine count, retractable gear, etc.) */
    ctx->atyp                      = NVP_ACF_GENERIC;
    ctx->revrs.n_engines           = -1;
    ctx->gear.has_retractable_gear = -1;

    /* Don't use 3rd-party commands/datarefs until we know the plane we're in */
    ctx->acfspec.qpac.ready   = 0;
    ctx->acfspec.i733.ready   = 0;
    ctx->acfspec.x738.ready   = 0;
    ctx->gear.assert          = NULL;
    ctx->otto.ffst.dr         = NULL;
    ctx->otto.conn.cc.name    = NULL;
    ctx->otto.disc.cc.name    = NULL;
    ctx->athr.disc.cc.name    = NULL;
    ctx->athr.toga.cc.name    = NULL;
    ctx->bking.rc_brk.rg.name = NULL;
    ctx->bking.rc_brk.mx.name = NULL;
    ctx->bking.rc_brk.ro.name = NULL;
    ctx->bking.rc_brk. assert = NULL;

    /* Reset some datarefs to match X-Plane's defaults at startup */
    _DO(1, XPLMSetDatai, 1, "sim/cockpit2/radios/actuators/com1_power");
    _DO(1, XPLMSetDatai, 1, "sim/cockpit2/radios/actuators/com2_power");
    _DO(1, XPLMSetDatai, 1, "sim/cockpit2/radios/actuators/nav1_power");
    _DO(1, XPLMSetDatai, 1, "sim/cockpit2/radios/actuators/nav2_power");
    _DO(1, XPLMSetDatai, 0, "sim/graphics/misc/kill_map_fms_line");

    /* Reset turnaround-enabled flight loop callback */
    if (ctx->ground.flc_g)
    {
        XPLMUnregisterFlightLoopCallback(ctx->ground.flc_g, &ctx->ground);
        XPLMSetDataf(ctx->ground.acf_roll_c, ctx->ground.nominal_roll_c);
        ctx->ground.idle.thrott_array = NULL;
        ctx->ground.idle.minimums = 0;
        ctx->ground.flc_g = NULL;
    }

    /* all good */
    ndt_log("navP [info]: nvp_chandlers_reset OK\n"); return (ctx->initialized = 0);
}

void nvp_chandlers_setmnu(void *inContext, void *inMenu)
{
    chandler_context *ctx = inContext;
    if (ctx)
    {
        ctx->menu_context = inMenu;
    }
}

static void print_acf_info(char *xaircraft_icao_code,
                           char *xaircraft_tail_numb,
                           char *xaircraft_auth_str,
                           char *xaircraft_desc_str, char *acf_file)
{
    ndt_log("navP [info]: plane is generic (\"%s\", \"%s\", \"%s\", \"%s\", \"%s\")\n",
            xaircraft_icao_code,
            xaircraft_tail_numb,
            xaircraft_auth_str,
            xaircraft_desc_str, acf_file);
}

// some addons' datarefs have a trailing space for no reason :-(
#define STRN_CASECMP_AUTO(s1, s2) strncasecmp(s1, s2, strlen(s2))
int nvp_chandlers_update(void *inContext)
{
    XPLMDataRef dref_temporary = NULL;
    char xaircraft_auth_str[501] = "";
    char xaircraft_desc_str[261] = "";
    char xaircraft_tail_numb[41] = "";
    char xaircraft_icao_code[41] = "";
    char acf_file[257], acf_path[513];
    int  engine_type_at_idx_zero = -1;
    chandler_context *ctx = inContext;
    if (!ctx)
    {
        return -1;
    }
    if (ctx->initialized)
    {
        return 0;
    }
    ctx->initialized = 1;
    ctx->first_fcall = 1;
    ctx->kill_daniel = 1;

    /* get the aircraft path and model information */
    if ((dref_temporary = XPLMFindDataRef("sim/aircraft/view/acf_author")))
    {
        dataref_read_string(dref_temporary, xaircraft_auth_str,  sizeof(xaircraft_auth_str));
    }
    if ((dref_temporary = XPLMFindDataRef("sim/aircraft/view/acf_descrip")))
    {
        dataref_read_string(dref_temporary, xaircraft_desc_str,  sizeof(xaircraft_desc_str));
    }
    if ((dref_temporary = XPLMFindDataRef("sim/aircraft/view/acf_tailnum")))
    {
        dataref_read_string(dref_temporary, xaircraft_tail_numb, sizeof(xaircraft_tail_numb));
    }
    if ((dref_temporary = XPLMFindDataRef("sim/aircraft/view/acf_ICAO")))
    {
        dataref_read_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
    }
    XPLMGetNthAircraftModel(XPLM_USER_AIRCRAFT, acf_file, acf_path);

    /* check enabled plugins to determine which plane we're flying */
    do // dummy loop we can break out of
    {
        XPLMPluginID pluginid;
        if (XPLM_NO_PLUGIN_ID != (pluginid = XPLMFindPluginBySignature(XPLM_FF_SIGNATURE)))
        {
            ndt_log("navP [info]: plane is FlightFactor Airbus A320-214 CFM56-5B4 Ultimate\n");
            ctx->assert.plugin_id = pluginid;
            ctx->assert.initialized = 0;
            ctx->atyp = NVP_ACF_A320ULT;
            break;
        }
        if (XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("QPAC.airbus.fbw") ||
            XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("QPAC.A380.airbus.fbw"))
        {
            if (!STRN_CASECMP_AUTO(xaircraft_auth_str, "QualityPark"))
            {
                ndt_log("navP [info]: plane is QPAC Airbus A320\n");
                ctx->atyp = NVP_ACF_A320_QP;
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "A330-300"))
            {
                ndt_log("navP [info]: plane is RWDesigns Airbus A330 QPAC\n");
                ctx->atyp = NVP_ACF_A330_RW;
                break;
            }
            if (XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("FFSTSmousehandler") ||   // 1.3.x or earlier
                XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("ru.ffsts.mousehandler")) // 1.4.x or later
            {
                ndt_log("navP [info]: plane is FlightFactor Airbus A350 QPAC\n");
                ctx->atyp = NVP_ACF_A350_FF;
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "Airbus A380"))
            {
                ndt_log("navP [info]: plane is Peter Hager Airbus A380 QPAC\n");
                ctx->atyp = NVP_ACF_A380_PH;
                break;
            }
            print_acf_info(xaircraft_icao_code, xaircraft_tail_numb, xaircraft_auth_str, xaircraft_desc_str, acf_file);
            ndt_log("navP [warning]: no aircraft type match despite plugin (QPAC.airbus.fbw)\n");
            ctx->atyp = NVP_ACF_A320_QP;
            break; // still QPAC-based variant
        }
        if (XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("bs.x737.plugin"))
        {
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "Boeing 737-800"))
            {
                ndt_log("navP [info]: plane is EADT Boeing x737\n");
                ctx->atyp = NVP_ACF_B737_EA;
                break;
            }
            print_acf_info(xaircraft_icao_code, xaircraft_tail_numb, xaircraft_auth_str, xaircraft_desc_str, acf_file);
            ndt_log("navP [warning]: no aircraft type match despite plugin (bs.x737.plugin)\n");
            ctx->atyp = NVP_ACF_B737_EA;
            break; // still an x737 variant
        }
        if (XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("de-ru.philippmuenzel-den_rain.757avionics") ||
            XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature( "ru.flightfactor-steptosky.757767avionics") ||
            XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature(                  "ru.stsff.757767avionics"))
        {
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "Boeing 757"))
            {
                ndt_log("navP [info]: plane is FlightFactor Boeing 757\n");
                ctx->atyp = NVP_ACF_B757_FF;
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "Boeing 767"))
            {
                ndt_log("navP [info]: plane is FlightFactor Boeing 767\n");
                ctx->atyp = NVP_ACF_B767_FF;
                break;
            }
            print_acf_info(xaircraft_icao_code, xaircraft_tail_numb, xaircraft_auth_str, xaircraft_desc_str, acf_file);
            ndt_log("navP [warning]: no aircraft type match despite plugin (757767avionics)\n");
            ctx->atyp = NVP_ACF_B767_FF; // still a 757 or 767 variant
            break;
        }
        if (XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("de.philippmuenzel.t7avionics"))
        {
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "Boeing 777"))
            {
                ndt_log("navP [info]: plane is FlightFactor Boeing 777\n");
                ctx->atyp = NVP_ACF_B777_FF;
                break;
            }
            print_acf_info(xaircraft_icao_code, xaircraft_tail_numb, xaircraft_auth_str, xaircraft_desc_str, acf_file);
            ndt_log("navP [warning]: no aircraft type match despite plugin (t7avionics)\n");
            ctx->atyp = NVP_ACF_B777_FF; // still a T7 variant
            break;
        }
        if (XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("FJCC.SSGERJ"))
        {
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "E 170LR"))
            {
                ndt_log("navP [info]: plane is SSG Embraer E-Jet 170LR\n");
                ctx->atyp = NVP_ACF_EMBE_SS;
                break;
            }
            print_acf_info(xaircraft_icao_code, xaircraft_tail_numb, xaircraft_auth_str, xaircraft_desc_str, acf_file);
            ndt_log("navP [warning]: no aircraft type match despite plugin (SSGERJ)\n");
            ctx->atyp = NVP_ACF_EMBE_SS; // still an SSG E-Jet variant
            break;
        }
        if (XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("Rotate.MD-80.Core"))
        {
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "Rotate MD-80"))
            {
                ndt_log("navP [info]: plane is Rotate McDonnell Douglas MD-88\n");
                ctx->atyp = NVP_ACF_MD80_RO;
                break;
            }
            print_acf_info(xaircraft_icao_code, xaircraft_tail_numb, xaircraft_auth_str, xaircraft_desc_str, acf_file);
            ndt_log("navP [warning]: no aircraft type match despite plugin (MD-80.Core)\n");
            ctx->atyp = NVP_ACF_MD80_RO; // still a Rotate addon
            break;
        }
        if (XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("1-sim.sasl"))
        {
            if (!STRN_CASECMP_AUTO(xaircraft_auth_str, "JARDESIGN") &&
                !STRN_CASECMP_AUTO(xaircraft_desc_str, "Airbus A320"))
            {
                ndt_log("navP [info]: plane is J.A.R. Design Airbus A320\n");
                ctx->atyp = NVP_ACF_A320_JD;
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_auth_str, "JARDESIGN") &&
                !STRN_CASECMP_AUTO(xaircraft_desc_str, "Airbus A330"))
            {
                ndt_log("navP [info]: plane is J.A.R. Design Airbus A330\n");
                ctx->atyp = NVP_ACF_A330_JD;
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_auth_str, "JARDESIGN"))
            {
                ndt_log("navP [info]: plane is J.A.R. Design (unknown model)\n");
                ctx->atyp = NVP_ACF_A320_JD;
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_auth_str, "FlyJsim") &&
                !strcasecmp       (xaircraft_icao_code, "B732"))
            {
                ndt_log("navP [info]: plane is FlyJSim Boeing 737-200\n");
                ctx->atyp = NVP_ACF_B737_FJ;
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "Dash 8 Q400"))
            {
                ndt_log("navP [info]: plane is FlyJSim Bombardier Dash 8 Q400\n");
                ctx->atyp = NVP_ACF_DH8D_FJ;
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_auth_str, "Daniel Klaue") &&
                !STRN_CASECMP_AUTO(xaircraft_desc_str, "ERJ-140"))
            {
                ndt_log("navP [info]: plane is Daniel Klaue ERJ-140\n");
                ctx->atyp = NVP_ACF_ERJ1_4D;
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_auth_str, "Marko Mamula") &&
                !STRN_CASECMP_AUTO(xaircraft_desc_str, "Embraer E175"))
            {
                ndt_log("navP [info]: plane is X-Crafts Embraer E-Jet 175\n");
                ctx->atyp = NVP_ACF_EMBE_XC;
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_auth_str, "Marko Mamula") &&
                !STRN_CASECMP_AUTO(xaircraft_desc_str, "Embraer ERJ-195LR"))
            {
                ndt_log("navP [info]: plane is X-Crafts Embraer E-Jet 195LR\n");
                ctx->atyp = NVP_ACF_GENERIC; // not all functionality of 175 yet
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "Sukhoy SuperJet"))
            {
                ndt_log("navP [info]: plane is Ramzzess Sukhoi Superjet 100-95LR\n");
                ctx->atyp = NVP_ACF_SSJ1_RZ;
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_auth_str, "Rob Wilson") &&
                !STRN_CASECMP_AUTO(xaircraft_desc_str, "Hawker 4000"))
            {
                ndt_log("navP [info]: plane is RWDesigns Hawker 4000\n");
                ctx->atyp = NVP_ACF_HA4T_RW;
                break;
            }
            // fall through
        }
        if (XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("gizmo.x-plugins.com"))
        {
            if (!STRN_CASECMP_AUTO(xaircraft_auth_str, "IXEG") &&
                !STRN_CASECMP_AUTO(xaircraft_desc_str, "Boeing 737-300"))
            {
                ndt_log("navP [info]: plane is IXEG Boeing 737-300\n");
                ctx->atyp = NVP_ACF_B737_XG;
                break;
            }
            // fall through
        }
        break; // no match, fall back to generic
    }
    while (0);

    /* we don't necessarily know what this aircraft may be yet */
    if (ctx->atyp == NVP_ACF_GENERIC)
    {
        print_acf_info(xaircraft_icao_code, xaircraft_tail_numb,
                       xaircraft_auth_str,  xaircraft_desc_str, acf_file);
    }

    /* ICAO type designator (for use by e.g. some of the flap callouts) */
    switch (ctx->atyp)
    {
        case NVP_ACF_A330_RW:
            sprintf                            (xaircraft_icao_code, "%.4s", "A333");
            dataref_wrte_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
            break;

        case NVP_ACF_A350_FF:
            sprintf                            (xaircraft_icao_code, "%.4s", "A359");
            dataref_wrte_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
            break;

        case NVP_ACF_HA4T_RW:
            sprintf                            (xaircraft_icao_code, "%.4s", "HA4T");
            dataref_wrte_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
            break;

        case NVP_ACF_SSJ1_RZ:
            sprintf                            (xaircraft_icao_code, "%.4s", "SU95");
            dataref_wrte_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
            break;

        case NVP_ACF_GENERIC:
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "A-10 Warthog"))
            {
                sprintf                            (xaircraft_icao_code, "%.4s", "A10");
                dataref_wrte_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "Lisa Airplanes Akoya"))
            {
                sprintf                            (xaircraft_icao_code, "%.4s", "AKOY");
                dataref_wrte_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "Mother Ship 1"))
            {
                sprintf                            (xaircraft_icao_code, "%.4s", "B52");
                dataref_wrte_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "Boeing 787"))
            {
                sprintf                            (xaircraft_icao_code, "%.4s", "B788");
                dataref_wrte_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "Bonanza V35B"))
            {
                sprintf                            (xaircraft_icao_code, "%.4s", "BE35");
                dataref_wrte_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "King Air C90"))
            {
                sprintf                            (xaircraft_icao_code, "%.4s", "BE9L");
                dataref_wrte_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "KC10"))
            {
                sprintf                            (xaircraft_icao_code, "%.4s", "DC10");
                dataref_wrte_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "Epic Victory"))
            {
                sprintf                            (xaircraft_icao_code, "%.4s", "EVIC");
                dataref_wrte_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_auth_str, "After") && !strnlen(xaircraft_desc_str, 1))
            {
                sprintf                            (xaircraft_icao_code, "%.4s", "FA7X");
                dataref_wrte_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "Ferrari"))
            {
                sprintf                            (xaircraft_icao_code, "%.4s", "P180");
                dataref_wrte_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "cirrus jet"))
            {
                sprintf                            (xaircraft_icao_code, "%.4s", "SF50");
                dataref_wrte_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
                break;
            }
            if (!STRN_CASECMP_AUTO(xaircraft_desc_str, "TBM 850"))
            {
                sprintf                            (xaircraft_icao_code, "%.4s", "TBM8");
                dataref_wrte_string(dref_temporary, xaircraft_icao_code, sizeof(xaircraft_icao_code));
                break;
            }
            // fall through
        default:
            break;
    }
    snprintf(ctx->icao, sizeof(ctx->icao), "%.4s", xaircraft_icao_code);
    snprintf(ctx->auth, sizeof(ctx->auth), "%.40s", xaircraft_auth_str);
    snprintf(ctx->desc, sizeof(ctx->desc), "%.40s", xaircraft_desc_str);
    ndt_log ("navP [info]: ICAO type: \"%.4s\"\n", xaircraft_icao_code);

    /* plane-specific braking ratios */
    if (XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("com.simcoders.rep"))
    {
        ctx->bking.rc_brk.rtio[0] = 0.5f;
        ctx->bking.rc_brk.rtio[1] = .75f;
        ctx->bking.rc_brk.rtio[2] = 1.0f;
    }
    else if (ctx->atyp == NVP_ACF_A320ULT)
    {
        ctx->bking.rc_brk.rtio[0] = 0.2f;
        ctx->bking.rc_brk.rtio[1] = 0.4f;
        ctx->bking.rc_brk.rtio[2] = 0.6f;
    }
    else // default values
    {
        ctx->bking.rc_brk.rtio[0] = 1.0f / 3.0f;
        ctx->bking.rc_brk.rtio[1] = 2.0f / 3.0f;
        ctx->bking.rc_brk.rtio[2] = 1.0f;
    }

    /* determine engine count and primary engine type */
    if ((ctx->revrs.n_engines = XPLMGetDatai(ctx->revrs.acf_numng)))
    {
        if (ctx->revrs.n_engines > 8) ctx->revrs.n_engines = 8;
        if (ctx->revrs.n_engines < 0) ctx->revrs.n_engines = 0;
    }
    if (ctx->revrs.n_engines > 0)
    {
        int t[8]; XPLMGetDatavi(ctx->revrs.acf_ngtyp, t, 0, ctx->revrs.n_engines);
        for (int i = 1; i < ctx->revrs.n_engines; i++)
        {
            if (t[i] != t[0])
            {
                ctx->revrs.n_engines = i; break; // additional dummy engines, used by e.g. Carenado
            }
        }
        engine_type_at_idx_zero = t[0];
    }

    /* plane-specific custom commands for automation disconnects, if any */
    switch (ctx->atyp)
    {
        case NVP_ACF_A320ULT:
            break; // TODO

        case NVP_ACF_A320_JD:
        case NVP_ACF_A330_JD:
            ctx->otto.disc.cc.name = "sim/autopilot/fdir_servos_down_one";
            ctx->athr.disc.cc.name = "sim/autopilot/autothrottle_off";
/*untested*/ctx->otto.conn.cc.name = "sim/autopilot/servos_on";
            break;

        case NVP_ACF_A320_QP:
        case NVP_ACF_A330_RW:
        case NVP_ACF_A350_FF:
        case NVP_ACF_A380_PH:
            ctx->otto.disc.cc.name = "sim/autopilot/fdir_servos_down_one";
            ctx->athr.disc.cc.name = "sim/autopilot/autothrottle_off";
            ctx->otto.conn.cc.name = "airbus_qpac/ap1_push";
            break;

        case NVP_ACF_B737_EA:
            ctx->otto.disc.cc.name = "x737/yoke/capt_AP_DISENG_BTN";
            ctx->athr.disc.cc.name = "x737/mcp/ATHR_ARM_TOGGLE";
            ctx->athr.toga.cc.name = "x737/mcp/TOGA_TOGGLE";
            ctx->otto.conn.cc.name = "x737/mcp/CMDA_TOGGLE";
            break;

        case NVP_ACF_B737_XG:
            ctx->otto.conn.cc.name = "ixeg/733/autopilot/AP_A_cmd_toggle";
            ctx->otto.disc.cc.name = "ixeg/733/autopilot/AP_disengage";
            ctx->athr.disc.cc.name = "ixeg/733/autopilot/at_disengage";
            ctx->athr.toga.cc.name = "sim/engines/TOGA_power";
            break;

        case NVP_ACF_B757_FF:
        case NVP_ACF_B767_FF:
            ctx->otto.conn.cc.name    = "private/ffsts/ap_cmdl";
            ctx->otto.disc.cc.name    = "1-sim/comm/AP/ap_disc";
            ctx->athr.disc.cc.name    = "1-sim/comm/AP/at_disc";
            ctx->athr.toga.cc.name    = "1-sim/comm/AP/at_toga";
            ctx->bking.rc_brk.rg.name =  "1-sim/brakes_regular";
            ctx->bking.rc_brk.mx.name =      "1-sim/brakes_max";
            break;

        case NVP_ACF_B777_FF:
            ctx->otto.disc.cc.name    =          "777/ap_disc";
            ctx->athr.disc.cc.name    =          "777/at_disc";
            ctx->athr.toga.cc.name    =          "777/at_toga";
            ctx->bking.rc_brk.mx.name =     "1-sim/brakes_max";
            ctx->bking.rc_brk.rg.name = "1-sim/brakes_regular";
            break;

        case NVP_ACF_EMBE_SS:
            ctx->otto.conn.cc.name = "SSG/EJET/MCP/AP_COMM";
            ctx->otto.disc.cc.name = "SSG/EJET/MCP/AP_COMM";
            ctx->athr.disc.cc.name = "SSG/EJET/MCP/AT_COMM";
            ctx->athr.toga.cc.name = "SSG/EJET/MCP/Toga";
            break;

        case NVP_ACF_EMBE_XC:
        case NVP_ACF_ERJ1_4D:
        case NVP_ACF_HA4T_RW:
        case NVP_ACF_SSJ1_RZ:
            ctx->otto.disc.cc.name = "sim/autopilot/fdir_servos_down_one";
            ctx->athr.disc.cc.name = "sim/autopilot/autothrottle_off";
            ctx->athr.toga.cc.name = "sim/autopilot/autothrottle_on";
            ctx->otto.conn.cc.name = "sim/autopilot/servos_on";
            break;

        case NVP_ACF_MD80_RO:
            if ((ctx->bking.rc_brk.protate = XPLMFindDataRef("Rotate/md80/systems/parking_brake_toggle_clicked")))
            {
                // special case: no need for deferred initialization with this command
                (ctx->bking.rc_brk.ro.name = "Rotate/md80/systems/parking_brake_toggle");
                (ctx->bking.rc_brk.ro.xpcr = XPLMFindCommand(ctx->bking.rc_brk.ro.name));
                if (ctx->bking.rc_brk.ro.xpcr == NULL)
                {
                    ctx->bking.rc_brk.ro.name  = NULL;
                }
            }
            ctx->athr.toga.cc.name = "Rotate/md80/autopilot/to_ga_button";
            ctx->athr.disc.cc.name = "Rotate/md80/autopilot/at_disc";
            ctx->otto.disc.cc.name = "Rotate/md80/autopilot/ap_disc";
            ctx->otto.conn.cc.name = "sim/autopilot/servos_on";
            break;

        case NVP_ACF_B737_FJ:
            ctx->otto.disc.cc.name = "sim/autopilot/fdir_servos_down_one";
            ctx->otto.conn.cc.name = "sim/autopilot/servos_on";
            break;

        case NVP_ACF_GENERIC:
            {
                switch (engine_type_at_idx_zero)
                {
                    case 4: case 5: // twin turbojet/turbofan
                        if (ctx->revrs.n_engines >= 2)
                        {
                            ctx->athr.disc.cc.name = "sim/autopilot/autothrottle_off";
                            ctx->athr.toga.cc.name = "sim/autopilot/autothrottle_on";
                            break;
                        }
                    default:
                        break;
                }
            }
            ctx->otto.disc.cc.name = "sim/autopilot/fdir_servos_down_one";
            ctx->otto.conn.cc.name = "sim/autopilot/servos_on";
            break;

        default: // not generic but no usable commands
            break;
    }

    /*
     * Reset MCDU pop-up handler status.
     *
     * Also pre-emptively disable plugin FMSes for all aircraft except
     * x737; if required, they will be re-enabled later automatically.
     *
     * Priority order for x737: x737FMC, then SimpleFMC, then X-FMC.
     */
    XPLMPluginID sfmc = XPLMFindPluginBySignature("pikitanga.xplane10.SimpleFMC");
    XPLMPluginID x737 = XPLMFindPluginBySignature("FJCC.x737FMC");
    XPLMPluginID xfmc = XPLMFindPluginBySignature("x-fmc.com");
    if (x737 != XPLM_NO_PLUGIN_ID)
    {
        if (ctx->atyp == NVP_ACF_B737_EA)
        {
            if (XPLMIsPluginEnabled(x737) == 0)
            {
                XPLMEnablePlugin(x737);
            }
        }
        else
        {
            if (XPLMIsPluginEnabled(x737))
            {
                XPLMDisablePlugin(x737);
            }
        }
    }
    if (sfmc != XPLM_NO_PLUGIN_ID)
    {
        if (ctx->atyp == NVP_ACF_B737_EA &&
            x737      == XPLM_NO_PLUGIN_ID)
        {
            if (XPLMIsPluginEnabled(sfmc) == 0)
            {
                XPLMEnablePlugin(sfmc);
            }
        }
        else
        {
            if (XPLMIsPluginEnabled(sfmc))
            {
                XPLMDisablePlugin(sfmc);
            }
        }
    }
    if (xfmc != XPLM_NO_PLUGIN_ID)
    {
        if (ctx->atyp == NVP_ACF_B737_EA   &&
            sfmc      == XPLM_NO_PLUGIN_ID &&
            x737      == XPLM_NO_PLUGIN_ID)
        {
            if (XPLMIsPluginEnabled(xfmc) == 0)
            {
                XPLMEnablePlugin(xfmc);
            }
        }
        else
        {
            if (XPLMIsPluginEnabled(xfmc))
            {
                XPLMDisablePlugin(xfmc);
            }
        }
    }
    ctx->mcdu.rc.i_disabled = -1;
    ctx->mcdu.rc.auth = ctx->auth;
    ctx->mcdu.rc.desc = ctx->desc;
    ctx->mcdu.rc.atyp = ctx->atyp;

    /* for the gear handle callouts */
    ctx->gear.callouts.atype = ctx->atyp;

    /* detect presence of PilotEdge */
    if (XPLM_NO_PLUGIN_ID != (ctx->volumes.pe = XPLMFindPluginBySignature("com.pilotedge.plugin.xplane")))
    {
        ctx->volumes.pe1 = XPLMFindDataRef("pilotedge/status/connected");
    }

    /* Custom ground stabilization system (via flight loop callback) */
    ctx->ground.nominal_roll_c = XPLMGetDataf(ctx->ground.acf_roll_c);

    /* all good */
    ndt_log("navP [info]: nvp_chandlers_update OK\n"); XPLMSpeakString("nav P configured"); return 0;
}

static int dataref_read_string(XPLMDataRef dataref, char *string_buffer, size_t buffer_size)
{
    if (dataref && string_buffer && buffer_size)
    {
        int len = XPLMGetDatab(dataref, string_buffer, 0, buffer_size - 1);
        if (len > 0)
        {
            string_buffer[buffer_size - 1] = '\0';
            return 0;
        }
        string_buffer[0] = '\0';
        return -1;
    }
    return ENOMEM;
}

static int dataref_wrte_string(XPLMDataRef dataref, char *string_buffer, size_t buffer_size)
{
    if (dataref && string_buffer && buffer_size)
    {
        size_t str_len = strnlen(string_buffer, buffer_size - 2);
        XPLMSetDatab(dataref, string_buffer, 0, str_len);
        XPLMSetDatab(dataref, "", str_len, 1);
    }
    return ENOMEM;
}

/*
 * action: set an aircraft's turnaround state to a pilot-friendly cold & dark variant.
 */
static int chandler_turna(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    chandler_context *ctx = inRefcon;
    if (inPhase == xplm_CommandEnd)
    {
        int speak = XPLMGetDatai(ctx->callouts.ref_park_brake);
        /* this can happen after calling XPLMReloadPlugins() */
        if (ctx->initialized == 0)
        {
            nvp_chandlers_reset (inRefcon);
            nvp_chandlers_update(inRefcon);
        }
        /*
         * Do any additional aircraft-specific stuff that can't be done earlier.
         */
        if (ctx->atyp & NVP_ACF_MASK_QPC)
        {
            if (ctx->acfspec.qpac.ready == 0)
            {
                aibus_fbw_init(&ctx->acfspec.qpac);
            }
        }
        if (ctx->first_fcall)
        {
            // if set to automatic, callouts become enabled on first turnaround
            if (XPLMGetDatai(ctx->callouts.ref_park_brake) == -1)
            {
                XPLMSetDatai(ctx->callouts.ref_park_brake, (speak = 1));
            }
            if (XPLMGetDatai(ctx->callouts.ref_speedbrake) == -1)
            {
                XPLMSetDatai(ctx->callouts.ref_speedbrake,     1);
            }
            if (XPLMGetDatai(ctx->callouts.ref_flap_lever) == -1)
            {
                XPLMSetDatai(ctx->callouts.ref_flap_lever,     1);
            }
            if (XPLMGetDatai(ctx->callouts.ref_gear_lever) == -1)
            {
                XPLMSetDatai(ctx->callouts.ref_gear_lever,     1);
            }
            if (first_fcall_do(ctx) == 0 && ctx->first_fcall == 0)
            {
                if ((ctx->atyp & NVP_ACF_MASK_JDN) == 0)
                {
                    XPLMSpeakString("turn around set");
                }
                XPLMCommandOnce(ctx->views.cbs[1].command);
            }
            XPLMSetDatai    (ctx->callouts.ref_park_brake, 0);
            XPLMCommandOnce (ctx->      bking.prk.cb.command);
            XPLMSetDatai(ctx->callouts.ref_park_brake, speak);
        }
    }
    return 0;
}

/*
 * action: set or unset parking brake.
 *
 * rationale: X-Plane only has a toggle for this :(
 */
static int chandler_p_max(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        chandler_context *ctx = inRefcon;
        refcon_braking   *rcb = &ctx->bking.rc_brk;
        refcon_assert1   *rca = rcb->assert;
        int speak = XPLMGetDatai(ctx->callouts.ref_park_brake);
        if (ctx->atyp & NVP_ACF_MASK_JDN)
        {
            speak = 0;
        }
        if (ctx->atyp & NVP_ACF_MASK_QPC)
        {
            if (ctx->acfspec.qpac.ready == 0)
            {
                aibus_fbw_init(&ctx->acfspec.qpac);
            }
            if (ctx->acfspec.qpac.ready)
            {
                // the FlightFactor-QPAC A350 has its parking brake dataref inverted
                XPLMSetDatai(ctx->acfspec.qpac.pkb_ref, (ctx->atyp != NVP_ACF_A350_FF));
                XPLMSetDatai(rcb->p_b_int,              (ctx->atyp != NVP_ACF_A350_FF));
                if (speak > 0) XPLMSpeakString("park brake set");
            }
            return 0;
        }
        if (ctx->volumes.pe != XPLM_NO_PLUGIN_ID && ctx->volumes.pe1)
        {
            if (XPLMGetDatai(ctx->volumes.pe1) == 1)
            {
                XPLMSetDataf(ctx->volumes.atc, 1.0f);
                XPLMSetDatai(ctx->volumes.snd,    1);
            }
        }
        if (rcb->ro.name)
        {
            if (XPLMGetDatai(rcb->protate) != 1)
            {
                XPLMCommandOnce(rcb->ro.xpcr);
            }
        }
        else if (rca)
        {
            if (XPLMGetDataf(rcb->p_b_rat) < 0.5f)
            {
                XPLMCommandOnce(rca->dat.p_brk_toggle);
            }
        }
        else
        {
            XPLMSetDataf(rcb->p_b_rat, 1.0f);
            XPLMSetDataf(rcb->p_b_flt, 1.0f);
        }
        if (speak > 0) XPLMSpeakString("park brake set");
    }
    return 0;
}

static int chandler_p_off(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        chandler_context *ctx = inRefcon;
        refcon_braking   *rcb = &ctx->bking.rc_brk;
        refcon_assert1   *rca = rcb->assert;
        int speak = XPLMGetDatai(ctx->callouts.ref_park_brake);
        if (ctx->atyp & NVP_ACF_MASK_JDN)
        {
            speak = 0;
        }
        if (ctx->atyp & NVP_ACF_MASK_QPC)
        {
            if (ctx->acfspec.qpac.ready == 0)
            {
                aibus_fbw_init(&ctx->acfspec.qpac);
            }
            if (ctx->acfspec.qpac.ready)
            {
                // the FlightFactor-QPAC A350 has its parking brake dataref inverted
                XPLMSetDatai(ctx->acfspec.qpac.pkb_ref, (ctx->atyp == NVP_ACF_A350_FF));
                XPLMSetDatai(rcb->p_b_int,              (ctx->atyp == NVP_ACF_A350_FF));
                if (speak > 0) XPLMSpeakString("park brake released");
            }
            return 0;
        }
        if (ctx->volumes.pe != XPLM_NO_PLUGIN_ID && ctx->volumes.pe1)
        {
            if (XPLMGetDatai(ctx->volumes.pe1) == 1)
            {
                XPLMSetDataf(ctx->volumes.atc, 1.0f);
            }
        }
        if (rcb->ro.name)
        {
            if (XPLMGetDatai(rcb->protate) != 0)
            {
                XPLMCommandOnce(rcb->ro.xpcr);
            }
        }
        else if (rca)
        {
            if (XPLMGetDataf(rcb->p_b_rat) > 0.5f)
            {
                XPLMCommandOnce(rca->dat.p_brk_toggle);
            }
        }
        else
        {
            XPLMSetDataf(rcb->p_b_rat, 0.0f);
            XPLMSetDataf(rcb->p_b_flt, 0.0f);
        }
        if (speak > 0) XPLMSpeakString("park brake released");
    }
    return 0;
}

/*
 * action: apply symmetrical L/R braking w/out applying the parking brake.
 *
 * rationale: same braking effect, but no interference with parking brake
 *            operation (practical, one can set or unset said brake while
 *            simultaneously applying brake pressure).
 */
static int chandler_b_max(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    float p_ratio = 1.0f;
    chandler_context *ctx = inRefcon;
    refcon_braking *rcb = &ctx->bking.rc_brk;
    refcon_assert1 *rca = rcb->assert;
    if (rca)
    {
        if (inPhase != xplm_CommandEnd)
        {
            rca->api.ValueSet(rca->dat.id_flt_pedal_brake_left, &p_ratio);
            rca->api.ValueSet(rca->dat.id_flt_pedal_brake_rigt, &p_ratio);
        } // else { auto-reset; }
        return 0;
    }
    if (ctx->atyp & NVP_ACF_MASK_QPC)
    {
        if (ctx->acfspec.qpac.ready == 0)
        {
            aibus_fbw_init(&ctx->acfspec.qpac);
        }
        if (ctx->acfspec.qpac.ready)
        {
            switch (inPhase)
            {
                case xplm_CommandBegin:
                    XPLMSetDatai(rcb->p_b_int, XPLMGetDatai(ctx->acfspec.qpac.pkb_ref));
                    if (ctx->acfspec.qpac.h_b_max)
                    {
                        XPLMCommandBegin(ctx->acfspec.qpac.h_b_max);
                        break;
                    }
                    XPLMSetDatai(ctx->acfspec.qpac.pkb_ref, 1); // use parking brake directly
                    break;
                case xplm_CommandEnd:
                    if (ctx->acfspec.qpac.h_b_max)
                    {
                        XPLMCommandEnd(ctx->acfspec.qpac.h_b_max);
                    }
                    XPLMSetDatai(ctx->acfspec.qpac.pkb_ref, XPLMGetDatai(rcb->p_b_int));
                    break;
                default:
                    break;
            }
        }
        return 0;
    }
    else
    {
        if (rcb->mx.name)
        {
            if (rcb->mx.xpcr == NULL)
            {
                rcb->mx.xpcr = XPLMFindCommand(rcb->mx.name);
            }
        }
        else
        {
            rcb->mx.xpcr = NULL;
        }
    }
    switch (inPhase)
    {
        case xplm_CommandBegin:
            if (rcb->mx.xpcr)
            {
                XPLMCommandBegin(rcb->mx.xpcr);
                break;
            }
            XPLMSetDataf(rcb->p_b_rat, p_ratio);
            break;
        case xplm_CommandContinue:
            if (rcb->mx.xpcr)
            {
                break;
            }
            XPLMSetDataf(rcb->p_b_rat, p_ratio);
            break;
        default: // xplm_CommandEnd
            if (rcb->mx.xpcr)
            {
                XPLMCommandEnd(rcb->mx.xpcr);
            }
            XPLMSetDataf(rcb->p_b_rat, XPLMGetDataf(rcb->p_b_flt));
            break;
    }
    return 0;
}

static int chandler_b_reg(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    chandler_context *ctx = inRefcon;
    refcon_braking *rcb = &ctx->bking.rc_brk;
    float p_b_flt = XPLMGetDataf(rcb->p_b_flt);
    float g_speed = XPLMGetDataf(rcb->g_speed) * 3.6f / 1.852f;
    float p_ratio = (g_speed < 30.0f) ? rcb->rtio[0] : (g_speed < 60.0f) ? rcb->rtio[1] : rcb->rtio[2];
    refcon_assert1 *rca = rcb->assert;
    if (rca)
    {
        if (inPhase != xplm_CommandEnd)
        {
            rca->api.ValueSet(rca->dat.id_flt_pedal_brake_left, &p_ratio);
            rca->api.ValueSet(rca->dat.id_flt_pedal_brake_rigt, &p_ratio);
        } // else { auto-reset; }
        return 0;
    }
    if (p_ratio < p_b_flt)
    {
        p_ratio = p_b_flt;
    }
    if (ctx->atyp & NVP_ACF_MASK_QPC)
    {
        if (ctx->acfspec.qpac.ready == 0)
        {
            aibus_fbw_init(&ctx->acfspec.qpac);
        }
        if (ctx->acfspec.qpac.ready)
        {
            switch (inPhase)
            {
                case xplm_CommandBegin:
                    XPLMSetDatai(rcb->p_b_int, XPLMGetDatai(ctx->acfspec.qpac.pkb_ref));
                    if (ctx->acfspec.qpac.h_b_reg)
                    {
                        XPLMCommandBegin(ctx->acfspec.qpac.h_b_reg);
                        break;
                    }
                    XPLMSetDatai(ctx->acfspec.qpac.pkb_ref, 1); // use parking brake directly
                    break;
                case xplm_CommandEnd:
                    if (ctx->acfspec.qpac.h_b_reg)
                    {
                        XPLMCommandEnd(ctx->acfspec.qpac.h_b_reg);
                    }
                    XPLMSetDatai(ctx->acfspec.qpac.pkb_ref, XPLMGetDatai(rcb->p_b_int));
                    break;
                default:
                    break;
            }
        }
        return 0;
    }
    else
    {
        if (rcb->rg.name)
        {
            if (rcb->rg.xpcr == NULL)
            {
                rcb->rg.xpcr = XPLMFindCommand(rcb->rg.name);
            }
        }
        else
        {
            rcb->rg.xpcr = NULL;
        }
    }
    switch (inPhase)
    {
        case xplm_CommandBegin:
            if (rcb->rg.xpcr)
            {
                XPLMCommandBegin(rcb->rg.xpcr);
                break;
            }
            XPLMSetDataf(rcb->p_b_rat, p_ratio);
            break;
        case xplm_CommandContinue:
            if (rcb->rg.xpcr)
            {
                break;
            }
            XPLMSetDataf(rcb->p_b_rat, p_ratio);
            break;
        default: // xplm_CommandEnd
            if (rcb->rg.xpcr)
            {
                XPLMCommandEnd(rcb->rg.xpcr);
            }
            XPLMSetDataf(rcb->p_b_rat, XPLMGetDataf(rcb->p_b_flt));
            break;
    }
    return 0;
}

/*
 * Default speedbrake extend/retract command handlers. For most planes, we just
 * pass the default command through, but a few addons require special handling.
 */
static int chandler_sp_ex(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        refcon_ixeg733   *i33 = NULL;
        refcon_eadt738   *x38 = NULL;
        chandler_context *ctx = inRefcon;
        int speak = XPLMGetDatai(ctx->callouts.ref_speedbrake);
        if (ctx->atyp == NVP_ACF_HA4T_RW)
        {
            if (ctx->spbrk.ha4t == NULL)
            {
                ctx->spbrk.ha4t = XPLMFindDataRef("Hawker4000/control/speedbrake_b");
            }
        }
        else
        {
            ctx->spbrk.ha4t = NULL;
        }
        switch (ctx->atyp)
        {
            case NVP_ACF_A320ULT:
            {
                float current_ratio = XPLMGetDataf(ctx->spbrk.srat);
                if (current_ratio < 0.49f)
                {
                    // spoilers armed, disarm but don't extend
                    {
                        XPLMSetDataf(ctx->spbrk.srat, 0.5f);
                    }
                    if (XPLMGetDataf(ctx->spbrk.srat) > 0.49f)
                    {
                        if (speak > 0) XPLMSpeakString("spoilers disarmed");
                    }
                    return 0;
                }
                if (current_ratio < 0.88f)
                {
                    {
                        XPLMSetDataf(ctx->spbrk.srat, current_ratio + 0.125f);
                    }
                    if (XPLMGetDataf(ctx->spbrk.srat) > 0.51f)
                    {
                        if (speak > 0) XPLMSpeakString("speedbrake");
                    }
                    return 0;
                }
                return 0;
            }

            case NVP_ACF_B737_EA:
                x38 = &ctx->acfspec.x738;
                if (x38->ready == 0)
                {
                    boing_738_init(x38);
                }
                break;

            case NVP_ACF_B737_XG:
                i33 = &ctx->acfspec.i733;
                if (i33->ready == 0)
                {
                    boing_733_init(i33);
                }
                break;

            case NVP_ACF_A320_JD:
            case NVP_ACF_A330_JD:
                speak = 0; // fall through
            default:
                if (ctx->spbrk.ha4t && XPLMGetDatai(ctx->spbrk.ha4t))
                {
                    // spoilers armed, disarm but don't extend
                    if (speak > 0) XPLMSpeakString("spoilers disarmed");
                    XPLMSetDatai(ctx->spbrk.ha4t, 0); return 0;
                }
//              else
                {
                    XPLMCommandOnce(ctx->spbrk.sext);
                }
                if (XPLMGetDataf(ctx->spbrk.srat) < +.01f)
                {
                    if (speak > 0) XPLMSpeakString("spoilers disarmed");
                    return 0;
                }
                if (XPLMGetDataf(ctx->spbrk.srat) > +.01f)
                {
                    if (speak > 0) XPLMSpeakString("speedbrake");
                    return 0;
                }
                return 0;
        }
        {
            float ratio = XPLMGetDataf(ctx->spbrk.srat);
            if (i33 && i33->ready)
            {
                if (ratio < -.01f)
                {
                    XPLMSetDataf(i33->slat, 0.0f);    // armed: retract fully
                    if (speak > 0) XPLMSpeakString("spoilers disarmed");
                    return 0;
                }
                XPLMSetDataf(i33->slat, 0.8f);        // extend: flight detent
                if (speak > 0) XPLMSpeakString("speedbrake");
                return 0;
            }
            if (x38 && x38->ready)
            {
                if (ratio > .1f && ratio < .2f)
                {
                    XPLMCommandOnce(x38->spret);      // extend: disarm spoilers
                    if (speak > 0) XPLMSpeakString("spoilers disarmed");
                    return 0;
                }
                XPLMCommandOnce(ctx->spbrk.sext);     // extend: one
                if (speak > 0) XPLMSpeakString("speedbrake");
                return 0;
            }
        }
    }
    return 0;
}

static int chandler_sp_re(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        refcon_ixeg733   *i33 = NULL;
        refcon_eadt738   *x38 = NULL;
        chandler_context *ctx = inRefcon;
        int speak = XPLMGetDatai(ctx->callouts.ref_speedbrake);
        if (ctx->atyp == NVP_ACF_HA4T_RW)
        {
            if (ctx->spbrk.ha4t == NULL)
            {
                ctx->spbrk.ha4t = XPLMFindDataRef("Hawker4000/control/speedbrake_b");
            }
        }
        else
        {
            ctx->spbrk.ha4t = NULL;
        }
        switch (ctx->atyp)
        {
            case NVP_ACF_A320ULT:
            {
                float current_ratio = XPLMGetDataf(ctx->spbrk.srat);
                if (current_ratio < 0.51f)
                {
                    // already retracted: arm spoilers
                    {
                        XPLMSetDataf(ctx->spbrk.srat, 0.0f);
                    }
                    if (XPLMGetDataf(ctx->spbrk.srat) < 0.01f)
                    {
                        if (speak > 0) XPLMSpeakString("spoilers armed");
                    }
                    return 0;
                }
                if (current_ratio > 0.62f)
                {
                    {
                        XPLMSetDataf(ctx->spbrk.srat, current_ratio - 0.125f);
                    }
                    if (XPLMGetDataf(ctx->spbrk.srat) < 0.51f)
                    {
                        if (speak > 0) XPLMSpeakString("speedbrake retracted");
                    }
                    return 0;
                }
                return 0;
            }

            case NVP_ACF_B737_EA:
                x38 = &ctx->acfspec.x738;
                if (x38->ready == 0)
                {
                    boing_738_init(x38);
                }
                break;

            case NVP_ACF_B737_XG:
                i33 = &ctx->acfspec.i733;
                if (i33->ready == 0)
                {
                    boing_733_init(i33);
                }
                break;

            case NVP_ACF_A320_JD:
            case NVP_ACF_A330_JD:
                speak = 0; // fall through
            default:
                if (ctx->atyp == NVP_ACF_EMBE_SS &&
                    XPLMGetDataf(ctx->spbrk.srat) < +.01f)
                {
                    // already retracted, we can't/needn't arm (automatic)
                    if (speak > 0) XPLMSpeakString("speedbrake retracted");
                    return 0;
                }
                if (ctx->spbrk.ha4t && XPLMGetDataf(ctx->spbrk.srat) < +.01f)
                {
                    // already retracted, arm spoilers
                    if (speak > 0) XPLMSpeakString("spoilers armed");
                    XPLMSetDatai(ctx->spbrk.ha4t, 1); return 0;
                }
//              else
                {
                    XPLMCommandOnce(ctx->spbrk.sret);
                }
                if (XPLMGetDataf(ctx->spbrk.srat) < -.01f)
                {
                    if (speak > 0) XPLMSpeakString("spoilers armed");
                    return 0;
                }
                if (XPLMGetDataf(ctx->spbrk.srat) < +.01f)
                {
                    if (speak > 0) XPLMSpeakString("speedbrake retracted");
                    return 0;
                }
                return 0;
        }
        {
            float ratio = XPLMGetDataf(ctx->spbrk.srat);
            if (i33 && i33->ready)
            {
                if (ratio < +.01f)
                {
                    if (ratio > -.01f)
                    {
                        XPLMSetDataf(i33->slat, .15f);// retract: arm spoilers
                    }
                    if (speak > 0) XPLMSpeakString("spoilers armed");
                    return 0;
                }
                if (ratio > +.51f)
                {
                    XPLMSetDataf(i33->slat, 0.8f);    // retract: flight detent
                    return 0;
                }
                XPLMSetDataf(i33->slat, 0.0f);        // retract: fully
                if (speak > 0) XPLMSpeakString("speedbrake retracted");
                return 0;
            }
            if (x38 && x38->ready)
            {
                if ((ratio < .01f) || (ratio > .1f && ratio < .2f))
                {
                    if (ratio < .01f)
                    {
                        XPLMCommandOnce(x38->sparm);  // retract: arm spoilers
                    }
                    if (speak > 0) XPLMSpeakString("spoilers armed");
                    return 0;
                }
                XPLMCommandOnce (ctx->spbrk.sret);    // retract: one
                if (XPLMGetDataf(ctx->spbrk.srat) < .01f)
                {
                    if (speak > 0) XPLMSpeakString("speedbrake retracted");
                }
                return 0;
            }
        }
    }
    return 0;
}

/*
 * Default trim controls' command handlers. For most planes, we just pass
 * the default command through, but a few addons require special handling.
 */
static int chandler_pt_up(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    chandler_context *ctx = inRefcon;
    refcon_eadt738   *x38 = NULL;
    switch (ctx->atyp)
    {
        case NVP_ACF_B737_EA:
            x38 = &ctx->acfspec.x738;
            if (x38->ready == 0)
            {
                boing_738_init(x38);
            }
            break;

        default:
            if (inPhase == xplm_CommandBegin) { XPLMCommandBegin(ctx->trims.pch.up.cd); return 0; }
            if (inPhase == xplm_CommandEnd)   { XPLMCommandEnd  (ctx->trims.pch.up.cd); return 0; }
            return 0;
    }
    if (x38 && x38->ready)
    {
        if (inPhase == xplm_CommandBegin) { XPLMCommandBegin(x38->pt_up); return 0; }
        if (inPhase == xplm_CommandEnd)   { XPLMCommandEnd  (x38->pt_up); return 0; }
        return 0;
    }
    return 0;
}

static int chandler_pt_dn(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    chandler_context *ctx = inRefcon;
    refcon_eadt738   *x38 = NULL;
    switch (ctx->atyp)
    {
        case NVP_ACF_B737_EA:
            x38 = &ctx->acfspec.x738;
            if (x38->ready == 0)
            {
                boing_738_init(x38);
            }
            break;

        default:
            if (inPhase == xplm_CommandBegin) { XPLMCommandBegin(ctx->trims.pch.dn.cd); return 0; }
            if (inPhase == xplm_CommandEnd)   { XPLMCommandEnd  (ctx->trims.pch.dn.cd); return 0; }
            return 0;
    }
    if (x38 && x38->ready)
    {
        if (inPhase == xplm_CommandBegin) { XPLMCommandBegin(x38->pt_dn); return 0; }
        if (inPhase == xplm_CommandEnd)   { XPLMCommandEnd  (x38->pt_dn); return 0; }
        return 0;
    }
    return 0;
}

static int chandler_at_lt(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    chandler_context *ctx = inRefcon;
    switch (ctx->atyp)
    {
        default:
            if (inPhase == xplm_CommandBegin) { XPLMCommandBegin(ctx->trims.ail.lt.cd); return 0; }
            if (inPhase == xplm_CommandEnd)   { XPLMCommandEnd  (ctx->trims.ail.lt.cd); return 0; }
            return 0;
    }
    return 0;
}

static int chandler_at_rt(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    chandler_context *ctx = inRefcon;
    switch (ctx->atyp)
    {
        default:
            if (inPhase == xplm_CommandBegin) { XPLMCommandBegin(ctx->trims.ail.rt.cd); return 0; }
            if (inPhase == xplm_CommandEnd)   { XPLMCommandEnd  (ctx->trims.ail.rt.cd); return 0; }
            return 0;
    }
    return 0;
}

static int chandler_rt_lt(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    chandler_context *ctx = inRefcon;
    switch (ctx->atyp)
    {
        default:
            if (inPhase == xplm_CommandBegin) { XPLMCommandBegin(ctx->trims.rud.lt.cd); return 0; }
            if (inPhase == xplm_CommandEnd)   { XPLMCommandEnd  (ctx->trims.rud.rt.cd); return 0; }
            return 0;
    }
    return 0;
}

static int chandler_rt_rt(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    chandler_context *ctx = inRefcon;
    switch (ctx->atyp)
    {
        default:
            if (inPhase == xplm_CommandBegin) { XPLMCommandBegin(ctx->trims.rud.rt.cd); return 0; }
            if (inPhase == xplm_CommandEnd)   { XPLMCommandEnd  (ctx->trims.rud.rt.cd); return 0; }
            return 0;
    }
    return 0;
}

/*
 * action: stow or deploy thrust reversers.
 *
 * rationale: X-Plane only has a toggle for this :(
 */
static int chandler_r_fwd(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        chandler_context *ctx = inRefcon;
        if (ctx->revrs.n_engines >= 1)
        {
            XPLMSetDatavi(ctx->revrs.prop_mode, propmode_fwd_get(), 0, ctx->revrs.n_engines);
        }
    }
    return 0;
}

static int chandler_r_rev(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        chandler_context *ctx = inRefcon;
        if (ctx->revrs.n_engines >= 1)
        {
            XPLMSetDatavi(ctx->revrs.prop_mode, propmode_rev_get(), 0, ctx->revrs.n_engines);
        }
    }
    return 0;
}

/*
 * action: always-available custom command which applies the appropriate switch
 *         (be it default or custom) based on the plane/addon we'll be flying.
 *
 * rationale: easier to map a joystick button to a single command for
 *            all planes than to a custom one on a plane-specific basis.
 */
static int chandler_swtch(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        chandler_command *cc = inRefcon;
        if (cc->name)
        {
            if (cc->xpcr == NULL)
            {
                if ((cc->xpcr = XPLMFindCommand(cc->name)) == NULL)
                {
                    ndt_log("navP [error]: command not found: \"%s\"\n", cc->name);
                    XPLMSpeakString("failed to resolve command");
                    cc->name = NULL; return 0;
                }
            }
            XPLMCommandOnce(cc->xpcr);
        }
    }
    return 0;
}

/*
 * Quick look view presets, utilities:
 *
 * - store the latest quick look view index
 * - select previous/next quick look view preset
 */
static int chandler_sview(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        int index = *((int*)inRefcon);
        XPLMSetDatai(ref_ql_idx_get(), index);
    }
    return 1; // passthrough
}

static int chandler_qlprv(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        chandler_context *ctx = inRefcon;
        int index = ndt_mod(ref_ql_idx_val() - 1, 10);
        XPLMCommandOnce(ctx->views.cbs[index].command);
    }
    return 0;
}

static int chandler_qlnxt(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        chandler_context *ctx = inRefcon;
        int index = ndt_mod(ref_ql_idx_val() + 1, 10);
        XPLMCommandOnce(ctx->views.cbs[index].command);
    }
    return 0;
}

static int chandler_flchg(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        chandler_context *ctx = inRefcon;
        if (XPLMGetDatai(ctx->callouts.ref_flap_lever) <= 0)
        {
            return 1;
        }
        switch (ctx->atyp)
        {
            case NVP_ACF_A320_JD:
            case NVP_ACF_A330_JD:
                return 1; // no callouts for J.A.R. addons
            case NVP_ACF_A320_QP:
            case NVP_ACF_A330_RW:
            case NVP_ACF_A350_FF:
            case NVP_ACF_A380_PH:
            case NVP_ACF_SSJ1_RZ:
            case NVP_ACF_A320ULT:
                flap_callout_setst(_flap_names_AIB1, lroundf(4.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                break;
//          case NVP_ACF_B727_FJ:
            case NVP_ACF_B737_EA:
            case NVP_ACF_B737_FJ:
            case NVP_ACF_B737_XG:
                flap_callout_setst(_flap_names_BNG1, lroundf(8.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                break;
            case NVP_ACF_B757_FF:
            case NVP_ACF_B767_FF:
            case NVP_ACF_B777_FF:
                flap_callout_setst(_flap_names_BNG2, lroundf(6.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                break;
            case NVP_ACF_DH8D_FJ:
                flap_callout_setst(_flap_names_BOM2, lroundf(4.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                break;
            case NVP_ACF_ERJ1_4D:
                flap_callout_setst(_flap_names_EMB1, lroundf(4.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                break;
            case NVP_ACF_EMBE_SS:
            case NVP_ACF_EMBE_XC:
                flap_callout_setst(_flap_names_EMB2, lroundf(6.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                break;
            default:
                if (!strcasecmp(ctx->icao, "A10"))
                {
                    flap_callout_setst(_flap_names_A10W, lroundf(2.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "AKOY") ||
                    !strcasecmp(ctx->icao, "BE20") ||
                    !strcasecmp(ctx->icao, "BE33") ||
                    !strcasecmp(ctx->icao, "BE35") ||
                    !strcasecmp(ctx->icao, "BE36") ||
                    !strcasecmp(ctx->icao, "BE58") ||
                    !strcasecmp(ctx->icao, "BE9L") ||
                    !strcasecmp(ctx->icao, "C404") ||
                    !strcasecmp(ctx->icao, "COL4") ||
                    !strcasecmp(ctx->icao, "DA40") ||
                    !strcasecmp(ctx->icao, "EA50") ||
                    !strcasecmp(ctx->icao, "EPIC") ||
                    !strcasecmp(ctx->icao, "EVIC") ||
                    !strcasecmp(ctx->icao, "LEG2") ||
                    !strcasecmp(ctx->icao, "P180") ||
                    !strcasecmp(ctx->icao, "SF50"))
                {
                    flap_callout_setst(_flap_names_2POS, lroundf(2.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "B52"))
                {
                    flap_callout_setst(_flap_names_B52G, lroundf(5.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "A318") || // most Airbus airliner variants
                    !strcasecmp(ctx->icao, "A319") ||
                    !strcasecmp(ctx->icao, "A320") ||
                    !strcasecmp(ctx->icao, "A321") ||
                    !strcasecmp(ctx->icao, "A330") ||
                    !strcasecmp(ctx->icao, "A332") ||
                    !strcasecmp(ctx->icao, "A333") ||
                    !strcasecmp(ctx->icao, "A340") ||
                    !strcasecmp(ctx->icao, "A342") ||
                    !strcasecmp(ctx->icao, "A343") ||
                    !strcasecmp(ctx->icao, "A345") ||
                    !strcasecmp(ctx->icao, "A346") ||
                    !strcasecmp(ctx->icao, "A350") ||
                    !strcasecmp(ctx->icao, "A358") ||
                    !strcasecmp(ctx->icao, "A359") ||
                    !strcasecmp(ctx->icao, "A380") ||
                    !strcasecmp(ctx->icao, "A388"))
                {
                    flap_callout_setst(_flap_names_AIB1, lroundf(8.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "B732") || // all Boeing 737 variants
                    !strcasecmp(ctx->icao, "B733") ||
                    !strcasecmp(ctx->icao, "B734") ||
                    !strcasecmp(ctx->icao, "B735") ||
                    !strcasecmp(ctx->icao, "B736") ||
                    !strcasecmp(ctx->icao, "B737") ||
                    !strcasecmp(ctx->icao, "B738") ||
                    !strcasecmp(ctx->icao, "B739") ||
                    !strcasecmp(ctx->icao, "E737") ||
                    !strcasecmp(ctx->icao, "P8"))
                {
                    flap_callout_setst(_flap_names_BNG1, lroundf(8.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "BSCA") || // all Boeing 747 variants
                    !strcasecmp(ctx->icao, "BLCF") ||
                    !strcasecmp(ctx->icao, "B74D") ||
                    !strcasecmp(ctx->icao, "B74F") ||
                    !strcasecmp(ctx->icao, "B74R") ||
                    !strcasecmp(ctx->icao, "B74S") ||
                    !strcasecmp(ctx->icao, "B741") ||
                    !strcasecmp(ctx->icao, "B742") ||
                    !strcasecmp(ctx->icao, "B743") ||
                    !strcasecmp(ctx->icao, "B744") ||
                    !strcasecmp(ctx->icao, "B747") ||
                    !strcasecmp(ctx->icao, "B748") ||
                    !strcasecmp(ctx->icao, "B752") || // all Boeing 757 variants
                    !strcasecmp(ctx->icao, "B753") ||
                    !strcasecmp(ctx->icao, "B757") ||
                    !strcasecmp(ctx->icao, "B762") || // all Boeing 767 variants
                    !strcasecmp(ctx->icao, "B763") ||
                    !strcasecmp(ctx->icao, "B764") ||
                    !strcasecmp(ctx->icao, "B767") ||
                    !strcasecmp(ctx->icao, "B77F") || // all Boeing 777 variants
                    !strcasecmp(ctx->icao, "B77L") ||
                    !strcasecmp(ctx->icao, "B77W") ||
                    !strcasecmp(ctx->icao, "B772") ||
                    !strcasecmp(ctx->icao, "B773") ||
                    !strcasecmp(ctx->icao, "B777") ||
                    !strcasecmp(ctx->icao, "B778") ||
                    !strcasecmp(ctx->icao, "B779") ||
                    !strcasecmp(ctx->icao, "B78X") || // all Boeing 787 variants
                    !strcasecmp(ctx->icao, "B787") ||
                    !strcasecmp(ctx->icao, "B788") ||
                    !strcasecmp(ctx->icao, "B789"))
                {
                    flap_callout_setst(_flap_names_BNG2, lroundf(6.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "B190"))
                {
                    flap_callout_setst(_flap_names_B190, lroundf(2.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "BD5J"))
                {
                    flap_callout_setst(_flap_names_BD5J, lroundf(3.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "C130"))
                {
                    flap_callout_setst(_flap_names_C130, lroundf(7.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "C150") ||
                    !strcasecmp(ctx->icao, "C152") ||
                    !strcasecmp(ctx->icao, "C170") ||
                    !strcasecmp(ctx->icao, "C172") ||
                    !strcasecmp(ctx->icao, "C180") ||
                    !strcasecmp(ctx->icao, "C182") ||
                    !strcasecmp(ctx->icao, "C185") ||
                    !strcasecmp(ctx->icao, "C206") ||
                    !strcasecmp(ctx->icao, "C207") ||
                    !strcasecmp(ctx->icao, "C208") ||
                    !strcasecmp(ctx->icao, "C210") ||
                    !strcasecmp(ctx->icao, "P210") ||
                    !strcasecmp(ctx->icao, "T210") ||
                    !strcasecmp(ctx->icao, "PA46"))
                {
                    flap_callout_setst(_flap_names_CSNA, lroundf(3.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "C340"))
                {
                    flap_callout_setst(_flap_names_C340, lroundf(2.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "CL30"))
                {
                    flap_callout_setst(_flap_names_BOM1, lroundf(3.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "CRUZ"))
                {
                    flap_callout_setst(_flap_names_3POS, lroundf(3.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "DC10"))
                {
                    flap_callout_setst(_flap_names_DC10, lroundf(3.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "FA7X"))
                {
                    flap_callout_setst(_flap_names_FA7X, lroundf(3.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "HA4T"))
                {
                    flap_callout_setst(_flap_names_HA4T, lroundf(3.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "MD80") ||
                    !strcasecmp(ctx->icao, "MD82") ||
                    !strcasecmp(ctx->icao, "MD83") ||
                    !strcasecmp(ctx->icao, "MD88"))
                {
                    int index = lroundf(6.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio));
                    if (ctx->atyp == NVP_ACF_MD80_RO)
                    {
                        // there is a delay in the dataref's value (caused by the animation??)
                        if (inCommand == ctx->callouts.cb_flapd.command)
                        {
                            if (++index == 2) // lever always skips 0.333 (index 2)
                            {
                                index++;
                            }
                            if (index > 6)
                            {
                                index = 6;
                            }
                        }
                        if (inCommand == ctx->callouts.cb_flapu.command)
                        {
                            if (--index == 2) // lever always skips 0.333 (index 2)
                            {
                                index--;
                            }
                            if (index < 0)
                            {
                                index = 0;
                            }
                        }
                    }
                    flap_callout_setst(_flap_names_MD80, index);
                    break;
                }
                if (!strcasecmp(ctx->icao, "PA32") ||
                    !strcasecmp(ctx->icao, "PA34"))
                {
                    flap_callout_setst(_flap_names_PIPR, lroundf(3.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "PC12"))
                {
                    flap_callout_setst(_flap_names_PC12, lroundf(3.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "PIPA"))
                {
                    flap_callout_setst(_flap_names_PIPA, lroundf(2.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                if (!strcasecmp(ctx->icao, "TBM8"))
                {
                    flap_callout_setst(_flap_names_TBM8, lroundf(3.0f * XPLMGetDataf(ctx->callouts.ref_flap_ratio)));
                    break;
                }
                return 1;
        }
        XPLMSetFlightLoopCallbackInterval(ctx->callouts.flc_flaps, 1.0f, 1, NULL);
    }
    return 1;
}

static int chandler_mcdup(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        refcon_cdu_pop *cdu = inRefcon;
        if (cdu->i_disabled == -1)
        {
            XPLMPluginID sfmc = XPLMFindPluginBySignature("pikitanga.xplane10.SimpleFMC");
            XPLMPluginID x737 = XPLMFindPluginBySignature("FJCC.x737FMC");
            XPLMPluginID xfmc = XPLMFindPluginBySignature("x-fmc.com");
            switch (cdu->atyp)
            {
                case NVP_ACF_A320ULT:
                    cdu->i_disabled = 1; return 0; // TODO

                case NVP_ACF_A320_JD:
                case NVP_ACF_A330_JD:
                case NVP_ACF_B737_XG:
                case NVP_ACF_MD80_RO:
                    cdu->i_disabled = 1; return 0; // custom FMS, but no popup/command

                case NVP_ACF_EMBE_SS:
                    cdu->i_disabled = 1; return 0; // handled elsewhere

                case NVP_ACF_B757_FF:
                case NVP_ACF_B767_FF:
                    if (NULL == (cdu->dataref[0] = XPLMFindDataRef("757Avionics/cdu/popup" )) ||
                        NULL == (cdu->dataref[1] = XPLMFindDataRef("757Avionics/cdu2/popup")))
                    {
                        cdu->i_disabled = 1; return 0;
                    }
                    cdu->i_disabled = 0; break;

                case NVP_ACF_B777_FF:
                    if (NULL == (cdu->dataref[0] = XPLMFindDataRef("T7Avionics/cdu/popup")))
                    {
                        cdu->i_disabled = 1; return 0;
                    }
                    cdu->i_disabled = 0; break;

                case NVP_ACF_A320_QP:
                case NVP_ACF_A330_RW:
                    if (NULL == (cdu->command[0] = XPLMFindCommand("AirbusFBW/UndockMCDU1")))
                    {
                        cdu->i_disabled = 1; return 0;
                    }
                    cdu->i_disabled = 0; break;

                case NVP_ACF_A350_FF:
                    if (NULL == (cdu->dataref[0] = XPLMFindDataRef("1-sim/misc/popupOis"  )) ||
                        NULL == (cdu->dataref[1] = XPLMFindDataRef("1-sim/misc/popupLeft" )) ||
                        NULL == (cdu->dataref[2] = XPLMFindDataRef("1-sim/misc/popupsHide")) ||
                        NULL == (cdu->command[0] = XPLMFindCommand("AirbusFBW/UndockMCDU1")))
                    {
                        cdu->i_disabled = 1; return 0;
                    }
                    cdu->i_disabled = 0; break;

                case NVP_ACF_EMBE_XC:
                    if (NULL == (cdu->dataref[0] = XPLMFindDataRef("sim/cockpit2/switches/generic_lights_switch")) ||
                        NULL == (cdu->dataref[1] = XPLMFindDataRef("sim/cockpit2/switches/custom_slider_on")))
                    {
                        cdu->i_disabled = 1; return 0;
                    }
                    cdu->i_disabled = 0; break;

                case NVP_ACF_HA4T_RW:
                    if (NULL == (cdu->command[0] = XPLMFindCommand("xap/panels/tim314/0")) ||
                        NULL == (cdu->command[1] = XPLMFindCommand("xap/panels/tim314/5")) ||
                        NULL == (cdu->command[2] = XPLMFindCommand("xap/panels/tim314/6")))
                    {
                        if (NULL == (cdu->command[0] = XPLMFindCommand("xap/panels/0")) ||
                            NULL == (cdu->command[1] = XPLMFindCommand("xap/panels/5")) ||
                            NULL == (cdu->command[2] = XPLMFindCommand("xap/panels/6")))
                        {
                            cdu->i_disabled = 1; return 0;
                        }
                        cdu->i_disabled = 0; break;
                    }
                    cdu->i_disabled = 0; break;

                case NVP_ACF_B737_EA:
                    if (x737 != XPLM_NO_PLUGIN_ID)
                    {
                        if (XPLMIsPluginEnabled(x737) == 0)
                        {
                            XPLMEnablePlugin(x737);
                        }
                        if (NULL == (cdu->command[0] = XPLMFindCommand("x737/UFMC/FMC_TOGGLE")))
                        {
                            cdu->i_disabled = 1; return 0;
                        }
                        cdu->i_disabled = 0; break;
                    }
                    // fall through
                default:
                {
                    cdu->command[0] = // reset commands (default
                    cdu->command[1] = // branch will always call
                    cdu->command[2] = // every non-NULL command)
                    cdu->command[3] = NULL; // :)
                    if (*cdu->auth && *cdu->desc)
                    {
                        if (!STRN_CASECMP_AUTO(cdu->auth, "Aerobask") ||
                            !STRN_CASECMP_AUTO(cdu->auth, "Stephane Buon"))
                        {
                            if ((cdu->command[0] = XPLMFindCommand("aerobask/gtn650/Popup")) &&
                                (cdu->command[1] = XPLMFindCommand("aerobask/skyview/toggle_left")))
                            {
                                cdu->i_disabled = 0; break; // Aerobask GTN
                            }
                            if ((cdu->command[0] = XPLMFindCommand("aerobask/skyview/toggle_left")))
                            {
                                cdu->i_disabled = 0; break; // Aerobask SkyView
                            }
                            if (((cdu->command[0] = XPLMFindCommand("sim/GPS/g430n1_popup"))  &&
                                 (cdu->command[1] = XPLMFindCommand("sim/GPS/g430n2_popup"))) ||
                                ((cdu->command[0] = XPLMFindCommand("sim/GPS/g430n1_popup"))))
                            {
                                cdu->i_disabled = 0; break; // X-Plane GPS
                            }
                            cdu->i_disabled = 1; return 0; // no SkyView, no GPS
                        }
                        if (!STRN_CASECMP_AUTO(cdu->auth, "Alabeo") ||
                            !STRN_CASECMP_AUTO(cdu->auth, "Carenado"))
                        {
                            if (!STRN_CASECMP_AUTO(cdu->desc, "B1900"))
                            {
                                if ((cdu->command[0] = XPLMFindCommand("xap/panels/2")))
                                {
                                    cdu->i_disabled = 0; break; // A/P & EFIS CP
                                }
                            }
                            if (!STRN_CASECMP_AUTO(cdu->desc, "CT206H Stationair"))
                            {
                                if ((cdu->command[0] = XPLMFindCommand("xap/panels/2")) && // A/P
                                    (cdu->command[2] = XPLMFindCommand("sim/GPS/g430n1_popup")))
                                {
                                    cdu->i_disabled = 0; break; // X-Plane GPS
                                }
                            }
                            if (!STRN_CASECMP_AUTO(cdu->desc, "C207 Skywagon"))
                            {
                                if ((cdu->command[0] = XPLMFindCommand("xap/panels/2")) && // A/P
                                    (cdu->command[1] = XPLMFindCommand("xap/panels/3")) && // FF
                                    (cdu->command[2] = XPLMFindCommand("sim/GPS/g430n1_popup")))
                                {
                                    cdu->i_disabled = 0; break; // X-Plane GPS
                                }
                            }
                            if (!STRN_CASECMP_AUTO(cdu->desc, "T210M Centurion II"))
                            {
                                if ((cdu->command[0] = XPLMFindCommand("xap/panels/2"))/* A/P */&&
                                    (cdu->command[1] = XPLMFindCommand("sim/GPS/g430n1_popup")) &&
                                    (cdu->command[2] = XPLMFindCommand("sim/GPS/g430n2_popup")))
                                {
                                    cdu->i_disabled = 0; break; // X-Plane GPS
                                }
                            }
                            if (!STRN_CASECMP_AUTO(cdu->desc, "C404 Titan"))
                            {
                                if ((cdu->command[0] = XPLMFindCommand("xap/panels/2"))         && // A/P
                                    (cdu->command[1] = XPLMFindCommand("sim/GPS/g430n1_popup")) &&
                                    (cdu->command[2] = XPLMFindCommand("sim/GPS/g430n2_popup")))
                                {
                                    cdu->i_disabled = 0; break; // X-Plane GPS
                                }
                            }
                            if (!STRN_CASECMP_AUTO(cdu->desc, "Piper PA-34 Seneca V"))
                            {
                                if ((cdu->command[0] = XPLMFindCommand("xap/panels/2")) && // A/P
                                    (cdu->command[1] = XPLMFindCommand("xap/panels/3")) && // G500
                                    (cdu->command[2] = XPLMFindCommand("xap/panels/4")) && // XPDR
                                    (cdu->command[3] = XPLMFindCommand("sim/GPS/g430n1_popup")))
                                {
                                    cdu->i_disabled = 0; break; // Carenado G500
                                }
                            }
                            if (XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("Carenado.G1000.Database"))
                            {
                                cdu->i_disabled = 1; return 0; // Carenado G1000
                            }
                            if (((cdu->command[0] = XPLMFindCommand("sim/GPS/g430n1_popup"))  &&
                                 (cdu->command[1] = XPLMFindCommand("sim/GPS/g430n2_popup"))) ||
                                ((cdu->command[0] = XPLMFindCommand("sim/GPS/g430n1_popup"))))
                            {
                                cdu->i_disabled = 0; break; // X-Plane GPS
                            }
                            cdu->i_disabled = 1; return 0; // no G500/G1000, GPS
                        }
                        if (!STRN_CASECMP_AUTO(cdu->auth, "Denis 'ddenn' Krupin"))
                        {
                            if (NULL == (cdu->command[0] = XPLMFindCommand("sim/operation/slider_12")))
                            {
                                cdu->i_disabled = 1; return 0;
                            }
                            cdu->i_disabled = 0; break;
                        }
                        if (((cdu->command[0] = XPLMFindCommand("sim/GPS/g430n1_popup"))  &&
                             (cdu->command[1] = XPLMFindCommand("sim/GPS/g430n2_popup"))) ||
                            ((cdu->command[0] = XPLMFindCommand("sim/GPS/g430n1_popup"))))
                        {
                            cdu->i_disabled = 0; break; // X-Plane GPS
                        }
                    }
                    if (sfmc != XPLM_NO_PLUGIN_ID)
                    {
                        if (XPLMIsPluginEnabled(sfmc) == 0)
                        {
                            XPLMEnablePlugin(sfmc);
                        }
                        if (NULL == (cdu->command[0] = XPLMFindCommand("pikitanga/SimpleFMC/ToggleSimpleFMC")))
                        {
                            cdu->i_disabled = 1; return 0;
                        }
                        cdu->i_disabled = 0; break;
                    }
                    if (xfmc != XPLM_NO_PLUGIN_ID)
                    {
                        if (XPLMIsPluginEnabled(xfmc) == 0)
                        {
                            XPLMEnablePlugin(xfmc);
                        }
                        if (NULL == (cdu->command[0] = XPLMFindCommand("xfmc/toggle")))
                        {
                            cdu->i_disabled = 1; return 0;
                        }
                        cdu->i_disabled = 0; break;
                    }
                    cdu->i_disabled = 1; return 0; // no plugin FMS found
                }
            }
            if (cdu->i_disabled == -1)
            {
                ndt_log("navP [debug]: chandler_mcdup has a BUG\n");
                cdu->i_disabled = 1; return 0;
            }
        }
        if (cdu->i_disabled)
        {
            return 0;
        }
        switch (cdu->atyp)
        {
            case NVP_ACF_EMBE_XC:
            {
                XPLMGetDatavi(cdu->dataref[1], &cdu->i_value[1], 16, 1);
                int ione = 1, itog = !cdu->i_value[1]; float ftog = itog;
                XPLMSetDatavf(cdu->dataref[0], &ftog, 22, 1); // toggle ND
                XPLMSetDatavf(cdu->dataref[0], &ftog, 21, 1); // toggle PFD
                XPLMSetDatavi(cdu->dataref[1], &itog, 16, 1); // toggle CDU
                XPLMSetDatavi(cdu->dataref[1], &itog, 19, 1); // toggle radios
                XPLMSetDatavi(cdu->dataref[1], &ione, 14, 1); // hide yoke left
                XPLMSetDatavi(cdu->dataref[1], &ione, 15, 1); // hide yoke right
                XPLMSetDatavi(cdu->dataref[1], &ione, 20, 1); // show toggle buttons
                return 0;
            }

            case NVP_ACF_B757_FF:
            case NVP_ACF_B767_FF:
                XPLMSetDatai(cdu->dataref[1], 1); // auto-reset
                // fall through
            case NVP_ACF_B777_FF:
                XPLMSetDatai(cdu->dataref[0], 1); // auto-reset
                return 0;

            case NVP_ACF_A350_FF:
                cdu->i_value[1] = !XPLMGetDatai(cdu->dataref[2]);
                cdu->i_value[0] = !XPLMGetDatai(cdu->dataref[1]);
                if (cdu->i_value[1]) // popupsHide == 0: MFD popups enabled
                {
                    XPLMSetDatai(cdu->dataref[1], cdu->i_value[0]); // popupLeft
                    XPLMSetDatai(cdu->dataref[0],             (0)); // popupOis
                    return 0;
                }
                // fall through
            default:
                if (cdu->command[0]) XPLMCommandOnce(cdu->command[0]);
                if (cdu->command[1]) XPLMCommandOnce(cdu->command[1]);
                if (cdu->command[2]) XPLMCommandOnce(cdu->command[2]);
                if (cdu->command[3]) XPLMCommandOnce(cdu->command[3]);
                return 0;
        }
    }
    return 0;
}

static int chandler_ffap1(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        XPLMDataRef dataref = *((XPLMDataRef*)inRefcon);
        if (dataref == NULL)
        {
            if ((dataref = XPLMFindDataRef("1-sim/AP/cmd_L_Button")) == NULL)
            {
                ndt_log("navP [warning]: chandler_ffap1: \"1-sim/AP/cmd_L_Button\" not found\n");
                return 0;
            }
            *((XPLMDataRef*)inRefcon) = dataref;
        }
        XPLMSetDatai(dataref, !XPLMGetDatai(dataref));
    }
    return 0;
}

static int chandler_32apc(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        refcon_assert1 *a32 = inRefcon;
        if (a32->initialized)
        {
            int32_t clicknow = 1; a32->api.ValueSet(a32->dat.id_s32_click_autopilot1, &clicknow);
        }
    }
    return 0;
}

static int chandler_32apd(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        refcon_assert1 *a32 = inRefcon;
        if (a32->initialized)
        {
            int32_t clicknow = 1; a32->api.ValueSet(a32->dat.id_s32_click_ss_tkovr_l, &clicknow);
        }
    }
    return 0;
}

static int chandler_32atd(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        refcon_assert1 *a32 = inRefcon;
        if (a32->initialized)
        {
            int32_t clicknow = 1; a32->api.ValueSet(a32->dat.id_s32_click_thr_disc_l, &clicknow);
        }
    }
    return 0;
}

static int chandler_ghndl(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandBegin) // before X-Plane moves the handle
    {
        refcon_gear    *gear = inRefcon;
        refcon_assert1 *a320 = gear->assert;
        if (gear->has_retractable_gear == -1)
        {
            if (a320)
            {
                gear->has_retractable_gear = 1;
            }
            else
            {
                gear->has_retractable_gear = !!XPLMGetDatai(gear->acf_gear_retract);
            }
        }
        if (gear->has_retractable_gear ==  1)
        {
            int speak = XPLMGetDatai(gear->callouts.ref);
            if (gear->callouts.atype & NVP_ACF_MASK_JDN)
            {
                speak = 0;
            }
            if (inCommand == gear->landing_gear_toggle.command ||
                inCommand == gear->landing_gear_down  .command)
            {
                if (a320)
                {
                    if (XPLMGetDataf(a320->dat.ldg_gears_lever) < 0.5f) // 0 -> 1
                    {
                        if (speak > 0) XPLMSpeakString("gear down"); return 1;
                    }
                }
                if (XPLMGetDatai(gear->gear_handle_down) == 0) // 0 -> 1
                {
                    if (speak > 0) XPLMSpeakString("gear down"); return 1;
                }
            }
            if (inCommand == gear->landing_gear_toggle.command ||
                inCommand == gear->landing_gear_up    .command)
            {
                if (a320)
                {
                    if (XPLMGetDataf(a320->dat.ldg_gears_lever) > 0.5f) // 1 -> 0
                    {
                        if (speak > 0) XPLMSpeakString("gear up"); return 1;
                    }
                }
                if (XPLMGetDatai(gear->gear_handle_down) != 0) // 1 -> 0
                {
                    if (speak > 0) XPLMSpeakString("gear up");   return 1;
                }
            }
            return 1; // let X-Plane actually move the handle
        }
        return 1; // let X-Plane actually move the handle
    }
    return 1; // let X-Plane actually move the handle
}

static int chandler_idleb(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    if (inPhase == xplm_CommandEnd)
    {
        refcon_ground ground = *((refcon_ground*)inRefcon);
        if (XPLMGetDataf(ground.idle.throttle_all) < 0.5f)
        {
            if (ground.idle.minimums > 0)
            {
                if (ground.idle.thrott_array)
                {
                    XPLMSetDatavf(ground.idle.thrott_array, ground.idle.r_t, 0, 2);
                    return 0;
                }
                XPLMSetDataf(ground.idle.throttle_all, ground.idle.r_taxi);
                return 0;
            }
            XPLMSetDataf(ground.idle.throttle_all, 0.2f);
            return 0;
        }
        return 0;
    }
    return 0;
}

static float flc_flap_func(float inElapsedSinceLastCall,
                           float inElapsedTimeSinceLastFlightLoop,
                           int   inCounter,
                           void *inRefcon)
{
    flap_callout_speak();
    return 0;
}

#define T_ZERO                (.0001f)
#define GS_KT_MIN             (2.500f)
#define GS_KT_MID             (26.25f)
#define GS_KT_MAX             (50.00f)
#define ACF_ROLL_SET(_var, _gs, _base)                  \
{                                                       \
    {                                                   \
        _var  = ((_gs - GS_KT_MIN) / 950.0f) + _base;   \
    }                                                   \
    if (_gs > GS_KT_MID)                                \
    {                                                   \
        _var -= ((_gs - GS_KT_MID) / 475.0f);           \
    }                                                   \
}
static float gnd_stab_hdlr(float inElapsedSinceLastCall,
                           float inElapsedTimeSinceLastFlightLoop,
                           int   inCounter,
                           void *inRefcon)
{
    if (inRefcon)
    {
        refcon_ground ground = *((refcon_ground*)inRefcon);
        float ground_spd_kts = XPLMGetDataf(ground.ground_spd) * 3.6f / 1.852f;
        float thrott_cmd_all = XPLMGetDataf(ground.idle.throttle_all) + T_ZERO;

#if 0
        float gstest[] =
        {
            ((GS_KT_MIN)),
            ((GS_KT_MID + GS_KT_MIN) / 2.0f),
            ((GS_KT_MID)),
            ((GS_KT_MID - GS_KT_MIN) * 1.5f) + GS_KT_MIN,
            ((GS_KT_MAX)),
        };
        for (int i = 0; i < (sizeof(gstest) / sizeof(gstest[0])); i++)
        {
            ACF_ROLL_SET(acf_roll_c, gstest[i], ground.nominal_roll_c);
            ndt_log("navP [debug]: acf_roll_co[%5.2fkts]: %.4f (nominal %.4f) (difference %.4f)\n",
                    gstest[i], acf_roll_c, ground.nominal_roll_c, acf_roll_c - ground.nominal_roll_c);
        }
#endif

        // first, raise our throttles to a minimum idle if required
        if (ground.idle.minimums >= 2)
        {
            if (thrott_cmd_all < ground.idle.r_idle)
            {
                XPLMSetDataf(ground.idle.throttle_all, ground.idle.r_idle);
            }
        }

        // then, update ground roll friction coefficient as required
        if (ground_spd_kts > GS_KT_MIN && ground_spd_kts < GS_KT_MAX)
        {
            float arc; ACF_ROLL_SET(arc, ground_spd_kts, ground.nominal_roll_c);
            XPLMSetDataf(ground.acf_roll_c, arc); return -4; // should be smooth
        }
        XPLMSetDataf(ground.acf_roll_c, ground.nominal_roll_c); return 0.25f;
    }
    return 0;
}

static int first_fcall_do(chandler_context *ctx)
{
    XPLMDataRef d_ref;
    XPLMCommandRef cr;
    int __skyview = 0;
    switch (ctx->atyp)
    {
        case NVP_ACF_A320ULT: // TODO
            if (ff_assert_init(&ctx->assert))
            {
                break;
            }
            else
            {
                int index[] = { 1, 4, 3, 1, 4, 1, };
                refcon_assert1 *rca = ctx->bking.rc_brk.assert = ctx->gear.assert = &ctx->assert;
                rca->api.ValueSet(rca->dat.id_u32_efis_nav_mod_lft, &index[0]);     // FCU alt. sel. increm.  (1000ft)
                rca->api.ValueSet(rca->dat.id_u32_efis_nav_mod_lft, &index[1]);     // ND m. sel. (cap. side) (arc)
                rca->api.ValueSet(rca->dat.id_u32_efis_nav_mod_rgt, &index[2]);     // ND m. sel. (f/o. side) (nav)
                rca->api.ValueSet(rca->dat.id_u32_efis_nav_rng_lft, &index[3]);     // ND r. sel. (cap. side) ( 20)
                rca->api.ValueSet(rca->dat.id_u32_efis_nav_rng_rgt, &index[4]);     // ND r. sel. (f/o. side) (160)
                rca->api.ValueSet(rca->dat.id_u32_emer_lights_mode, &index[5]);     // arm em. exit lts
            }
            break;

        case NVP_ACF_A320_QP:
            if ((d_ref = XPLMFindDataRef("AirbusFBW/DUBrightness")))
            {
                float DUBrightness[1] = { 0.8f, };
                for  (int i = 0; i < 8; i++)
                {
                    XPLMSetDatavf(d_ref, &DUBrightness[0], i, 1);
                }
            }
            if ((d_ref = XPLMFindDataRef("AirbusFBW/OHPLightSwitches")))
            {
                int OHPLightSwitches[1] = { 1, };
                XPLMSetDatavi(d_ref, &OHPLightSwitches[0], 7, 1);                   // strobes: automatic
            }
            _DO(1, XPLMSetDatai, 1, "AirbusFBW/ALT100_1000");                       // FCU alt. sel. increm.  (1000ft)
            _DO(1, XPLMSetDatai, 3, "AirbusFBW/NDmodeCapt");                        // ND m. sel. (cap. side) (arc)
            _DO(1, XPLMSetDatai, 2, "AirbusFBW/NDmodeFO");                          // ND m. sel. (f/o. side) (nav)
            _DO(1, XPLMSetDatai, 1, "AirbusFBW/NDrangeCapt");                       // ND r. sel. (cap. side) ( 20)
            _DO(1, XPLMSetDatai, 4, "AirbusFBW/NDrangeFO");                         // ND r. sel. (f/o. side) (160)
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_1_selection_copilot");  // VOR1 on ND2 off
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_2_selection_copilot");  // VOR2 on ND2 off
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_1_selection_pilot");    // VOR1 on ND1 off
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_2_selection_pilot");    // VOR2 on ND1 off
            break;

        case NVP_ACF_A330_RW:
            if ((d_ref = XPLMFindDataRef("AirbusFBW/DUBrightness")))
            {
                float DUBrightness[1] = { 0.8f, };
                for  (int i = 0; i < 8; i++)
                {
                    XPLMSetDatavf(d_ref, &DUBrightness[0], i, 1);
                }
            }
            _DO(1, XPLMSetDatai, 1, "AirbusFBW/ALT100_1000");                       // FCU alt. sel. increm.  (1000ft)
            _DO(1, XPLMSetDatai, 3, "AirbusFBW/NDmodeCapt");                        // ND m. sel. (cap. side) (arc)
            _DO(1, XPLMSetDatai, 2, "AirbusFBW/NDmodeFO");                          // ND m. sel. (f/o. side) (nav)
            _DO(1, XPLMSetDatai, 1, "AirbusFBW/NDrangeCapt");                       // ND r. sel. (cap. side) ( 20)
            _DO(1, XPLMSetDatai, 4, "AirbusFBW/NDrangeFO");                         // ND r. sel. (f/o. side) (160)
            _DO(1, XPLMSetDataf,.8f,"A330/LIGHTS/INTEG_LT");                        // ins. label light
            _DO(1, XPLMSetDatai, 1, "A330/lighting/DOME_LIGHT");                    // ambi. dome light
            _DO(1, XPLMSetDatai, 1, "A330/OVERHEAD/EMEREXITLT_SWITCH");             // arm em. exit lts
            _DO(1, XPLMSetDatai, 1, "A330/ELECTRICAL/APU_BAT");                     // self-explanatory
            _DO(1, XPLMSetDatai, 1, "A330/ELECTRICAL/APU_GEN");                     // self-explanatory
            _DO(1, XPLMSetDatai, 1, "A330/ELECTRICAL/BUS_TIE");                     // self-explanatory
            _DO(1, XPLMSetDatai, 1, "A330/ELECTRICAL/GEN_1");                       // self-explanatory
            _DO(1, XPLMSetDatai, 1, "A330/ELECTRICAL/GEN_2");                       // self-explanatory
            _DO(1, XPLMSetDatai, 1, "A330/HYD/YELLOW_ELEC_2");                      // hydraulics AUTO
            _DO(1, XPLMSetDatai, 1, "A330/HYD/YELLOW_ENG_2");                       // hydraulics AUTO
            _DO(1, XPLMSetDatai, 1, "A330/HYD/GREEN_ELEC_1");                       // hydraulics AUTO
            _DO(1, XPLMSetDatai, 1, "A330/HYD/GREEN_ENG_1");                        // hydraulics AUTO
            _DO(1, XPLMSetDatai, 1, "A330/HYD/GREEN_ENG_2");                        // hydraulics AUTO
            _DO(1, XPLMSetDatai, 1, "A330/HYD/BLUE_ELEC_1");                        // hydraulics AUTO
            _DO(1, XPLMSetDatai, 1, "A330/HYD/BLUE_ENG_1");                         // hydraulics AUTO
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_1_selection_copilot");  // VOR1 on ND2 off
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_2_selection_copilot");  // VOR2 on ND2 off
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_1_selection_pilot");    // VOR1 on ND1 off
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_2_selection_pilot");    // VOR2 on ND1 off
            break;

        case NVP_ACF_A350_FF:
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_1_selection_copilot");  // VOR1 on ND2 off
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_2_selection_copilot");  // VOR2 on ND2 off
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_1_selection_pilot");    // VOR1 on ND1 off
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_2_selection_pilot");    // VOR2 on ND1 off
            _DO(1, XPLMSetDatai,      0, "1-sim/fcu/navL/flag");                    // sync with above
            _DO(1, XPLMSetDatai,      0, "1-sim/fcu/navL2/flag");                   // sync with above
            _DO(1, XPLMSetDatai,      0, "1-sim/fcu/navR/flag");                    // sync with above
            _DO(1, XPLMSetDatai,      0, "1-sim/fcu/navR2/flag");                   // sync with above
            _DO(1, XPLMSetDatai,      1, "1-sim/fcu/altModeSwitch");                // FCU alt. sel. increm.  (1000ft)
            _DO(1, XPLMSetDataf,   3.0f, "1-sim/fcu/ndModeLeft/switch");            // ND m. sel. (cap. side) (arc)
            _DO(1, XPLMSetDataf,   2.0f, "1-sim/fcu/ndModeRight/switch");           // ND m. sel. (f/o. side) (nav)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/fcu/ndZoomLeft/switch");            // ND r. sel. (cap. side) ( 20)
            _DO(1, XPLMSetDataf,   4.0f, "1-sim/fcu/ndZoomRight/switch");           // ND r. sel. (f/o. side) (160)
            _DO(1, XPLMSetDatai,      1, "1-sim/radio/button/1/16");                // RMP1: microph. on VHF1 (on)
            _DO(1, XPLMSetDatai,      1, "1-sim/radio/push/1/1");                   // RMP1: receiver on VHF1 (on)
            _DO(1, XPLMSetDataf, 270.0f, "1-sim/radio/1/1/rotary");                 // RMP1: receiver on VHF1 (volume)
            _DO(1, XPLMSetDatai,      1, "1-sim/radio/push/1/10");                  // RMP1: c. crew intercom (on)
            _DO(1, XPLMSetDataf, 270.0f, "1-sim/radio/1/10/rotary");                // RMP1: c. crew intercom (volume)
            _DO(1, XPLMSetDatai,      1, "1-sim/radio/push/1/11");                  // RMP1: p. announcements (on)
            _DO(1, XPLMSetDataf, 270.0f, "1-sim/radio/1/11/rotary");                // RMP1: p. announcements (volume)
            _DO(1, XPLMSetDatai,      1, "1-sim/1/switch");                         // Evac. panel: selector  (capt&purs)
            _DO(1, XPLMSetDatai,      1, "1-sim/2/switch");                         // Ext. lighting: strobe  (auto)
            _DO(1, XPLMSetDatai,      1, "1-sim/5/switch");                         // Ext. lighting: logo    (auto)
            _DO(1, XPLMSetDatai,      1, "1-sim/20/switch");                        // Ext. lighting: emerg.  (auto)
            _DO(1, XPLMSetDatai,      1, "1-sim/37/switch");                        // Braking: anti-skid sw. (on)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/15/button");                        // Cabin panel: wireless  (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/16/button");                        // Cabin panel: pas. data (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/17/button");                        // Cabin panel: sat. com. (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/18/button");                        // Cabin panel: lan. cam. (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/19/button");                        // Cabin panel: FAR4 sys. (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/20/button");                        // Cabin panel: mob. com. (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/21/button");                        // Cabin panel: IFEC sys. (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/22/button");                        // Cabin panel: DER  sys. (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/38/button");                        // Oxyg. panel: crew sup. (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/42/button");                        // Vent. panel: cooling   (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/44/button");                        // Vent. panel: c. fans 1 (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/45/button");                        // Vent. panel: c. fans 2 (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/53/button");                        // NSS: data to avionics  (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/54/button");                        // NSS: cabin data to NSS (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/230/button");                       // NSS: gatelink          (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/55/button");                        // Cargo: A/C I. v. (fwd) (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/56/button");                        // Cargo: A/C I. v. (aft) (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/57/button");                        // Cargo: A/C I. v. (blk) (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/59/button");                        // Cargo: A/C heating sw. (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/65/button");                        // Hydraulics: YELL. p. 1 (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/67/button");                        // Hydraulics: GREEN p. 1 (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/75/button");                        // Hydraulics: YELL. p. 2 (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/77/button");                        // Hydraulics: GREEN p. 2 (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/84/button");                        // Fuel: C tank feed mode (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/96/button");                        // Elec. panel: AC bus 1A (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/99/button");                        // Elec. panel: AC bus 1B (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/100/button");                       // Elec. panel: AC bus t. (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/101/button");                       // Elec. panel: APU (gen) (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/103/button");                       // Elec. panel: AC bus 2B (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/106/button");                       // Elec. panel: AC bus 2A (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/109/button");                       // Air panel: bleed en. 1 (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/110/button");                       // Air panel: heat. en. 1 (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/114/button");                       // Air panel: heat. en. 2 (auto)
            _DO(1, XPLMSetDataf,   1.0f, "1-sim/115/button");                       // Air panel: bleed en. 2 (auto)
            _DO(1, XPLMSetDatai,      1, "1-sim/air/airFlowSwitch");                // Air panel: flow selec. (norm)
            _DO(1, XPLMSetDatai,      1, "1-sim/air/crossBeedSwitch");              // Air panel: c.b. selec. (auto)
            _DO(1, XPLMSetDataf,   0.4f, "1-sim/air/ckptSettingRotery");            // Air panel: cock. temp. (purser)
            _DO(1, XPLMSetDataf,   0.4f, "1-sim/air/cabinSettingRotery");           // Air panel: cabin temp. (purser)
            break;

        case NVP_ACF_A380_PH:
            if ((d_ref = XPLMFindDataRef("sim/cockpit2/switches/instrument_brightness_ratio")))
            {
                float instrument_brightness_ratio[3] = { 0.8f, 0.4f, 0.0f, };
                XPLMSetDatavf(d_ref, &instrument_brightness_ratio[0],  0, 1);       // Backlighting: switches, FCU
//              XPLMSetDatavf(d_ref, &instrument_brightness_ratio[0],  1, 1);       // Controlled by plugin?
                XPLMSetDatavf(d_ref, &instrument_brightness_ratio[0],  2, 1);       // Navigation display
                XPLMSetDatavf(d_ref, &instrument_brightness_ratio[0],  3, 1);       // ECAM (upp. display)
                XPLMSetDatavf(d_ref, &instrument_brightness_ratio[2],  4, 1);       // OIS (unus. display)
                XPLMSetDatavf(d_ref, &instrument_brightness_ratio[0],  5, 1);       // Primary f. display
                XPLMSetDatavf(d_ref, &instrument_brightness_ratio[0],  6, 1);       // Control dis. units
                XPLMSetDatavf(d_ref, &instrument_brightness_ratio[0],  7, 1);       // ECAM (low. display)
//              XPLMSetDatavf(d_ref, &instrument_brightness_ratio[2],  8, 1);       // ???
                XPLMSetDatavf(d_ref, &instrument_brightness_ratio[1],  9, 1);       // LED lam. 4 displays
//              XPLMSetDatavf(d_ref, &instrument_brightness_ratio[2], 10, 1);       // ???
//              XPLMSetDatavf(d_ref, &instrument_brightness_ratio[2], 11, 1);       // ???
//              XPLMSetDatavf(d_ref, &instrument_brightness_ratio[2], 12, 1);       // ???
//              XPLMSetDatavf(d_ref, &instrument_brightness_ratio[2], 13, 1);       // ???
//              XPLMSetDatavf(d_ref, &instrument_brightness_ratio[2], 14, 1);       // ???
                XPLMSetDatavf(d_ref, &instrument_brightness_ratio[0], 15, 1);       // R.M. Panel displays, instrument outline lighting
            }
            if ((d_ref = XPLMFindDataRef("sim/cockpit2/switches/panel_brightness_ratio")))
            {
                float panel_brightness_ratio[1] = { 0.4f, };
                XPLMSetDatavf(d_ref, &panel_brightness_ratio[0], 0, 1);             // Cockpit/flood lights
            }
            _DO(1, XPLMSetDatai, 2, "sim/cockpit2/EFIS/map_mode");                  // ND m. sel. (cap. side) (arc)
            _DO(1, XPLMSetDatai, 3, "sim/cockpit2/EFIS/map_range");                 // ND r. sel. (cap. side) (20)
            _DO(1, XPLMSetDatai, 1, "com/petersaircraft/aibus/ALT100_1000");        // FCU alt. sel. increm.  (1000ft)
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_1_selection_copilot");  // VOR1 on ND2 off
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_2_selection_copilot");  // VOR2 on ND2 off
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_1_selection_pilot");    // VOR1 on ND1 off
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_2_selection_pilot");    // VOR2 on ND1 off
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_airport_on");
            _DO(0, XPLMSetDatai, 0, "sim/cockpit2/EFIS/EFIS_fix_on");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_ndb_on");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_vor_on");
            break;

        case NVP_ACF_B737_EA:
            _DO(1, XPLMSetDatai, 0, "x737/cockpit/yoke/captYokeVisible");
            _DO(1, XPLMSetDatai, 0, "x737/cockpit/yoke/foYokeVisible");
            break;

        case NVP_ACF_B737_XG:
            _DO(1, XPLMSetDataf,   0.0f, "ixeg/733/aircond/aircond_cont_cabin_sel_act");    // Cont. cab. air temper. (normal)
            _DO(1, XPLMSetDataf,   0.0f, "ixeg/733/aircond/aircond_pass_cabin_sel_act");    // Pass. cab. air temper. (normal)
            _DO(1, XPLMSetDataf,   1.0f, "ixeg/733/bleedair/bleedair_recirc_fan_act");      // Bleed air recirc. fans (auto)
//          _DO(1, XPLMSetDataf, -20.0f, "ixeg/733/ehsi/dh_pt_act");                        // ADI DH REF (cap. side) (reset)   // note: only works when power is on
//          _DO(1, XPLMSetDataf, -20.0f, "ixeg/733/ehsi/dh_cpt_act");                       // ADI DH REF (f/o. side) (reset)   // note: only works when power is on
            _DO(1, XPLMSetDataf,   2.0f, "ixeg/733/ehsi/ehsi_mode_pt_act");                 // HSI m.sel. (cap. side) (map)
            _DO(1, XPLMSetDataf,   3.0f, "ixeg/733/ehsi/ehsi_mode_cpt_act");                // HSI m.sel. (f/o. side) (ctr)
            _DO(1, XPLMSetDataf,   1.0f, "ixeg/733/ehsi/ehsi_range_pt_act");                // HSI r.sel. (cap. side) ( 20)
            _DO(1, XPLMSetDataf,   4.0f, "ixeg/733/ehsi/ehsi_range_cpt_act");               // HSI r.sel. (f/o. side) (160)
            _DO(1, XPLMSetDataf,   0.0f, "ixeg/733/rheostats/light_breakers_act");          // Circuit breakers light (off)
            _DO(1, XPLMSetDataf,   0.8f, "ixeg/733/rheostats/light_pedpanel_act");          // Panel light (pedestal) (daylight)
            _DO(1, XPLMSetDataf,   0.8f, "ixeg/733/rheostats/light_overhead_act");          // Panel light (overhead) (daylight)
            _DO(1, XPLMSetDataf,   .21f, "ixeg/733/rheostats/light_pedflood_act");          // Flood light (pedestal) (daylight)
            _DO(1, XPLMSetDataf,   0.2f, "ixeg/733/rheostats/light_afds_act");              // Flood light (A.F.D.S.) (daylight)
            _DO(1, XPLMSetDataf,   0.8f, "ixeg/733/rheostats/light_fmc_pt_act");            // MCDU: lig. (cap. side) (daylight)
            _DO(1, XPLMSetDataf,   0.8f, "ixeg/733/rheostats/light_fmc_cpt_act");           // MCDU: lig. (f/o. side) (daylight)
            _DO(1, XPLMSetDataf,   0.0f, "ixeg/733/rheostats/light_mapbr_pt_act");          // Maps: lig. (cap. side) (off)
            _DO(1, XPLMSetDataf,   0.0f, "ixeg/733/rheostats/light_mapbr_cpt_act");         // Maps: lig. (f/o. side) (off)
            if ((d_ref = XPLMFindDataRef("sim/cockpit2/switches/instrument_brightness_ratio")))
            {
                float instrument_brightness_ratio[1] = { 0.8f, };
                XPLMSetDatavf(d_ref, &instrument_brightness_ratio[0], 0, 1);                // Panel (cap. side): daylight
                XPLMSetDatavf(d_ref, &instrument_brightness_ratio[0], 1, 1);                // Panel (f/o. side): daylight
                XPLMSetDatavf(d_ref, &instrument_brightness_ratio[0], 2, 1);                // Panel (backgrnd.): neon off
            }
            break;

        case NVP_ACF_B757_FF:
        case NVP_ACF_B767_FF:
            // the following two are special, the buttons auto-revert to zero;
            // thankfully they always work (even without any electrical power)
            if ((d_ref = XPLMFindDataRef("1-sim/vor1/isAuto")) &&
                (0000 == XPLMGetDatai(d_ref)))
            {
                _DO(1, XPLMSetDatai,1, "anim/78/button"); // VOR 1: manual -> auto
            }
            if ((d_ref = XPLMFindDataRef("1-sim/vor2/isAuto")) &&
                (0000 == XPLMGetDatai(d_ref)))
            {
                _DO(1, XPLMSetDatai,1, "anim/79/button"); // VOR 2: manual -> auto
            }
            _DO(0, XPLMSetDataf,360.0f,"sim/cockpit2/radios/actuators/nav1_obs_deg_mag_pilot"); // VOR 1: course selector: 360
            _DO(1, XPLMSetDataf,360.0f,"1-sim/radios/nav3_obs_deg_mag_pilot");                  // VOR 2: course selector: 360
            _DO(1, XPLMSetDataf,  .85f,"1-sim/adf/leftBigRotary");                  // ADF 1: 3 // LSGG GLA (Gland)
            _DO(1, XPLMSetDataf,  .20f,"1-sim/adf/leftMidRotary");                  // ADF 1: 7
            _DO(1, XPLMSetDataf,  .40f,"1-sim/adf/leftSmallRotary");                // ADF 1: 5
            _DO(1, XPLMSetDataf,  .10f,"1-sim/adf/rightBigRotary");                 // ADF 2: 2 // KJFK BBN (Babylon)
            _DO(1, XPLMSetDataf,  .80f,"1-sim/adf/rightMidRotary");                 // ADF 2: 7
            _DO(1, XPLMSetDataf,  .60f,"1-sim/adf/rightSmallRotary");               // ADF 2: 5
//          _DO(1, XPLMSetDatai,    1, "anim/75/button");                           // Terrain override switch (on)
//          _DO(1, XPLMSetDataf, 1.0f, "1-sim/gauges/terrOVRDcover");               // Terrain override switch (lift cover)
//          _DO(1, XPLMSetDataf, 1.0f, "1-sim/electrical/batteryCover");            // Battery on/off selector (lift cover)
//          _DO(1, XPLMSetDataf, 1.0f, "1-sim/elecengcont/leftCover");              // Engine elec. contr. (L) (lift cover)
//          _DO(1, XPLMSetDataf, 1.0f, "1-sim/elecengcont/rightCover");             // Engine elec. contr. (R) (lift cover)
            _DO(1, XPLMSetDatai,    1, "anim/3/button");                            // Engine elec. contr. (L) (on)
            _DO(1, XPLMSetDatai,    1, "anim/4/button");                            // Engine elec. contr. (R) (on)
            _DO(1, XPLMSetDatai,    1, "anim/8/button");                            // Engine-dr. hy. pump (L) (on)
            _DO(1, XPLMSetDatai,    1, "anim/11/button");                           // Engine-dr. hy. pump (R) (on)
            _DO(1, XPLMSetDatai,    1, "anim/54/button");                           // Air: trim air valve sw. (on)
            _DO(1, XPLMSetDatai,    1, "anim/55/button");                           // Air: L recirc. fans sw. (on)
            _DO(1, XPLMSetDatai,    1, "anim/56/button");                           // Air: R recirc. fans sw. (on)
            _DO(1, XPLMSetDatai,    1, "anim/59/button");                           // Air: L bleed ISLN valve (on/open)
            _DO(1, XPLMSetDatai,    1, "anim/87/button");                           // Air: R bleed ISLN valve (on/open)
            _DO(1, XPLMSetDatai,    1, "anim/90/button");                           // Air: C bleed ISLN valve (on/open)
            _DO(1, XPLMSetDatai,    1, "anim/60/button");                           // Air: L e. bleed air sw. (on)
            _DO(1, XPLMSetDatai,    1, "anim/62/button");                           // Air: R e. bleed air sw. (on)
            _DO(1, XPLMSetDataf, 0.5f, "1-sim/cond/fwdTempControl");                // Temp. control (fwd ca.) (auto)
            _DO(1, XPLMSetDataf, 0.5f, "1-sim/cond/midTempControl");                // Temp. control (mid ca.) (auto)
            _DO(1, XPLMSetDataf, 0.5f, "1-sim/cond/aftTempControl");                // Temp. control (aft ca.) (auto)
            _DO(1, XPLMSetDataf, 0.5f, "1-sim/cond/fltdkTempControl");              // Temp. control (f. deck) (auto)
            _DO(1, XPLMSetDatai,    0, "params/autosave");                          // DO THIS FIRST prior any changes
            _DO(1, XPLMSetDataf, 0.1f, "params/sound/avionics");                    // sounds: turn down whiny avionics
            _DO(1, XPLMSetDataf, 1.0f, "params/sound/pa");                          // sounds: hear PA from flight deck
            _DO(1, XPLMSetDataf, 0.0f, "params/sound/cc");                          // cabin crew: Daniel OFF (yaaaay!)
            if ((d_ref = XPLMFindDataRef("sim/cockpit2/switches/custom_slider_on")))
            {
                int left_front_door_slider_set_open[1] = { 1, };
                XPLMSetDatavi(d_ref, &left_front_door_slider_set_open[0], 0, 1);
            }
            if (ctx->atyp == NVP_ACF_B757_FF)
            {
//              _DO(1, XPLMSetDatai, 1, "1-sim/ndpanel/2/hsiModeButton");               // requires a Modern EFIS Panel
                _DO(1, XPLMSetDatai, 4, "1-sim/ndpanel/2/hsiModeRotary");               // ND m. sel. (f/o. side) (map)
                _DO(1, XPLMSetDatai, 4, "1-sim/ndpanel/2/hsiRangeRotary");              // ND r. sel. (f/o. side) (160)
                _DO(1, XPLMSetDatai, 4, "1-sim/ndpanel/1/hsiModeRotary");               // ND m. sel. (cap. side) (map)
                _DO(1, XPLMSetDatai, 1, "1-sim/ndpanel/1/hsiRangeRotary");              // ND r. sel. (cap. side) ( 20)
                _DO(1, XPLMSetDatai, 0, "757Avionics/options/ND/advEfisPanel");         // avionics: Modern EFIS Panel     (off) required for EGPWS, Terrain
                _DO(1, XPLMSetDatai, 0, "757Avionics/options/PFD/SpdTrendVector");      // avionics: Trend Vector          (off) requires speedtape, FMA/Top
                _DO(1, XPLMSetDatai, 1, "757Avionics/options/PFD/speedTape");           // avionics: Airspeed Tape         (yes) always linked to fma on top
                _DO(1, XPLMSetDatai, 1, "757Avionics/options/PFD/fmaOnTop");            // avionics: FMA on Top            (yes) always linked to speed tape
                _DO(1, XPLMSetDatai, 0, "757Avionics/options/ND/tasGs");                // avionics: TAS and GS            (off) GS on PFD (even w/out tape)
                _DO(1, XPLMSetDatai, 0, "params/gpwsType");                             // avionics: EGPWS                 (off) requires: Modern EFIS Panel
                _DO(1, XPLMSetDatai, 2, "params/redDisplays");                          // avionics: red displays          (all)
                _DO(1, XPLMSetDatai, 0, "757Avionics/options/PFD/IlsDevWarning");       // avionics: ILS Deviation Warning (off)
                _DO(1, XPLMSetDatai, 1, "757Avionics/options/PFD/advRaAlerts");         // avionics: RA Alerts         (1,000ft)
                _DO(1, XPLMSetDatai, 1, "1-sim/optoins/eicas/apuRPM");                  // avionics: EICAS display settings
                _DO(1, XPLMSetDatai, 1, "1-sim/optoins/eicas/EngineFuelFlow");          // avionics: EICAS display settings
                _DO(1, XPLMSetDatai, 1, "1-sim/optoins/eicas/HydraulicPressure");       // avionics: EICAS display settings
                _DO(1, XPLMSetDatai, 0, "1-sim/optoins/eicas/TirePressureDisplay");     // avionics: EICAS display settings
                _DO(1, XPLMSetDatai, 0, "1-sim/optoins/eicas/APUOilQuantityDisplay");   // avionics: EICAS display settings
                _DO(1, XPLMSetDatai, 0, "1-sim/optoins/eicas/BrakeTemperatureDisplay"); // avionics: EICAS display settings
                _DO(1, XPLMSetDatai, 0, "1-sim/optoins/eicas/BulkCargoCompartmentTemperature");   // EICAS display settings
            }
            else
            {
                _DO(1, XPLMSetDatai, 1, "1-sim/ndpanel/2/hsiModeButton");               // ND m. sel. (f/o. side) (ctr)
                _DO(1, XPLMSetDatai, 2, "1-sim/ndpanel/2/hsiModeRotary");               // ND m. sel. (f/o. side) (map)
                _DO(1, XPLMSetDatai, 4, "1-sim/ndpanel/2/hsiRangeRotary");              // ND r. sel. (f/o. side) (160)
                _DO(1, XPLMSetDatai, 2, "1-sim/ndpanel/1/hsiModeRotary");               // ND m. sel. (cap. side) (map)
                _DO(1, XPLMSetDatai, 1, "1-sim/ndpanel/1/hsiRangeRotary");              // ND r. sel. (cap. side) ( 20)
                _DO(1, XPLMSetDatai, 1, "757Avionics/options/ND/advEfisPanel");         // avionics: Modern EFIS Panel     (yes) required for EGPWS, Terrain
                _DO(1, XPLMSetDatai, 1, "757Avionics/options/PFD/SpdTrendVector");      // avionics: Trend Vector          (yes) requires speedtape, FMA/Top
                _DO(1, XPLMSetDatai, 1, "757Avionics/options/PFD/speedTape");           // avionics: Airspeed Tape         (yes) always linked to fma on top
                _DO(1, XPLMSetDatai, 1, "757Avionics/options/PFD/fmaOnTop");            // avionics: FMA on Top            (yes) always linked to speed tape
                _DO(1, XPLMSetDatai, 0, "757Avionics/options/ND/tasGs");                // avionics: TAS and GS            (off) GS on PFD (even w/out tape)
                _DO(1, XPLMSetDatai, 1, "params/gpwsType");                             // avionics: EGPWS                 (yes) requires: Modern EFIS Panel
                _DO(1, XPLMSetDatai, 1, "params/redDisplays");                          // avionics: red displays          (mix)
                _DO(1, XPLMSetDatai, 1, "757Avionics/options/PFD/IlsDevWarning");       // avionics: ILS Deviation Warning (yes)
                _DO(1, XPLMSetDatai, 2, "757Avionics/options/PFD/advRaAlerts");         // avionics: RA Alerts         (2,500ft)
                _DO(1, XPLMSetDatai, 1, "1-sim/optoins/eicas/apuRPM");                  // avionics: EICAS display settings
                _DO(1, XPLMSetDatai, 1, "1-sim/optoins/eicas/EngineFuelFlow");          // avionics: EICAS display settings
                _DO(1, XPLMSetDatai, 1, "1-sim/optoins/eicas/HydraulicPressure");       // avionics: EICAS display settings
                _DO(1, XPLMSetDatai, 0, "1-sim/optoins/eicas/TirePressureDisplay");     // avionics: EICAS display settings
                _DO(1, XPLMSetDatai, 0, "1-sim/optoins/eicas/APUOilQuantityDisplay");   // avionics: EICAS display settings
                _DO(1, XPLMSetDatai, 1, "1-sim/optoins/eicas/BrakeTemperatureDisplay"); // avionics: EICAS display settings
                _DO(1, XPLMSetDatai, 0, "1-sim/optoins/eicas/BulkCargoCompartmentTemperature");   // EICAS display settings
            }
            _DO(1, XPLMSetDatai,    1, "757Avionics/options/AP/attHldAtApEngagement");  // avionics: ATT on CMD engage     (yes)
            _DO(1, XPLMSetDatai,    0, "757Avionics/options/AP/gsCaptBeforeLocCapt");   // avionics: GS before LOC         (N/A)
            _DO(1, XPLMSetDatai,    1, "757Avionics/options/AP/manualCmdSelForApp");    // avionics: CMD manual arm        (yes)
            _DO(1, XPLMSetDatai,    1, "757Avionics/options/ND/DigitWindBearing");      // avionics: Digital Wind Bearing  (yes)
            _DO(1, XPLMSetDatai,    0, "757Avionics/options/PFD/roundDialRa");          // avionics: Round Dial RA         (N/A)
            _DO(1, XPLMSetDatai,    1, "757Avionics/options/ND/advRangeArcs");          // avionics: Range Arcs            (yes)
            _DO(1, XPLMSetDatai,    0, "757Avionics/options/ND/adfPointers");           // avionics: ADF Pointers          (N/A)
            _DO(1, XPLMSetDatai,    0, "757Avionics/options/PFD/RisingRwy");            // avionics: Rising Runway         (N/A)
            _DO(1, XPLMSetDatai,    0, "757Avionics/options/ND/hdgUpMap");              // avionics: Heading Up Map        (N/A)
            _DO(1, XPLMSetDatai,    0, "757Avionics/options/PFD/FdType");               // avionics: Integrated Cue FD     (off)
            _DO(1, XPLMSetDatai,    1, "757Avionics/options/FMS/isGPS");                // avionics: GPS EQUIPPED          (yes)
            _DO(1, XPLMSetDatai,    1, "757Avionics/fms/type");                         // avionics: PIP FMC               (yes)
//          _DO(1, XPLMSetDatai,    1, "757Avionics/engine");                           // needs setting non-dataref variable(s)
            _DO(1, XPLMSetDatai,    0, "WINGLETS/WINGLETS");                            // airplane: winglets              (off)
            _DO(1, XPLMSetDataf, 0.0f, "params/reflectLevelInstruments");           // custom reflections OFF
            _DO(1, XPLMSetDataf, 0.0f, "params/reflectLevelWindows");               // custom reflections OFF
            _DO(1, XPLMSetDataf, 0.0f, "params/reflectLevel");                      // custom reflections OFF
            _DO(1, XPLMSetDataf, 0.0f, "params/effectLevelGlowScreen");             // custom screen glow OFF
            _DO(1, XPLMSetDataf, 0.0f, "params/effectLevelGlowCDU");                // custom screen glow OFF
            _DO(1, XPLMSetDataf, 0.0f, "params/effectLevelBlick");                  // custom screen glow OFF
            _DO(1, XPLMSetDataf, 0.0f, "params/effectLevel");                       // custom screen glow OFF
            _DO(1, XPLMSetDataf, 0.1f, "params/wingflexLevel");                     // custom "wing flex" LOW
            _DO(1, XPLMSetDatai,    0, "params/constrol");                          // F/O. is in control OFF
            _DO(1, XPLMSetDatai,    0, "params/shakeOnTurb");                       // shake in turb. air OFF
            _DO(1, XPLMSetDatai,    0, "params/customEffects");                     // custom ef. control OFF
            _DO(1, XPLMSetDatai,    0, "params/windshearType");                     // advanced windshear OFF
            _DO(1, XPLMSetDatai,    0, "params/dynamicBlinds");                     // dyn. window blinds OFF
            _DO(1, XPLMSetDatai,    0, "params/autoGearLever");                     // automatic g. lever OFF
            _DO(1, XPLMSetDatai,    0, "params/last_position");                     // inter-flight data  OFF
            _DO(1, XPLMSetDatai,    0, "params/realism_level");                     // "challenge" level  OFF (0)
            _DO(1, XPLMSetDatai,    1, "params/betterpushback");                    // use BetterPushback ON
            _DO(1, XPLMSetDatai,    1, "params/throttleblock");                     // throttle level bl. ON
            _DO(1, XPLMSetDatai,    1, "params/real_limits");                       // real aircr. limits ON
            _DO(1, XPLMSetDatai,    1, "params/real_time");                         // real aircr. timing ON
            _DO(1, XPLMSetDatai,    1, "params/realSound");                         // real sound config. ON
            _DO(1, XPLMSetDatai,    1, "params/yokehide");                          // hide CA & FO yokes ON
            _DO(1, XPLMSetDatai,    1, "params/wxrType");                           // real weather radar ON
            _DO(1, XPLMSetDatai,    1, "params/metric");                            // metric measurement ON
            _DO(1, XPLMSetDatai,    1, "params/wheel");                             // scr. wheel support ON (default behavior)
            _DO(1, XPLMSetDatai,    1, "params/ground_start_unit");                 // minimal ground config
            _DO(1, XPLMSetDatai,    1, "params/gpu");                               // minimal ground config
            _DO(1, XPLMSetDatai,    1, "params/gate");                              // minimal ground config
            _DO(1, XPLMSetDatai,    1, "params/fuel_truck");                        // minimal ground config
            _DO(1, XPLMSetDatai,    0, "params/ACU");                               // minimal ground config
            _DO(1, XPLMSetDatai,    0, "params/LSU");                               // minimal ground config
            _DO(1, XPLMSetDatai,    0, "params/bus");                               // minimal ground config
            _DO(1, XPLMSetDatai,    0, "params/cover");                             // minimal ground config
            _DO(1, XPLMSetDatai,    0, "params/deice");                             // minimal ground config
            _DO(1, XPLMSetDatai,    0, "params/stairs");                            // minimal ground config
            break; // note: no datarefs for e.g. charts, chocks; some settings only affect either of 757/767

        case NVP_ACF_B777_FF:
            _DO(1, XPLMSetDatai,    0, "anim/31/switch");                           // VOR1 on ND1 off
            _DO(1, XPLMSetDatai,    0, "anim/32/switch");                           // VOR2 on ND1 off
            _DO(1, XPLMSetDatai,    2, "anim/64/switch");                           // ND m. sel. (capt. side) (map)
            _DO(1, XPLMSetDatai,    1, "anim/65/switch");                           // ND r. sel. (capt. side) (20)
//          _DO(1, XPLMSetDatai,    1, "anim/???/button"); // TODO: find dataref    // Terrain override switch (on)
//          _DO(1, XPLMSetDataf, 1.0f, "anim/33/cover");                            // Terrain override switch (lift cover)
//          _DO(1, XPLMSetDataf, 1.0f, "anim/14/cover");                            // Engine elec. contr. (L) (lift cover)
//          _DO(1, XPLMSetDataf, 1.0f, "anim/16/cover");                            // Engine elec. contr. (R) (lift cover)
            _DO(1, XPLMSetDatai,    1, "anim/108/button");                          // Engine-dr. hy. pump (L) (on)
            _DO(1, XPLMSetDatai,    1, "anim/111/button");                          // Engine-dr. hy. pump (R) (on)
            _DO(1, XPLMSetDatai,    1, "anim/116/button");                          // Engine elec. contr. (L) (norm)
            _DO(1, XPLMSetDatai,    1, "anim/117/button");                          // Engine elec. contr. (R) (norm)
            _DO(1, XPLMSetDatai,    1, "anim/154/button");                          // Engine autostart switch (on)
            _DO(1, XPLMSetDatai,    1, "anim/134/button");                          // Air: upper recirc. fans (on)
            _DO(1, XPLMSetDatai,    1, "anim/135/button");                          // Air: lower recirc. fans (on)
            _DO(1, XPLMSetDatai,    1, "anim/137/button");                          // Air: L t. air valve sw. (on)
            _DO(1, XPLMSetDatai,    1, "anim/138/button");                          // Air: R t. air valve sw. (on)
            _DO(1, XPLMSetDatai,    1, "anim/139/button");                          // Air: L bleed ISLN valve (auto)
            _DO(1, XPLMSetDatai,    1, "anim/140/button");                          // Air: C bleed ISLN valve (auto)
            _DO(1, XPLMSetDatai,    1, "anim/141/button");                          // Air: R bleed ISLN valve (auto)
            _DO(1, XPLMSetDatai,    1, "anim/143/button");                          // Air: APU  bleed air sw. (auto)
            _DO(1, XPLMSetDatai,    1, "anim/142/button");                          // Air: L e. bleed air sw. (on)
            _DO(1, XPLMSetDatai,    1, "anim/144/button");                          // Air: R e. bleed air sw. (on)
            _DO(1, XPLMSetDatai,    1, "anim/25/switch");                           // Cargo temp. cont. (aft) (low)
            _DO(1, XPLMSetDatai,    1, "anim/26/switch");                           // Cargo temp. cont. (blk) (low)
            _DO(1, XPLMSetDataf, 0.5f, "anim/5/rotery");                            // Temp. control (f. deck) (auto)
            _DO(1, XPLMSetDataf, 0.5f, "anim/6/rotery");                            // Temp. control (all ca.) (auto)
            break;

        case NVP_ACF_EMBE_SS:
            for (int i = 0; i < XPLMCountHotKeys(); i++)
            {
                char outDescr[513];
                XPLMHotKeyID h_key = XPLMGetNthHotKey(i);
                XPLMGetHotKeyInfo(h_key, NULL, NULL, outDescr, NULL);
                if (strncasecmp(outDescr, "SHIFT F8", 8) == 0)
                {
                    XPLMSetHotKeyCombination(h_key, XPLM_VK_F14, xplm_DownFlag);
                }
                if (strncasecmp(outDescr, "F8", 2) == 0)
                {
                    /* This awful hack ain't going anywhere, GG SSG :-( */
                    XPLMSetHotKeyCombination(h_key, XPLM_VK_F15, xplm_DownFlag);
                    break;
                }
            }
            _DO(1, XPLMSetDatai, 1, "ssg/EJET/GND/rain_hide_sw");                   // Disable custom rain effects
            _DO(1, XPLMSetDatai, 0, "ssg/EJET/GND/stair1_ON");                      // Hide passenger stairs
            _DO(1, XPLMSetDatai, 0, "ssg/EJET/GND/seats_hide_sw");                  // Hide captain's seat
            _DO(1, XPLMSetDatai, 0, "ssg/EJET/GND/yokes_hide_sw");                  // Hide both yokes
            break;

        case NVP_ACF_EMBE_XC:
            if ((d_ref = XPLMFindDataRef("sim/cockpit2/electrical/generator_on")))
            {
                int generator_on[1] = { 1, };
                XPLMSetDatavi(d_ref, &generator_on[0], 0, 1);
                XPLMSetDatavi(d_ref, &generator_on[0], 1, 1);
                _DO(1, XPLMSetDatai, 1, "sim/cockpit2/electrical/APU_generator_on");
            }
            if ((d_ref = XPLMFindDataRef("sim/cockpit2/electrical/inverter_on")))
            {
                int inverter_on[1] = { 1, };
                XPLMSetDatavi(d_ref, &inverter_on[0], 0, 1);
                XPLMSetDatavi(d_ref, &inverter_on[0], 1, 1);
                _DO(1, XPLMSetDatai, 1, "sim/cockpit2/electrical/cross_tie");
            }
            if ((d_ref = XPLMFindDataRef("sim/cockpit2/ice/ice_inlet_heat_on_per_engine")))
            {
                int ice_inlet_heat_on_per_engine[1] = { 1, };
                XPLMSetDatavi(d_ref, &ice_inlet_heat_on_per_engine[0], 0, 1);
                XPLMSetDatavi(d_ref, &ice_inlet_heat_on_per_engine[0], 1, 1);
                _DO(1, XPLMSetDatai, 1, "sim/cockpit2/ice/ice_surfce_heat_on");
                _DO(1, XPLMSetDatai, 1, "sim/cockpit2/ice/ice_window_heat_on");
            }
            if ((d_ref = XPLMFindDataRef("sim/cockpit2/switches/generic_lights_switch")))
            {
                float generic_lights_switch[1] = { 1.0f, };
                XPLMSetDatavf(d_ref, &generic_lights_switch[0], 14, 1); // pack 1
                XPLMSetDatavf(d_ref, &generic_lights_switch[0], 15, 1); // pack 2
                XPLMSetDatavf(d_ref, &generic_lights_switch[0], 28, 1); // bleed1
                XPLMSetDatavf(d_ref, &generic_lights_switch[0], 29, 1); // bleed2
            }
            _DO(0, XPLMSetDatai, 2, "sim/cockpit2/radios/actuators/HSI_source_select_copilot");
            _DO(0, XPLMSetDatai, 2, "sim/cockpit2/radios/actuators/HSI_source_select_pilot");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_airport_on");
            _DO(0, XPLMSetDatai, 0, "sim/cockpit2/EFIS/EFIS_fix_on");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_ndb_on");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_vor_on");
            _DO(0, XPLMSetDatai, 4, "sim/cockpit2/EFIS/map_range");
            break;

        case NVP_ACF_SSJ1_RZ:
        {
            // check avionics state, enable and delay processing if required
            XPLMDataRef av_pwr_on = XPLMFindDataRef("sim/cockpit2/switches/avionics_power_on");
            if (NULL == av_pwr_on)
            {
                XPLMSpeakString("nav P first call failed");
                return (ctx->first_fcall = 0) - 1;
            }
            if (ctx->first_fcall > +0 && !XPLMGetDatai(av_pwr_on))
            {
                ctx->first_fcall = -2; // pass 1, enable pass 2
                XPLMSetDatai(av_pwr_on, 1); return 0;
            }

            // items that require avionics power to change state
            _DO(1, XPLMSetDataf, 0.0f, "but/temp/airL");
            _DO(1, XPLMSetDataf, 0.0f, "but/temp/airR");
            _DO(1, XPLMSetDataf, 0.0f, "but/temp/heater");
            _DO(1, XPLMSetDataf, 1.0f, "but/fuel/pumpLt");
            _DO(1, XPLMSetDataf, 1.0f, "but/fuel/pumpRt");
            _DO(1, XPLMSetDataf, 1.0f, "but/sim/cockpit2/fuel/fuel_tank_pump_on0");
            _DO(1, XPLMSetDataf, 1.0f, "but/sim/cockpit2/fuel/fuel_tank_pump_on1");
            _DO(1, XPLMSetDataf, 1.0f, "but/sim/cockpit2/fuel/fuel_tank_pump_on2");
            _DO(1, XPLMSetDataf, 1.0f, "but/sim/cockpit2/fuel/fuel_tank_pump_on3");
            _DO(1, XPLMSetDataf, 1.0f, "but/sim/cockpit2/fuel/fuel_tank_pump_on4");
            if (ctx->first_fcall == -2)
            {
                ctx->first_fcall += -1; return 0; // pass 2, enable pass 3
            }

            // everything else
            _DO(1, XPLMSetDataf, 1.0f, "door/lock");
            _DO(1, XPLMSetDataf, 1.0f, "elec/cap1");
            _DO(1, XPLMSetDataf, 1.0f, "elec/cap2");
            _DO(1, XPLMSetDataf, 1.0f, "elec/cap3");
            _DO(1, XPLMSetDataf, 1.0f, "elec/cap4");
            _DO(1, XPLMSetDataf, 1.0f, "elec/cap8");
            _DO(1, XPLMSetDataf, 0.0f, "prep/lock1");
            _DO(1, XPLMSetDataf, 0.0f, "prep/lock2");
            _DO(1, XPLMSetDataf, 0.0f, "prep/lock3");
            _DO(1, XPLMSetDataf, 0.0f, "prep/lock4");
            _DO(1, XPLMSetDataf, 0.0f, "prep/lock5");
            _DO(1, XPLMSetDataf, 0.0f, "prep/lock6");
            _DO(1, XPLMSetDataf, .75f, "lights/brt");
            _DO(1, XPLMSetDataf, 0.5f, "lights/flood");
            _DO(1, XPLMSetDataf, 0.5f, "but/lights/pilot");
            if (ctx->first_fcall == -3)
            {
                XPLMSetDatai(av_pwr_on, 0); // pass 3, avionics back to off
            }
            if ((cr = XPLMFindCommand("shortcuts/hydraulic_system_auto_tgl")))
            {
                XPLMCommandOnce(cr);
            }

            // datarefs: X-Plane default
            _DO(0, XPLMSetDatai, 2, "sim/cockpit2/radios/actuators/HSI_source_select_copilot");
            _DO(0, XPLMSetDatai, 2, "sim/cockpit2/radios/actuators/HSI_source_select_pilot");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_1_selection_copilot");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_2_selection_copilot");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_1_selection_pilot");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_2_selection_pilot");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_airport_on");
            _DO(0, XPLMSetDatai, 0, "sim/cockpit2/EFIS/EFIS_fix_on");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_ndb_on");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_vor_on");
            _DO(0, XPLMSetDatai, 4, "sim/cockpit2/EFIS/map_range");
            _DO(0, XPLMSetDatai, 2, "sim/cockpit2/EFIS/map_mode");
            break;
        }

        case NVP_ACF_HA4T_RW:
            _DO(1, XPLMSetDatai, 0, "Hawker4000/hideshow/car");
            _DO(1, XPLMSetDatai, 1, "Hawker4000/yoke/hide_show");
            _DO(1, XPLMSetDatai, 1, "Hawker4000/bleed/l_bleed_b");
            _DO(1, XPLMSetDatai, 1, "Hawker4000/bleed/r_bleed_b");
            _DO(1, XPLMSetDatai, 0, "Hawker4000/pref/wingflex_b");
            _DO(1, XPLMSetDatai, 0, "Hawker4000/pref/map_detail_b");
            _DO(1, XPLMSetDatai, 0, "Hawker4000/hideshow/chockscones");
            _DO(1, XPLMSetDatai, 0, "Hawker4000/hideshow/engine_cover");
            _DO(1, XPLMSetDatai, 1, "Hawker4000/gear/anti_skid_button");
            _DO(1, XPLMSetDatai, 1, "Hawker4000/pilot/elec/l_gen_button");
            _DO(1, XPLMSetDatai, 1, "Hawker4000/pilot/elec/r_gen_button");
//          _DO(1, XPLMSetDatai, 3, "Hawker4000/pilot/pilot_BRG_1_CYCLE");      // unwritable
//          _DO(1, XPLMSetDatai, 0, "Hawker4000/pilot/pilot_BRG_2_CYCLE");      // unwritable
            _DO(0, XPLMSetDatai, 2, "sim/cockpit2/radios/actuators/HSI_source_select_copilot");
            _DO(0, XPLMSetDatai, 2, "sim/cockpit2/radios/actuators/HSI_source_select_pilot");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_airport_on");
            _DO(0, XPLMSetDatai, 0, "sim/cockpit2/EFIS/EFIS_fix_on");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_ndb_on");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_vor_on");
            _DO(0, XPLMSetDatai, 4, "sim/cockpit2/EFIS/map_range");
            break;

        case NVP_ACF_MD80_RO:
            if ((d_ref = XPLMFindDataRef("sim/cockpit2/switches/panel_brightness_ratio")))
            {
                float panel_brightness_ratio[2] = { 0.5f, .25f, };
                XPLMSetDatavf(d_ref, &panel_brightness_ratio[0], 1, 1);             // Inst. lights intensity control
                XPLMSetDatavf(d_ref, &panel_brightness_ratio[1], 3, 1);             // Flood lights intensity control
            }
            if ((d_ref = XPLMFindDataRef("sim/cockpit2/switches/instrument_brightness_ratio")))
            {
                float instrument_brightness_ratio[1] = { 1.0f, };
                XPLMSetDatavf(d_ref, &instrument_brightness_ratio[0], 0, 1);        // LED readouts intensity control
            }
            _DO(1, XPLMSetDatai, 1, "Rotate/md80/electrical/GPU_power_available");
            _DO(1, XPLMSetDatai, 1, "Rotate/md80/misc/hide_yoke_button_clicked");
            _DO(1, XPLMSetDatai, 2, "Rotate/md80/instruments/nav_display_range");
            _DO(1, XPLMSetDatai, 2, "Rotate/md80/instruments/nav_display_mode");
            break;

        case NVP_ACF_GENERIC:
            // note: this path is always non-verbose (never warn for unapplicable datarefs)
            // datarefs: Aerobask
            _DO(0, XPLMSetDatai, 1, "sim/har/reflets");                             // LEG2: refl. off
            _DO(0, XPLMSetDatai, 0, "sim/har/pitchservo");                          // LEG2: 500ft/min
            _DO(0, XPLMSetDatai, 0, "aerobask/E1000/flags_on");                     // EPIC
            _DO(0, XPLMSetDatai, 1, "aerobask/E1000/yokeL_hidden");                 // EPIC
            _DO(0, XPLMSetDatai, 1, "aerobask/E1000/yokeR_hidden");                 // EPIC
            _DO(0, XPLMSetDatai, 0, "aerobask/E1000/reflections_skyview_on");       // EPIC
            _DO(0, XPLMSetDatai, 0, "aerobask/E1000/reflections_windshield_on");    // EPIC
            _DO(0, XPLMSetDatai, 0, "aerobask/eclipse/flags_on");                   // EA50
            _DO(0, XPLMSetDatai, 0, "aerobask/eclipse/reflections_skyview_on");     // EA50
            _DO(0, XPLMSetDatai, 0, "aerobask/eclipse/reflections_windshield_on");  // EA50
            _DO(0, XPLMSetDatai, 0, "aerobask/victory/flags_on");                   // EVIC
            _DO(0, XPLMSetDatai, 1, "aerobask/victory/yokeL_hidden");               // EVIC
            _DO(0, XPLMSetDatai, 1, "aerobask/victory/yokeR_hidden");               // EVIC
            _DO(0, XPLMSetDatai, 0, "aerobask/victory/reflections_skyview_on");     // EVIC
            _DO(0, XPLMSetDatai, 0, "aerobask/victory/reflections_windshield_on");  // EVIC
            _DO(0, XPLMSetDatai, 0, "aerobask/panthera/flags_on");                  // PIPA
            _DO(0, XPLMSetDatai, 0, "aerobask/panthera/reflections_skyview_on");    // PIPA
            _DO(0, XPLMSetDatai, 0, "aerobask/panthera/reflections_windshield_on"); // PIPA
            _DO(0,XPLMSetDataf,0.0f,"aerobask/panthera/reflections_EFIS");          // PIPA
            _DO(0,XPLMSetDataf,0.0f,"aerobask/panthera/reflections_annunciators");  // PIPA
            // datarefs: Alabeo, Carenado, SimCoders REP
            _DO(0, XPLMSetDatai, 1, "com/dkmp/HideYokeL");                          // various aircraft
            _DO(0, XPLMSetDatai, 1, "com/dkmp/HideYokeR");                          // various aircraft
            _DO(0, XPLMSetDatai, 0, "thranda/views/InstRefl");                      // various aircraft
            _DO(0, XPLMSetDatai, 0, "thranda/views/WindowRefl");                    // various aircraft
            _DO(0, XPLMSetDatai, 0, "thranda/views/staticelements");                // various aircraft
            _DO(0, XPLMSetDatai, 0, "simcoders/rep/staticelements/visible");        // all REP packages
            _DO(0, XPLMSetDatai, 1, "thranda/cockpit/actuators/HideYokeL");         // various aircraft
            _DO(0, XPLMSetDatai, 1, "thranda/cockpit/actuators/HideYokeR");         // various aircraft
            if ((d_ref = XPLMFindDataRef("com/dkmp/WindowRefl")) &&
                (xplmType_Int & XPLMGetDataRefTypes(d_ref)))
            {
                _DO(0, XPLMSetDatai, 0, "com/dkmp/InstrRefl");
                _DO(0, XPLMSetDatai, 0, "com/dkmp/WindowRefl");
            }
            else if (((d_ref = XPLMFindDataRef("com/dkmp/windowrefl"))))
            {
                if ((XPLMGetDataRefTypes(d_ref) & xplmType_Int))
                {
                    _DO(0, XPLMSetDatai,    0, "com/dkmp/windowrefl");
                }
                else
                {
                    _DO(0, XPLMSetDataf, 0.0f, "com/dkmp/windowrefl");
                }
            }
            if ((d_ref = XPLMFindDataRef("com/dkmp/tintedwindows")))
            {
                if ((XPLMGetDataRefTypes(d_ref) & xplmType_Int))
                {
                    _DO(0, XPLMSetDatai,    0, "com/dkmp/tintedwindows");
                }
                else
                {
                    _DO(0, XPLMSetDataf, 0.0f, "com/dkmp/tintedwindows");
                }
            }
            if (XPLMFindDataRef("com/dkmp/static")    ||
                XPLMFindDataRef("thranda/views/bush") ||
                XPLMFindDataRef("com/dkmp/staticelements"))
            {
                _DO(0, XPLMSetDatai, 1, "thranda/views/bush");                      // C207
                _DO(0, XPLMSetDatai, 1, "com/dkmp/VCWindows");                      // C208
                _DO(0, XPLMSetDatai, 1, "com/dkmp/cargopod");                       // C208
                _DO(0, XPLMSetDatai, 0, "com/dkmp/static");                         // various aircraft
                _DO(0, XPLMSetDatai, 0, "com/dkmp/staticelements");                 // various aircraft
                if (strcasecmp(ctx->icao, "C404"))
                {
                    _DO(0, XPLMSetDatai, 1, "sim/cockpit2/switches/no_smoking");        // HideYokeL
                    _DO(0, XPLMSetDatai, 1, "sim/cockpit2/switches/fasten_seat_belts"); // HideYokeR
                }
            }
            if (!strcasecmp(ctx->icao, "BE20"))
            {
                _DO(0, XPLMSetDataf, 1.0f, "com/dkmp/winglets");
                _DO(0, XPLMSetDataf, 1.0f, "com/dkmp/PassengerDoorHandle");
            }
            if (!strcasecmp(ctx->icao, "C206"))
            {
                _DO(0, XPLMSetDatai, 1, "com/dkmp/fairings");
                _DO(0, XPLMSetDatai, 0, "com/dkmp/InstRefl");
                _DO(0, XPLMSetDatai, 1, "com/dkmp/WindowRefl"); // inverted
            }
            if (!strcasecmp(ctx->icao, "TBM8"))
            {
                _DO(0, XPLMSetDataf, -0.5f, "thranda/cockpit/actuators/VisorL");
                _DO(0, XPLMSetDataf, -0.5f, "thranda/cockpit/actuators/VisorR");
                _DO(0, XPLMSetDataf,  1.0f, "thranda/cockpit/actuators/VisorSwingL");
                _DO(0, XPLMSetDataf,  1.0f, "thranda/cockpit/actuators/VisorSwingR");
                _DO(0, XPLMSetDataf,  0.0f, "thranda/cockpit/actuators/VisorSlideL");
                _DO(0, XPLMSetDataf,  0.0f, "thranda/cockpit/actuators/VisorSlideR");
            }
            // datarefs: DDenn Challenger 300
            _DO(0, XPLMSetDatai, 1, "cl300/fms/alt_rep");
            _DO(0, XPLMSetDatai, 1, "cl300/hide_pilots");
            // datarefs: X-Plane default
            _DO(0, XPLMSetDatai, 2, "sim/cockpit2/radios/actuators/HSI_source_select_copilot");
            _DO(0, XPLMSetDatai, 2, "sim/cockpit2/radios/actuators/HSI_source_select_pilot");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_1_selection_copilot");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_2_selection_copilot");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_1_selection_pilot");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_2_selection_pilot");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_airport_on");
            _DO(0, XPLMSetDatai, 0, "sim/cockpit2/EFIS/EFIS_fix_on");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_ndb_on");
            _DO(0, XPLMSetDatai, 1, "sim/cockpit2/EFIS/EFIS_vor_on");
            _DO(0, XPLMSetDatai, 4, "sim/cockpit2/EFIS/map_range");
            if (ctx->auth[0] && ctx->desc[0])
            {
                if (!STRN_CASECMP_AUTO(ctx->auth, "Alabeo") ||
                    !STRN_CASECMP_AUTO(ctx->auth, "Carenado"))
                {
                    if (!STRN_CASECMP_AUTO(ctx->desc, "Pilatus PC12"))
                    {
                        // make the aircraft less tail-heavy to improve ground handling
                        _DO(0, XPLMSetDataf, -0.30f, "sim/aircraft/overflow/acf_cgZ_fwd");
                        _DO(0, XPLMSetDataf, -0.20f, "sim/flightmodel/misc/cgz_ref_to_default");
                        _DO(0, XPLMSetDataf, +0.10f, "sim/aircraft/overflow/acf_cgZ_aft");
                        // let's also skip drawing the FMS line from the HSI display's map
                        _DO(1, XPLMSetDatai, 1, "sim/graphics/misc/kill_map_fms_line");
                        // and fully declutter the HSI/Avidyne displays by default
                        _DO(0, XPLMSetDatai, 0, "com/dkmp/Avidyne/Declutter");
                    }
                    if (!STRN_CASECMP_AUTO(ctx->desc, "C207 Skywagon"))
                    {
                        // make the aircraft less tail-heavy to improve ground handling
                        _DO(0, XPLMSetDataf, -0.20f, "sim/aircraft/overflow/acf_cgZ_fwd");
                        _DO(0, XPLMSetDataf, -0.10f, "sim/flightmodel/misc/cgz_ref_to_default");
                        _DO(0, XPLMSetDataf, +0.00f, "sim/aircraft/overflow/acf_cgZ_aft");
                    }
                    if (!STRN_CASECMP_AUTO(ctx->desc, "C404 Titan"))
                    {
                        if ((d_ref = XPLMFindDataRef("sim/cockpit2/switches/instrument_brightness_ratio")))
                        {
                            float instrument_brightness_ratio[1] = { 0.5f, };
                            XPLMSetDatavf(d_ref, &instrument_brightness_ratio[0], 4, 1); // autopilot/warning annunciator brightness
                        }
                    }
                    if (!STRN_CASECMP_AUTO(ctx->desc, "Bonanza V35B"))
                    {
                        if ((d_ref = XPLMFindDataRef("sim/cockpit2/switches/custom_slider_on")))
                        {
                            int tip_tanks_enabled[1] = { 1, };
                            XPLMSetDatavi(d_ref, &tip_tanks_enabled[0], 10, 1);
                        }
                    }
                }
                if (!STRN_CASECMP_AUTO(ctx->auth, "Aerobask") ||
                    !STRN_CASECMP_AUTO(ctx->auth, "Stephane Buon"))
                {
                    if (!STRN_CASECMP_AUTO(ctx->desc, "Pipistrel Panthera"))
                    {
                        __skyview = 1;
                        _DO(0, XPLMSetDatai,      0, "sim/cockpit2/autopilot/airspeed_is_mach");
                        _DO(0, XPLMSetDataf, 120.0f, "sim/cockpit2/autopilot/airspeed_dial_kts_mach");
                    }
                    if (!STRN_CASECMP_AUTO(ctx->desc, "Epic E1000") ||
                        !STRN_CASECMP_AUTO(ctx->desc, "Epic Victory"))
                    {
                        __skyview = 1;
                        _DO(0, XPLMSetDatai,      0, "sim/cockpit2/autopilot/airspeed_is_mach");
                        _DO(0, XPLMSetDataf, 160.0f, "sim/cockpit2/autopilot/airspeed_dial_kts_mach");
                        _DO(0, XPLMSetDatai,      0, "sim/cockpit2/pressurization/actuators/bleed_air_mode");
                        _DO(0, XPLMSetDatai,      0, "sim/cockpit2/ice/ice_pitot_heat_on_pilot");
                        _DO(0, XPLMSetDatai,      0, "sim/cockpit2/EFIS/EFIS_weather_on");
                        _DO(0, XPLMSetDatai,      0, "sim/cockpit2/EFIS/EFIS_tcas_on");
                        _DO(0, XPLMSetDatai,      0, "sim/cockpit2/ice/ice_detect_on");
                    }
                    if (!STRN_CASECMP_AUTO(ctx->desc, "The Eclipse 550"))
                    {
                        __skyview = 1;
                        _DO(0, XPLMSetDatai,      0, "sim/cockpit2/autopilot/airspeed_is_mach");
                        _DO(0, XPLMSetDataf, 160.0f, "sim/cockpit2/autopilot/airspeed_dial_kts_mach");
                        _DO(0, XPLMSetDatai,      0, "sim/cockpit2/pressurization/actuators/bleed_air_mode");
                    }
                }
            }
            if (__skyview == 0) // don't mess w/Aerobask's WXR radar
            {
                _DO(0, XPLMSetDatai, 2, "sim/cockpit2/EFIS/map_mode");
            }
            if (ctx->revrs.n_engines == 1) // single-engine: select default fuel tank
            {
                if ((d_ref = XPLMFindDataRef("sim/aircraft/overflow/acf_has_fuel_all")))
                {
                    int acf_has_fuel_all, acf_num_tanks = 0; float rat[9];
                    {
                        acf_has_fuel_all = XPLMGetDatai(d_ref);
                    }
                    if ((d_ref = XPLMFindDataRef("sim/aircraft/overflow/acf_tank_rat")))
                    {
                        for (int i = 0, j = XPLMGetDatavf(d_ref, rat, 0, 9); i < j; i++)
                        {
                            if (rat[i] > .01f)
                            {
                                acf_num_tanks++;
                            }
                        }
                        if (acf_num_tanks == 4 && (fabsf(rat[0] - rat[1]) < .01f &&
                                                   fabsf(rat[2] - rat[3]) < .01f))
                        {
                            acf_num_tanks -= 2; // e.g. Bonanza V35B w/tip tanks
                        }
                        if (acf_num_tanks == 2)
                        {
                            if (acf_has_fuel_all) // can draw from all tanks at once
                            {
                                _DO(0, XPLMSetDatai, 4, "sim/cockpit2/fuel/fuel_tank_selector");
                            }
                            else
                            {
                                _DO(0, XPLMSetDatai, 1, "sim/cockpit2/fuel/fuel_tank_selector");
                                _DO(0, XPLMSetDatai, 0, "aerobask/panthera/fuel_position");
                                _DO(0, XPLMSetDatai, 1, "aerobask/victory/fuel_position");
                                _DO(0, XPLMSetDatai, 1, "aerobask/E1000/fuel_position");
                                _DO(0, XPLMSetDatai, 0, "sim/har/fueltank");
                            }
                        }
                    }
                }
            }
            break;

        default:
            break;
    }

    /* resolve addon-specific references early (might be faster?) */
    chandler_command *list[] =
    {
        &ctx->otto.conn.cc,
        &ctx->otto.disc.cc,
        &ctx->athr.disc.cc,
        &ctx->athr.toga.cc,
        &ctx->bking.rc_brk.rg,
        &ctx->bking.rc_brk.mx,
        NULL,
    };
    for (int i = 0; list[i]; i++)
    {
        if (list[i]->name && list[i]->xpcr == NULL)
        {
            list[i]->xpcr = XPLMFindCommand(list[i]->name);
        }
    }
    if (ctx->atyp == NVP_ACF_B757_FF || ctx->atyp == NVP_ACF_B767_FF)
    {
        ctx->otto.ffst.dr = XPLMFindDataRef("1-sim/AP/cmd_L_Button");
    }
    if (ctx->gear.has_retractable_gear == -1)
    {
        ctx->gear.has_retractable_gear = !!XPLMGetDatai(ctx->gear.acf_gear_retract);
    }

    /*
     * Custom ground stabilization system (via flight loop callback)
     *
     * Minimum ground throttle detent.
     * Testing parameters:
     * - weather: CAVOK preset
     * - runways: follow terrain contour OFF
     * - airport: KNTD (Naval Base Ventura County)
     * - taxiing: ideal peak speed ~20.0 Knots ground speed
     * - pistons: minimum propeller speed ~1,000.0 r/minute
     */
    switch (ctx->atyp)
    {
        case NVP_ACF_A320_QP:
            ctx->ground.idle.thrott_array = XPLMFindDataRef("AirbusFBW/throttle_input");
            ctx->ground.idle.r_t[0]   = 0.10875f; // ~26.1% N1 @ NTD
            ctx->ground.idle.r_t[1]   = 0.10875f; // ~26.1% N1 @ NTD
            ctx->ground.idle.minimums = 1; break;

        case NVP_ACF_A330_RW:
            ctx->ground.idle.thrott_array = XPLMFindDataRef("AirbusFBW/throttle_input");
            ctx->ground.idle.r_t[0]   = 0.10625f; // ~30.1% N1 @ NTD (CF6 engine)
            ctx->ground.idle.r_t[1]   = 0.10625f; // ~30.1% N1 @ NTD (CF6 engine)
            ctx->ground.idle.minimums = 1; break;

        case NVP_ACF_A350_FF:
            ctx->ground.idle.thrott_array = XPLMFindDataRef("AirbusFBW/throttle_input");
            ctx->ground.idle.r_t[0]   = 0.10000f; // ~25.3% N1 @ NTD
            ctx->ground.idle.r_t[1]   = 0.10000f; // ~25.3% N1 @ NTD
            ctx->ground.idle.minimums = 1; break;

        // will only work so long as you don't touch any hardware throttles
        // in this particular aircraft during any given X-Plane session :-(
        // also the QPAC-equivalent dataref isn't writable in this aircraft
        case NVP_ACF_A380_PH:
            ctx->ground.idle.r_taxi   = 0.17500f; // ~29.7% N1 @ NTD
            ctx->ground.idle.minimums = 1; break;

        case NVP_ACF_B737_XG:
            ctx->ground.idle.r_taxi   = 0.13333f; // ~33.3% N1 @ NTD
            ctx->ground.idle.minimums = 1; break;

        case NVP_ACF_B757_FF:
            if ((d_ref = XPLMFindDataRef("757Avionics/engine")))
            {
                switch (XPLMGetDatai(d_ref))
                {
                    case 0: // Pratt & Whitney
                        ctx->ground.idle.r_taxi   = 0.09000f; // ~26.1% N1 @ NTD
                        ctx->ground.idle.minimums = 1; break;
                    case 1: // Rolls-Royce
                        ctx->ground.idle.r_taxi   = 0.15555f; // ~26.1% N1 @ NTD
                        ctx->ground.idle.minimums = 1; break;
                    default:
                        ndt_log("navP [warning]: couldn't determine engine type for FF757\n");
                        break;
                }
                break;
            }
            ndt_log("navP [warning]: couldn't obtain engine type data reference for FF757\n");
            break;

        case NVP_ACF_B767_FF:
            if ((d_ref = XPLMFindDataRef("757Avionics/engine")))
            {
                switch (XPLMGetDatai(d_ref))
                {
                    case 0: // Pratt & Whitney
                        ctx->ground.idle.r_taxi   = 0.16666f; // ~26.1% N1 @ NTD
                        ctx->ground.idle.minimums = 1; break;
                    default:
                        ndt_log("navP [warning]: couldn't determine engine type for FF767\n");
                        break;
                }
                break;
            }
            ndt_log("navP [warning]: couldn't obtain engine type data reference for FF767\n");
            break;

        case NVP_ACF_B777_FF:
            if ((d_ref = XPLMFindDataRef("1-sim/engineType")))
            {
                switch (XPLMGetDatai(d_ref))
                {
                    case 1: // General Electric
                        ctx->ground.idle.r_taxi   = 0.09765f; // ~28.1% N1 @ NTD
                        ctx->ground.idle.minimums = 1; break;
                    default:
                        ndt_log("navP [warning]: couldn't determine engine type for FF777\n");
                        break;
                }
                break;
            }
            ndt_log("navP [warning]: couldn't obtain engine type data reference for FF777\n");
            break;

        case NVP_ACF_EMBE_XC:
            ctx->ground.idle.r_taxi   = 0.09100f; // ~33.3% N1 @ NTD
            ctx->ground.idle.minimums = 1; break;

        case NVP_ACF_HA4T_RW:
            ctx->ground.idle.r_taxi   = 0.16666f; // ~35.7% N1 @ NTD
            ctx->ground.idle.minimums = 1; break;

        default:
        {
            if (XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("com.simcoders.rep"))
            {
                if (!STRN_CASECMP_AUTO(ctx->icao, "BE33") ||
                    !STRN_CASECMP_AUTO(ctx->icao, "BE35"))
                {
                    ctx->ground.idle.r_idle   = 0.06666f;
                    ctx->ground.idle.r_taxi   = 0.16666f;
                    ctx->ground.idle.minimums = 2; break;
                }
                if (!STRN_CASECMP_AUTO(ctx->icao, "BE58"))
                {
                    ctx->ground.idle.r_idle   = 0.08333f;
                    ctx->ground.idle.r_taxi   = 0.16666f;
                    ctx->ground.idle.minimums = 2; break;
                }
                break;
            }
            if (!STRN_CASECMP_AUTO(ctx->auth, "Aerobask") ||
                !STRN_CASECMP_AUTO(ctx->auth, "Stephane Buon"))
            {
                if (!STRN_CASECMP_AUTO(ctx->desc, "Lancair Legacy FG"))
                {
                    ctx->ground.idle.r_idle   = 0.02000f;
                    ctx->ground.idle.r_taxi   = 0.06666f;
                    ctx->ground.idle.minimums = 2; break;
                }
                if (!STRN_CASECMP_AUTO(ctx->desc, "Pipistrel Panthera"))
                {
                    ctx->ground.idle.r_idle   = 0.04166f;
                    ctx->ground.idle.r_taxi   = 0.11111f;
                    ctx->ground.idle.minimums = 2; break;
                }
                if (!STRN_CASECMP_AUTO(ctx->desc, "Epic Victory"))
                {
                    ctx->ground.idle.r_taxi   = 0.16666f; // ~45.0% N1 @ NTD
                    ctx->ground.idle.minimums = 1; break;
                }
                if (!STRN_CASECMP_AUTO(ctx->desc, "The Eclipse 550"))
                {
                    ctx->ground.idle.r_taxi   = 0.23875f; // ~50.0% N1 @ NTD
                    ctx->ground.idle.minimums = 1; break;
                }
                break;
            }
            if (!STRN_CASECMP_AUTO(ctx->auth, "Alabeo") ||
                !STRN_CASECMP_AUTO(ctx->auth, "Carenado"))
            {
                if (!STRN_CASECMP_AUTO(ctx->desc, "CT206H Stationair"))
                {
                    ctx->ground.idle.r_idle   = 0.06666f;
                    ctx->ground.idle.r_taxi   = 0.13333f;
                    ctx->ground.idle.minimums = 2; break;
                }
                if (!STRN_CASECMP_AUTO(ctx->desc, "C207 Skywagon"))
                {
                    ctx->ground.idle.r_idle   = 0.03333f;
                    ctx->ground.idle.r_taxi   = 0.13333f;
                    ctx->ground.idle.minimums = 2; break;
                }
                if (!STRN_CASECMP_AUTO(ctx->desc, "T210M Centurion II"))
                {
                    ctx->ground.idle.r_taxi   = 0.13333f;
                    ctx->ground.idle.minimums = 1; break;
                }
                if (!STRN_CASECMP_AUTO(ctx->desc, "Pilatus PC12"))
                {
                    ctx->ground.idle.r_taxi   = 0.33333f;
                    ctx->ground.idle.minimums = 1; break;
                }
                break;
            }
            break;
        }
    }
    if (ctx->ground.flc_g)
    {
        XPLMUnregisterFlightLoopCallback(ctx->ground.flc_g, &ctx->ground);
    }
    XPLMRegisterFlightLoopCallback((ctx->ground.flc_g = &gnd_stab_hdlr), 1, &ctx->ground);

#define TIM_ONLY
#ifdef  TIM_ONLY
    /*
     * Kill X-Plane ATC (not needed, may/may not cause crashes in some places).
     * Do it as late as possible (avoid interfering w/init. of X-Plane itself).
     * Doing it too early might have caused a crash in the PilotEdge plugin :(
     *
     * This is all very much guesswork, really :-(
     */
    _DO(1, XPLMSetDataf, 1.0f, "sim/private/controls/perf/kill_atc");

    /*
     * Sound and related datarefs.
     *
     * NOTE: NVPmenu.c, menu_handler() has code covering the same functionality,
     * we must always remember to update said code too when making changes here
     *
     * Default to 25% volume for all planes.
     */
    if (ctx->atyp == NVP_ACF_B737_XG)
    {
        XPLMSetDataf(ctx->volumes.atc, 0.50f);
    }
    else
    {
        if ((ctx->volumes.tmp = XPLMFindDataRef("aerobask/eclipse/custom_volume_ratio")) &&
            (!STRN_CASECMP_AUTO(ctx->desc, "The Eclipse 550")))
        {
            XPLMSetDataf(ctx->volumes.evr, 0.10f); // engines are a bit loud in this plane :(
            XPLMSetDataf(ctx->volumes.tmp, 0.25f); // all other custom sounds (excl. engines)
        }
        else
        {
            if ((ctx->volumes.tmp = XPLMFindDataRef("volumeX")))
            {
                XPLMSetDataf(ctx->volumes.tmp, 0.10f); // FlightFactor master slider
            }
            if ((ctx->volumes.tmp = XPLMFindDataRef("com/dkmp/mastervolknob")))
            {
                XPLMSetDataf(ctx->volumes.tmp, 0.25f); // Carenado 3.0 master slider
            }
            XPLMSetDataf(ctx->volumes.evr, 0.25f);
        }
        if (ctx->atyp == NVP_ACF_A320ULT)
        {
            // TODO
        }
        XPLMSetDataf(ctx->volumes.wxr, 0.25f);
        XPLMSetDataf(ctx->volumes.wvr, 0.25f);
        XPLMSetDataf(ctx->volumes.gvr, 0.25f);
        XPLMSetDataf(ctx->volumes.pvr, 0.25f);
        XPLMSetDataf(ctx->volumes.fvr, 0.25f);
        XPLMSetDataf(ctx->volumes.atc, 0.50f);
    }
    XPLMSetDatai(ctx->volumes.snd, 1); // ALL sounds
    XPLMSetDatai(ctx->volumes.txt, 1); // "text" ATC
    XPLMSetDatai(ctx->volumes.spc, 0); // verbal ATC

    /*
     * Slight frame rate bump, but it makes clouds a bit more transparent :|
     */
    if (ctx->menu_context)
    {
        nvp_menu_tachy(ctx->menu_context, xplm_Menu_Checked);
    }
#endif

    return (ctx->first_fcall = 0);
}

static int aibus_fbw_init(refcon_qpacfbw *fbw)
{
    if (fbw && fbw->ready == 0)
    {
        if ((fbw->h_b_max = XPLMFindCommand("1-sim/brakes_max"    )) &&
            (fbw->h_b_reg = XPLMFindCommand("1-sim/brakes_regular")))
        {
            if ((fbw->pkb_ref = XPLMFindDataRef("1-sim/parckBrake")))
            {
                /*
                 * We're good to go!
                 */
                (fbw->ready = 1); return 0;
            }
        }
        if ((fbw->h_b_max = XPLMFindCommand("sim/flight_controls/brakes_max"    )) &&
            (fbw->h_b_reg = XPLMFindCommand("sim/flight_controls/brakes_regular")))
        {
            if ((fbw->pkb_ref = XPLMFindDataRef("com/petersaircraft/airbus/ParkBrake")))
            {
                /*
                 * Peter's A380 seems less twitchy than the other QPACs…
                 */
                (fbw->ready = 1); return 0;
            }
            if ((fbw->pkb_ref = XPLMFindDataRef("AirbusFBW/ParkBrake")))
            {
                /*
                 * The QPAC A320 and RWDesigns's braking is so twitchy, we
                 * simply bypass braking commands altogether; instead use
                 * the parking brake toggle directly…
                 */
                (fbw->h_b_max = fbw->h_b_reg = NULL);

                /*
                 * We're (finally) good to go!
                 */
                (fbw->ready = 1); return 0;
            }
        }
        return -1;
    }
    return 0;
}

static int boing_733_init(refcon_ixeg733 *i33)
{
    if (i33 && i33->ready == 0)
    {
        if ((i33->slat = XPLMFindDataRef("ixeg/733/hydraulics/speedbrake_act")))
        {
            (i33->ready = 1); return 0;
        }
        return -1;
    }
    return 0;
}

static int boing_738_init(refcon_eadt738 *x38)
{
    if (x38 && x38->ready == 0)
    {
        if ((x38->sparm = XPLMFindCommand("x737/speedbrakes/SPEEDBRAKES_ARM" )) &&
            (x38->spret = XPLMFindCommand("x737/speedbrakes/SPEEDBRAKES_DOWN")) &&
            (x38->pt_up = XPLMFindCommand("x737/trim/CAPT_STAB_TRIM_UP_ALL"  )) &&
            (x38->pt_dn = XPLMFindCommand("x737/trim/CAPT_STAB_TRIM_DOWN_ALL")))
        {
            (x38->ready = 1); return 0;
        }
        return -1;
    }
    return 0;
}

static int ff_assert_init(refcon_assert1 *ffa)
{
    if (ffa && !ffa->initialized)
    {
        XPLMSendMessageToPlugin(ffa->plugin_id, XPLM_FF_MSG_GET_SHARED_INTERFACE, &ffa->api);
        if (ffa->api.DataVersion == NULL || ffa->api.DataAddUpdate == NULL)
        {
            return EAGAIN;
        }
#if 0
        /*
         * For YFMS
         *
         * s32 Aircraft.FMGS.FCU1.Lateral                           heading     DEG
         * s32 Aircraft.FMGS.FCU1.Vertical                          v/speed     100FT
         * s32 Aircraft.FMGS.FCU1.Altitude                          altitude    100FT
         * s32 Aircraft.FMGS.FCU1.Speed                             airspeed    KTS/0.01M
         * s32 Aircraft.FMGS.FCU1.SpeedIsMach                                   boolean
         * s32 Aircraft.Cockpit.Panel.FCU_Mach.Click                            boolean
         * s32 Aircraft.FMGS.FCU1.BaroL                             pressure    100INHG/1HPA
         * s32 Aircraft.FMGS.FCU1.BaroR                             pressure    100INHG/1HPA
         * s32 Aircraft.FMGS.FCU1.BaroModeL                         QNH2/QFE1,STD:QNH-2/QFE-1
         * s32 Aircraft.FMGS.FCU1.BaroModeR                         QNH2/QFE1,STD:QNH-2/QFE-1
         * s32 Aircraft.FMGS.FCU1.BaroTypeL                         InHg/hPa    0/1
         * s32 Aircraft.FMGS.FCU1.BaroTypeR                         InHg/hPa    0/1
         * u32 Aircraft.Cockpit.Pedestal.TCAS_Traffic.Target        index       0-based
         * u32 Aircraft.Cockpit.Pedestal.ATC_Alt.Target             index       0-based
         * u32 Aircraft.Cockpit.Pedestal.ATC_Mode.Target            index       0-based
         * u32 Aircraft.Navigation.ATC.CodeSet                      squawk      4-digit
         */
        {
            const char *valueDescription;
            char tmp[2048], fullname[2048];
            int vID, valueID, parentValueID;
            unsigned int valueType, valueFlags;
            ndt_log("navP [debug] =======================\n");
            unsigned int valuesCount = ffa->api.ValuesCount();
            ndt_log("navP [debug]: valuesCount: %u\n", valuesCount);
            for (unsigned int ii = 0; ii < valuesCount; ii++)
            {
                vID = valueID         = ffa->api.ValueIdByIndex (ii);
                valueType             = ffa->api.ValueType (valueID);
                valueFlags            = ffa->api.ValueFlags(valueID);
                valueDescription      = ffa->api.ValueDesc (valueID);
                sprintf(fullname, "%s", ffa->api.ValueName (valueID));
                while (valueID && (parentValueID = ffa->api.ValueParent(valueID)) >= 0)
                {
                    valueID = parentValueID;
                    sprintf(tmp, "%s", fullname);
                    sprintf(fullname, "%s.%s", ffa->api.ValueName(valueID), tmp);
                }
                ndt_log("navP [debug]: ID: %d, name: \"%s\", desc: \"%s\", type: %u, flags: %u\n", vID, fullname, valueDescription, valueType, valueFlags);
            }
            ndt_log("navP [debug] =======================\n");
        }
#endif

        /* Initialize the aircraft's data references via the provided API */
        ffa->dat.ldg_gears_lever         = XPLMFindDataRef       ("model/controls/gears_lever");
        ffa->dat.id_flt_pedal_brake_left = ffa->api.ValueIdByName("Aircraft.Cockpit.Panel.BrakesL");
        ffa->dat.id_flt_pedal_brake_rigt = ffa->api.ValueIdByName("Aircraft.Cockpit.Panel.BrakesR");
        ffa->dat.p_brk_toggle            = XPLMFindCommand       ("sim/flight_controls/brakes_toggle_max");
        ffa->dat.id_u32_efis_nav_mod_lft = ffa->api.ValueIdByName("Aircraft.Cockpit.Panel.EFIS_NavModeL.Target");
        ffa->dat.id_u32_efis_nav_mod_rgt = ffa->api.ValueIdByName("Aircraft.Cockpit.Panel.EFIS_NavModeR.Target");
        ffa->dat.id_u32_efis_nav_rng_lft = ffa->api.ValueIdByName("Aircraft.Cockpit.Panel.EFIS_NavRangeL.Target");
        ffa->dat.id_u32_efis_nav_rng_rgt = ffa->api.ValueIdByName("Aircraft.Cockpit.Panel.EFIS_NavRangeR.Target");
        ffa->dat.id_u32_fcu_tgt_alt_step = ffa->api.ValueIdByName("Aircraft.Cockpit.Panel.FCU_AltitudeStep.Target");
        ffa->dat.id_u32_emer_lights_mode = ffa->api.ValueIdByName("Aircraft.Cockpit.Overhead.LightEmerMode.Target");
        ffa->dat.id_s32_click_thr_disc_l = ffa->api.ValueIdByName("Aircraft.Cockpit.Pedestal.EngineDisconnect1.Click");
        ffa->dat.id_s32_click_ss_tkovr_l = ffa->api.ValueIdByName("Aircraft.Cockpit.Panel.SidestickTakeoverL.Click");
        ffa->dat.id_s32_click_autopilot1 = ffa->api.ValueIdByName("Aircraft.Cockpit.Panel.FCU_AutoPilot1.Click");
        if (ffa->dat.id_flt_pedal_brake_left <= 0 ||
            ffa->dat.id_flt_pedal_brake_rigt <= 0 ||
            ffa->dat.id_u32_efis_nav_mod_lft <= 0 ||
            ffa->dat.id_u32_efis_nav_mod_rgt <= 0 ||
            ffa->dat.id_u32_efis_nav_rng_lft <= 0 ||
            ffa->dat.id_u32_efis_nav_rng_rgt <= 0 ||
            ffa->dat.id_u32_fcu_tgt_alt_step <= 0 ||
            ffa->dat.id_u32_emer_lights_mode <= 0 ||
            ffa->dat.id_s32_click_thr_disc_l <= 0 ||
            ffa->dat.id_s32_click_ss_tkovr_l <= 0 ||
            ffa->dat.id_s32_click_autopilot1 <= 0 ||
            ffa->dat.ldg_gears_lever      == NULL ||
            ffa->dat.p_brk_toggle         == NULL)
        {
            ndt_log("navP [debug]: ff_assert_init: can't find required data\n");
            return EINVAL;
        }
        ffa->initialized = 1; return 0;
    }
    return 0;
}

static int priv_getdata_i(void *inRefcon)
{
    return *((int*)inRefcon);
}

static void priv_setdata_i(void *inRefcon, int inValue)
{
    *((int*)inRefcon) = inValue;
}

static float priv_getdata_f(void *inRefcon)
{
    return *((float*)inRefcon);
}

static void priv_setdata_f(void *inRefcon, float inValue)
{
    *((float*)inRefcon) = inValue;
}

#undef REGISTER_CHANDLER
#undef UNREGSTR_CHANDLER
#undef CALLOUT_PARKBRAKE
#undef CALLOUT_SPEEDBRAK
#undef CALLOUT_FLAPLEVER
#undef CALLOUT_GEARLEVER
#undef STRN_CASECMP_AUTO
#undef ACF_ROLL_SET
#undef GS_KT_MIN
#undef GS_KT_MID
#undef GS_KT_MAX
#undef T_ZERO
#undef _DO
