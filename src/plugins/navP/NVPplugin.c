/*
 * NVPplugin.c
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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "XPLM/XPLMDefs.h"
#include "XPLM/XPLMPlanes.h"
#include "XPLM/XPLMPlugin.h"
#include "XPLM/XPLMUtilities.h"

#include "common/common.h"

#include "NVPchandlers.h"
#include "NVPmenu.h"
#include "YFSmain.h"

/* Logging callback */
static int log_with_sdk(const char *format, va_list ap);

/* Miscellaneous data */
void         *chandler_context = NULL;
void         *navpmenu_context = NULL;
yfms_context *navpyfms_context = NULL;

#define PLUGIN_NAME "navP"      // or "YFMS"
#define INTRO_SPEAK "nav P OK"  // or "Y FMS OK"

#if IBM
#include <windows.h>
BOOL APIENTRY DllMain(HANDLE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
#endif

PLUGIN_API int XPluginStart(char *outName,
                            char *outSig,
                            char *outDesc)
{
    strncpy(outName, PLUGIN_NAME,            255);
    strncpy(outSig,  "Rodeo314."PLUGIN_NAME, 255);
    strncpy(outDesc, "Yet Another X-Plugin", 255);

    /* set ndt_log callback so we write everything to the X-Plane log */
    ndt_log_set_callback(&log_with_sdk);

    /* Initialize command handling context */
    if ((chandler_context = nvp_chandlers_init()) == NULL)
    {
        return 0;
    }

#if APL
    /* use native (POSIX) paths under OS X */
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);
#endif

    /* all good */
#ifdef NDT_VERSION
    XPLMDebugString(PLUGIN_NAME " [info]: version " NDT_VERSION "\n");
#else
    XPLMDebugString(PLUGIN_NAME " [info]: unknown version :-(\n");
#endif
    XPLMDebugString(PLUGIN_NAME " [info]: XPluginStart OK\n"); return 1;
}

PLUGIN_API void XPluginStop(void)
{
    /* close command handling context */
    if (chandler_context)
    {
        nvp_chandlers_close(&chandler_context);
    }

    /* unset ndt_log callback */
    ndt_log_set_callback(NULL);
}

PLUGIN_API int XPluginEnable(void)
{
    /* navP features a menu :-) */
    if ((navpmenu_context = nvp_menu_init()) == NULL)
    {
        return 0; // menu creation failed :(
    }

    /* and an FMS, too! */
    if ((navpyfms_context = yfs_main_init()) == NULL)
    {
        return 0; // menu creation failed :(
    }

    /* all good */
    if (XPLMFindPluginBySignature("x-fmc.com") == XPLM_NO_PLUGIN_ID)
    {
        XPLMSpeakString(INTRO_SPEAK);
    }
    XPLMDebugString(PLUGIN_NAME " [info]: XPluginEnable OK\n"); return 1;
}

PLUGIN_API void XPluginDisable(void)
{
    /* reset command handlers */
    nvp_chandlers_reset(chandler_context);

    /* kill the menu */
    if (navpmenu_context) nvp_menu_close(&navpmenu_context);

    /* and the FMS */
    if (navpyfms_context) yfs_main_close(&navpyfms_context);

    /* all good */
    XPLMDebugString(PLUGIN_NAME " [info]: XPluginDisable OK\n");
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFromWho,
                                      long         inMessage,
                                      void        *inParam)
{
    switch (inMessage)
    {
        case XPLM_MSG_PLANE_CRASHED:
            break;

        case XPLM_MSG_PLANE_LOADED:
            if (inParam == XPLM_USER_AIRCRAFT) // user's plane changed
            {
                nvp_menu_reset     (navpmenu_context);
                nvp_chandlers_reset(chandler_context);
            }
            break;

        case XPLM_MSG_AIRPORT_LOADED:
            nvp_menu_reset(navpmenu_context);
            break;

        case XPLM_MSG_SCENERY_LOADED:
            nvp_menu_reset(navpmenu_context);
            break;

        case XPLM_MSG_AIRPLANE_COUNT_CHANGED:
            break;

        case XPLM_MSG_PLANE_UNLOADED:
            break;

        case XPLM_MSG_WILL_WRITE_PREFS:
            break;

        case XPLM_MSG_LIVERY_LOADED:
            if (inParam == XPLM_USER_AIRCRAFT) // custom plugins loaded
            {
                nvp_menu_setup      (navpmenu_context);
                nvp_chandlers_update(chandler_context);
            }
            break;

        default:
            break;
    }
}

static int log_with_sdk(const char *format, va_list ap)
{
    int ret;
    char string[1024];
    ret = vsnprintf(string, sizeof(string), format, ap);
    XPLMDebugString(string);
    return ret;
}
