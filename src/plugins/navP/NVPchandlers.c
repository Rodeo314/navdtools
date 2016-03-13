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
#include <stdlib.h>
#include <string.h>

#include "XPLM/XPLMDataAccess.h"
#include "XPLM/XPLMPlanes.h"
#include "XPLM/XPLMPlugin.h"
#include "XPLM/XPLMUtilities.h"

#include "common/common.h"

#include "NVPchandlers.h"

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
    int           ready;
    XPLMDataRef pkb_ref;
} refcon_ff_a350;

typedef struct
{
    int              ready;
    int            pkb_var;
    XPLMDataRef    pkb_tmp;
    XPLMDataRef    pkb_ref;
    XPLMCommandRef h_b_max;
    XPLMCommandRef h_b_reg;
} refcon_qpacfbw;

typedef struct
{
    refcon_ff_a350 a350;
    refcon_qpacfbw qpac;
    XPLMDataRef p_b_rat;
    XPLMDataRef l_b_rat;
    XPLMDataRef r_b_rat;
    int         acf_typ;
} refcon_braking;

typedef struct
{
    int initialized;

    enum
    {
        NVP_ACF_GENERIC = 0x0000000,
        NVP_ACF_A320_JD = 0x0000010,
        NVP_ACF_A332_JD = 0x0000020,
        NVP_ACF_A320_QP = 0x0000100,
        NVP_ACF_A333_RW = 0x0000200,
        NVP_ACF_A350_FF = 0x0001000,
        NVP_ACF_B752_FF = 0x0010000,
        NVP_ACF_B763_FF = 0x0100000,
        NVP_ACF_B77L_FF = 0x1000000,
    } atyp;
#define NVP_ACF_MASK_JDN  0x00000F0 // all J.A.R.Design addons
#define NVP_ACF_MASK_QPC  0x000FF00 // all QPAC-powered addons
#define NVP_ACF_MASK_FFR  0xFFFF000 // all FlightFactor addons
#define NVP_ACF_MASK_32x  0x0000110 // all A320 series aircraft
#define NVP_ACF_MASK_33x  0x0000220 // all A330 series aircraft
#define NVP_ACF_MASK_35x  0x000F000 // all A350 series aircraft
#define NVP_ACF_MASK_75x  0x00F0000 // all B757 series aircraft
#define NVP_ACF_MASK_76x  0x0F00000 // all B767 series aircraft
#define NVP_ACF_MASK_77x  0xF000000 // all B777 series aircraft

    struct
    {
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
} chandler_context;

static int  dataref_read_string(XPLMDataRef dataref, char *string_buffer,  size_t buffer_size);
static int  chandler_p_max(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int  chandler_p_off(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int  chandler_b_max(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int  chandler_b_reg(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int  chandler_swtch(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon);
static int  aibus_350_init(                                               refcon_ff_a350 *ffa);
static int  aibus_fbw_init(                                               refcon_qpacfbw *fbw);
static int  priv_getdata_i(                                       void *inRefcon             );
static void priv_setdata_i(                                       void *inRefcon, int inValue);

void* nvp_chandlers_init(void)
{
    chandler_context *ctx = calloc(1, sizeof(chandler_context));
    if (!ctx)
    {
        return NULL;
    }

    /* Custom commands: braking */
    ctx->bking.prk.cb.command = XPLMCreateCommand("navP/brakes/parking", "apply max. park brake");
    ctx->bking.off.cb.command = XPLMCreateCommand("navP/brakes/release", "release parking brake");
    ctx->bking.max.cb.command = XPLMCreateCommand("navP/brakes/maximum", "maximum braking action");
    ctx->bking.reg.cb.command = XPLMCreateCommand("navP/brakes/regular", "regular braking action");
    ctx->bking.rc_brk.p_b_rat = XPLMFindDataRef  ("sim/cockpit2/controls/parking_brake_ratio");
    ctx->bking.rc_brk.l_b_rat = XPLMFindDataRef  ("sim/cockpit2/controls/left_brake_ratio");
    ctx->bking.rc_brk.r_b_rat = XPLMFindDataRef  ("sim/cockpit2/controls/right_brake_ratio");
    if (!ctx->bking.prk.cb.command || !ctx->bking.off.cb.command                      ||
        !ctx->bking.max.cb.command || !ctx->bking.reg.cb.command                      ||
        !ctx->bking.rc_brk.p_b_rat || !XPLMCanWriteDataRef(ctx->bking.rc_brk.l_b_rat) ||
        !ctx->bking.rc_brk.l_b_rat || !XPLMCanWriteDataRef(ctx->bking.rc_brk.l_b_rat) ||
        !ctx->bking.rc_brk.r_b_rat || !XPLMCanWriteDataRef(ctx->bking.rc_brk.r_b_rat))
    {
        goto fail;
    }
    else
    {
        XPLMRegisterCommandHandler((ctx->bking.prk.cb.command),
                                   (ctx->bking.prk.cb.handler = &chandler_p_max),
                                   (ctx->bking.prk.cb.before  = 0),
                                   (ctx->bking.prk.cb.refcon  = &ctx->bking.rc_brk));
        XPLMRegisterCommandHandler((ctx->bking.off.cb.command),
                                   (ctx->bking.off.cb.handler = &chandler_p_off),
                                   (ctx->bking.off.cb.before  = 0),
                                   (ctx->bking.off.cb.refcon  = &ctx->bking.rc_brk));
        XPLMRegisterCommandHandler((ctx->bking.max.cb.command),
                                   (ctx->bking.max.cb.handler = &chandler_b_max),
                                   (ctx->bking.max.cb.before  = 0),
                                   (ctx->bking.max.cb.refcon  = &ctx->bking.rc_brk));
        XPLMRegisterCommandHandler((ctx->bking.reg.cb.command),
                                   (ctx->bking.reg.cb.handler = &chandler_b_reg),
                                   (ctx->bking.reg.cb.before  = 0),
                                   (ctx->bking.reg.cb.refcon  = &ctx->bking.rc_brk));
    }

    /* Custom commands: autopilot and autothrottle */
    ctx->otto.disc.cb.command = XPLMCreateCommand("navP/switches/ap_disc", "A/P disconnect");
    ctx->athr.disc.cb.command = XPLMCreateCommand("navP/switches/at_disc", "A/T disconnect");
    ctx->athr.toga.cb.command = XPLMCreateCommand("navP/switches/at_toga", "A/T takeoff/GA");
    if (!ctx->otto.disc.cb.command ||
        !ctx->athr.disc.cb.command ||
        !ctx->athr.toga.cb.command)
    {
        goto fail;
    }
    else
    {
        XPLMRegisterCommandHandler((ctx->otto.disc.cb.command),
                                   (ctx->otto.disc.cb.handler = &chandler_swtch),
                                   (ctx->otto.disc.cb.before  = 0),
                                   (ctx->otto.disc.cb.refcon  = &ctx->otto.disc.cc));
        XPLMRegisterCommandHandler((ctx->athr.disc.cb.command),
                                   (ctx->athr.disc.cb.handler = &chandler_swtch),
                                   (ctx->athr.disc.cb.before  = 0),
                                   (ctx->athr.disc.cb.refcon  = &ctx->athr.disc.cc));
        XPLMRegisterCommandHandler((ctx->athr.toga.cb.command),
                                   (ctx->athr.toga.cb.handler = &chandler_swtch),
                                   (ctx->athr.toga.cb.before  = 0),
                                   (ctx->athr.toga.cb.refcon  = &ctx->athr.toga.cc));
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

    /* unregister all handlers */
    if (ctx->bking.prk.cb.handler)
    {
        XPLMUnregisterCommandHandler(ctx->bking.prk.cb.command,
                                     ctx->bking.prk.cb.handler,
                                     ctx->bking.prk.cb.before,
                                     ctx->bking.prk.cb.refcon);
        ctx->bking.prk.cb.handler = NULL;
    }
    if (ctx->bking.off.cb.handler)
    {
        XPLMUnregisterCommandHandler(ctx->bking.off.cb.command,
                                     ctx->bking.off.cb.handler,
                                     ctx->bking.off.cb.before,
                                     ctx->bking.off.cb.refcon);
        ctx->bking.off.cb.handler = NULL;
    }
    if (ctx->bking.reg.cb.handler)
    {
        XPLMUnregisterCommandHandler(ctx->bking.max.cb.command,
                                     ctx->bking.max.cb.handler,
                                     ctx->bking.max.cb.before,
                                     ctx->bking.max.cb.refcon);
        ctx->bking.max.cb.handler = NULL;
    }
    if (ctx->bking.reg.cb.handler)
    {
        XPLMUnregisterCommandHandler(ctx->bking.reg.cb.command,
                                     ctx->bking.reg.cb.handler,
                                     ctx->bking.reg.cb.before,
                                     ctx->bking.reg.cb.refcon);
        ctx->bking.reg.cb.handler = NULL;
    }
    if (ctx->otto.disc.cb.handler)
    {
        XPLMUnregisterCommandHandler(ctx->otto.disc.cb.command,
                                     ctx->otto.disc.cb.handler,
                                     ctx->otto.disc.cb.before,
                                     ctx->otto.disc.cb.refcon);
        ctx->otto.disc.cb.handler = NULL;
    }
    if (ctx->athr.disc.cb.handler)
    {
        XPLMUnregisterCommandHandler(ctx->athr.disc.cb.command,
                                     ctx->athr.disc.cb.handler,
                                     ctx->athr.disc.cb.before,
                                     ctx->athr.disc.cb.refcon);
        ctx->athr.disc.cb.handler = NULL;
    }
    if (ctx->athr.toga.cb.handler)
    {
        XPLMUnregisterCommandHandler(ctx->athr.toga.cb.command,
                                     ctx->athr.toga.cb.handler,
                                     ctx->athr.toga.cb.before,
                                     ctx->athr.toga.cb.refcon);
        ctx->athr.toga.cb.handler = NULL;
    }

    /* …and all datarefs */
    if (ctx->bking.rc_brk.qpac.pkb_tmp)
    {
        XPLMUnregisterDataAccessor(ctx->bking.rc_brk.qpac.pkb_tmp);
        ctx->bking.rc_brk.qpac.pkb_tmp = NULL;
    }

    /* all good */
    free(ctx);
    return 0;
}

int nvp_chandlers_reset(void *inContext)
{
    chandler_context *ctx = inContext;
    if (!ctx)
    {
        return -1;
    }

    /* Reset the aircraft/addon type */
    ctx->atyp = NVP_ACF_GENERIC;

    /* Don't use 3rd-party commands/datarefs until we know the plane we're in */
    ctx->bking.rc_brk.a350.ready = 0;
    ctx->bking.rc_brk.qpac.ready = 0;
    ctx->   athr.disc.cc.name = NULL;
    ctx->   athr.toga.cc.name = NULL;
    ctx->   otto.disc.cc.name = NULL;

    /* all good */
    ndt_log("navP [info]: nvp_chandlers_reset OK\n"); return (ctx->initialized = 0);
}

int nvp_chandlers_update(void *inContext)
{
    XPLMDataRef xdref_acf_ICAO = NULL;
    char xaircraft_icao_code[41] = "";
//  char acf_file[257], acf_path[513];
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

    /* get the aircraft path and model information */
    if ((xdref_acf_ICAO = XPLMFindDataRef("sim/aircraft/view/acf_ICAO")))
    {
        dataref_read_string(xdref_acf_ICAO, xaircraft_icao_code, sizeof(xaircraft_icao_code));
    }
//  XPLMGetNthAircraftModel(XPLM_USER_AIRCRAFT, acf_file, acf_path);

    /* check enabled plugins to determine which plane we're flying */
    do // dummy loop we can break out of
    {
        if (XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("QPAC.airbus.fbw"))
        {
            if (!strlen(xaircraft_icao_code))
            {
                if (XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("FFSTSmousehandler"))
                {
                    ndt_log("navP [info]: plane is FlightFactor-QPAC Airbus A350 XWB Advanced\n");
                    ctx->atyp = NVP_ACF_A350_FF;
                    break;
                }
                ndt_log("navP [info]: plane is RWDesigns-QPAC Airbus A330-300\n");
                ctx->atyp = NVP_ACF_A333_RW;
                break;
            }
            if (!strcasecmp(xaircraft_icao_code, "A320"))
            {
                ndt_log("navP [info]: plane is QPAC Airbus A320-200 IAE\n");
                ctx->atyp = NVP_ACF_A320_QP;
                break;
            }
            ndt_log("navP [warning]: no aircraft type match despite plugin (QPAC.airbus.fbw)\n");
            break; // fall back to generic
        }
        if (XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("de-ru.philippmuenzel-den_rain.757avionics") ||
            XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("ru.flightfactor-steptosky.757767avionics"))
        {
            if (!strcasecmp(xaircraft_icao_code, "B763"))
            {
                ndt_log("navP [info]: plane is FlightFactor-StepToSky Boeing 767 Professional\n");
                ctx->atyp = NVP_ACF_B763_FF;
                break;
            }
            // TODO: future FlightFactor 757 and 767 addons
            ndt_log("navP [warning]: no aircraft type match despite plugin (757767avionics)\n");
            break; // fall back to generic
        }
        if (XPLM_NO_PLUGIN_ID != XPLMFindPluginBySignature("de.philippmuenzel.t7avionics"))
        {
            if (!strcasecmp(xaircraft_icao_code, "B77L"))
            {
                ndt_log("navP [info]: plane is FlightFactor Boeing 777 Worldliner Professional\n");
                ctx->atyp = NVP_ACF_B77L_FF;
                break;
            }
            // TODO: other FlightFactor T7 addons
            ndt_log("navP [warning]: no aircraft type match despite plugin (t7avionics)\n");
            break; // fall back to generic
        }
    }
    while (0);

    /* we know the aircraft type, propagate it where it's needed */
    ctx->bking.rc_brk.acf_typ = ctx->atyp;

    /* all good */
    switch (ctx->atyp)
    {
        case NVP_ACF_A320_QP:
            ctx->otto.disc.cc.name = "sim/autopilot/fdir_servos_down_one";
            ctx->athr.disc.cc.name = "sim/autopilot/autothrottle_off";
            break;

        case NVP_ACF_A333_RW:
            ctx->otto.disc.cc.name = "sim/autopilot/fdir_servos_down_one";
            ctx->athr.disc.cc.name = "sim/autopilot/autothrottle_off";
            break;

        case NVP_ACF_A350_FF:
            ctx->otto.disc.cc.name = "sim/autopilot/fdir_servos_down_one";
            ctx->athr.disc.cc.name = "sim/autopilot/autothrottle_off";
            break;

        case NVP_ACF_B763_FF:
            ctx->otto.disc.cc.name = "1-sim/comm/AP/ap_disc";
            ctx->athr.disc.cc.name = "1-sim/comm/AP/at_disc";
            ctx->athr.toga.cc.name = "1-sim/comm/AP/at_toga";
            break;

        case NVP_ACF_B77L_FF:
            ctx->otto.disc.cc.name = "777/ap_disc";
            ctx->athr.disc.cc.name = "777/at_disc";
            ctx->athr.toga.cc.name = "777/at_toga";
            break;

        case NVP_ACF_GENERIC:
        default:
            ctx->otto.disc.cc.name = "sim/autopilot/fdir_servos_down_one";
            ctx->athr.disc.cc.name = "sim/autopilot/autothrottle_off";
            ndt_log("navP [info]: plane is generic\n");
            break;
    }
    ndt_log("navP [info]: nvp_chandlers_update OK\n"); XPLMSpeakString("command handlers OK"); return 0;
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

/*
 * action: set or unset parking brake.
 *
 * rationale: X-Plane only has a toggle for this :(
 */
static int chandler_p_max(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    refcon_braking *rc = inRefcon;
    if (rc->acf_typ == NVP_ACF_A350_FF)
    {
        if (rc->a350.ready == 0)
        {
            aibus_350_init(&rc->a350);
        }
        if (rc->a350.ready && inPhase == xplm_CommandEnd)
        {
            XPLMSetDatai(rc->a350.pkb_ref, 0); // inverted
        }
        return 0;
    }
    if (rc->acf_typ & NVP_ACF_MASK_QPC)
    {
        if (rc->qpac.ready == 0)
        {
            aibus_fbw_init(&rc->qpac);
        }
        if (rc->qpac.ready && inPhase == xplm_CommandEnd)
        {
            XPLMSetDatai(rc->qpac.pkb_ref, 1);
            XPLMSetDatai(rc->qpac.pkb_tmp, 1);
        }
        return 0;
    }
    if (inPhase == xplm_CommandEnd)
    {
        XPLMSetDataf(rc->p_b_rat, 1.0f);
    }
    return 0;
}

static int chandler_p_off(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    refcon_braking *rc = inRefcon;
    if (rc->acf_typ == NVP_ACF_A350_FF)
    {
        if (rc->a350.ready == 0)
        {
            aibus_350_init(&rc->a350);
        }
        if (rc->a350.ready && inPhase == xplm_CommandEnd)
        {
            XPLMSetDatai(rc->a350.pkb_ref, 1); // inverted
        }
        return 0;
    }
    if (rc->acf_typ & NVP_ACF_MASK_QPC)
    {
        if (rc->qpac.ready == 0)
        {
            aibus_fbw_init(&rc->qpac);
        }
        if (rc->qpac.ready && inPhase == xplm_CommandEnd)
        {
            XPLMSetDatai(rc->qpac.pkb_ref, 0);
            XPLMSetDatai(rc->qpac.pkb_tmp, 0);
        }
        return 0;
    }
    if (inPhase == xplm_CommandEnd)
    {
        XPLMSetDataf(rc->p_b_rat, 0.0f);
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
    refcon_braking *rc = inRefcon;
    if (rc->acf_typ & NVP_ACF_MASK_QPC)
    {
        if (rc->qpac.ready == 0)
        {
            aibus_fbw_init(&rc->qpac);
        }
        if (rc->qpac.ready)
        {
            switch (inPhase)
            {
                case xplm_CommandBegin:
                    XPLMSetDatai(rc->qpac.pkb_tmp, XPLMGetDatai(rc->qpac.pkb_ref));
                    XPLMSetDatai(rc->qpac.pkb_ref, 1);
                    XPLMCommandBegin(rc->qpac.h_b_max);
                    break;
                case xplm_CommandEnd:
                    XPLMCommandEnd(rc->qpac.h_b_max);
                    XPLMSetDatai(rc->qpac.pkb_ref, XPLMGetDatai(rc->qpac.pkb_tmp));
                    break;
                default:
                    break;
            }
        }
        return 0;
    }
    /*
     * Using 90% of max. braking makes the command behave the same
     * as brakes_regular, i.e. it doesn't unset the parking brake :-)
     */
    switch (inPhase)
    {
        case xplm_CommandBegin:
            XPLMSetDataf(rc->l_b_rat, 0.9f);
            XPLMSetDataf(rc->r_b_rat, 0.9f);
            break;
        case xplm_CommandContinue:
            XPLMSetDataf(rc->l_b_rat, 0.9f);
            XPLMSetDataf(rc->r_b_rat, 0.9f);
            break;
        case xplm_CommandEnd:
        default:
            XPLMSetDataf(rc->l_b_rat, 0.0f);
            XPLMSetDataf(rc->r_b_rat, 0.0f);
            break;
    }
    return 0;
}

static int chandler_b_reg(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void *inRefcon)
{
    refcon_braking *rc = inRefcon;
    if (rc->acf_typ & NVP_ACF_MASK_QPC)
    {
        if (rc->qpac.ready == 0)
        {
            aibus_fbw_init(&rc->qpac);
        }
        if (rc->qpac.ready)
        {
            switch (inPhase)
            {
                case xplm_CommandBegin:
                    XPLMSetDatai(rc->qpac.pkb_tmp, XPLMGetDatai(rc->qpac.pkb_ref));
                    XPLMSetDatai(rc->qpac.pkb_ref, 1);
                    XPLMCommandBegin(rc->qpac.h_b_reg);
                    break;
                case xplm_CommandEnd:
                    XPLMCommandEnd(rc->qpac.h_b_reg);
                    XPLMSetDatai(rc->qpac.pkb_ref, XPLMGetDatai(rc->qpac.pkb_tmp));
                    break;
                default:
                    break;
            }
        }
        return 0;
    }
    switch (inPhase)
    {
        case xplm_CommandBegin:
            XPLMSetDataf(rc->l_b_rat, 0.5f);
            XPLMSetDataf(rc->r_b_rat, 0.5f);
            break;
        case xplm_CommandContinue:
            XPLMSetDataf(rc->l_b_rat, 0.5f);
            XPLMSetDataf(rc->r_b_rat, 0.5f);
            break;
        case xplm_CommandEnd:
        default:
            XPLMSetDataf(rc->l_b_rat, 0.0f);
            XPLMSetDataf(rc->r_b_rat, 0.0f);
            break;
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
    chandler_command *cc = inRefcon;
    if (cc->name)
    {
        if (cc->xpcr == NULL)
        {
            cc->xpcr = XPLMFindCommand(cc->name);
        }
        if (cc->xpcr && inPhase == xplm_CommandEnd)
        {
            XPLMCommandOnce(cc->xpcr);
        }
    }
    return 0;
}

static int aibus_350_init(refcon_ff_a350 *ffa)
{
    if (ffa && ffa->ready == 0)
    {
        if ((ffa->pkb_ref = XPLMFindDataRef("1-sim/parckBrake")))
        {
            (ffa->ready = 1);
        }
    }
    return 0;
}

static int aibus_fbw_init(refcon_qpacfbw *fbw)
{
    if (fbw && fbw->ready == 0)
    {
        if (fbw->pkb_tmp == NULL)
        {
            fbw->pkb_tmp = XPLMRegisterDataAccessor("navP/refcon_qpacfbw/pkb_tmp",
                                                    xplmType_Int, 1,
                                                    &priv_getdata_i,
                                                    &priv_setdata_i,
                                                    NULL, NULL, NULL, NULL, NULL,
                                                    NULL, NULL, NULL, NULL, NULL,
                                                    &fbw->pkb_var, &fbw->pkb_var);
        }
        fbw->pkb_ref = XPLMFindDataRef("AirbusFBW/ParkBrake");
        fbw->h_b_max = XPLMFindCommand("sim/flight_controls/brakes_max");
        fbw->h_b_reg = XPLMFindCommand("sim/flight_controls/brakes_regular");
        if (fbw->pkb_tmp && fbw->pkb_ref && fbw->h_b_max && fbw->h_b_reg)
        {
            fbw->ready = 1;
        }
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
