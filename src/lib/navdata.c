/*
 * navdata.c
 *
 * This file is part of the navdtools source code.
 *
 * (C) Copyright 2014 Timothy D. Walker and others.
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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "common/common.h"
#include "common/list.h"

#include "compat/compat.h"

#include "airport.h"
#include "airway.h"
#include "navdata.h"
#include "ndb_xpgns.h"
#include "waypoint.h"

static int compare_apt(const void *apt1, const void *apt2);
static int compare_awy(const void *awy1, const void *awy2);
static int compare_wpt(const void *wpt1, const void *wpt2);

ndt_navdatabase* ndt_navdatabase_init(const char *ndr, ndt_navdataformat fmt)
{
    int  err = 0;
    char errbuf[64];

    ndt_navdatabase *ndb = calloc(1, sizeof(ndt_navdatabase));
    if (!ndb)
    {
        err = ENOMEM;
        goto end;
    }

    ndb->airports  = ndt_list_init();
    ndb->airways   = ndt_list_init();
    ndb->waypoints = ndt_list_init();

    if (!ndb->airports || !ndb->airways || !ndb->waypoints || !ndr)
    {
        err = ENOMEM;
        goto end;
    }

    switch (fmt)
    {
        case NDT_NAVDFMT_XPGNS:
            if ((err = ndt_ndb_xpgns_navdatabase_init(ndb, ndr)))
            {
                goto end;
            }
            break;

        case NDT_NAVDFMT_OTHER:
        default:
            err = -1;
            goto end;
    }

    /*
     * While the navdata is usually already sorted, we don't know how, and some
     * lists are compiled from several source files, so we need to re-sort all
     * lists using a known method which can then be used for retrieving items.
     */
    ndt_list_sort(ndb->airports,  sizeof(ndt_airport*),  &compare_apt);
    ndt_list_sort(ndb->airways,   sizeof(ndt_airway*),   &compare_awy);
    ndt_list_sort(ndb->waypoints, sizeof(ndt_waypoint*), &compare_wpt);

end:
    if (err)
    {
        ndt_navdatabase_close(&ndb);
        strerror_r(err, errbuf, sizeof(errbuf));
        fprintf(stderr, "navdata: failed to open database (%s)\n", errbuf);
    }
    return ndb;
}

void ndt_navdatabase_close(ndt_navdatabase **_ndb)
{
    if (_ndb && *_ndb)
    {
        size_t i;
        ndt_navdatabase *ndb = *_ndb;

        if (ndb->airports)
        {
            while ((i = ndt_list_count(ndb->airports)))
            {
                ndt_airport *apt = ndt_list_item(ndb->airports, i-1);
                ndt_list_rem                    (ndb->airports, apt);
                ndt_airport_close               (              &apt);
            }
            ndt_list_close(&ndb->airports);
        }

        if (ndb->airways)
        {
            while ((i = ndt_list_count(ndb->airways)))
            {
                ndt_airway* awy = ndt_list_item(ndb->airways, i-1);
                ndt_list_rem                   (ndb->airways, awy);
                ndt_airway_close               (             &awy);
            }
            ndt_list_close(&ndb->airways);
        }

        if (ndb->waypoints)
        {
            while ((i = ndt_list_count(ndb->waypoints)))
            {
                ndt_waypoint *wpt = ndt_list_item(ndb->waypoints, i-1);
                ndt_list_rem                     (ndb->waypoints, wpt);
                ndt_waypoint_close               (               &wpt);
            }
            ndt_list_close(&ndb->waypoints);
        }

        free(ndb);

        *_ndb = NULL;
    }
}

ndt_airport* ndt_navdata_get_airport(ndt_navdatabase *ndb, const char *idt)
{
    if (idt)
    {
        for (size_t i = 0; i < ndt_list_count(ndb->airports); i++)
        {
            ndt_airport *apt = ndt_list_item (ndb->airports,  i);
            if (apt)
            {
                int cmp  = strncasecmp(idt, apt->info.idnt, sizeof(apt->info.idnt));
                if (cmp <= 0)
                {
                    if (!cmp)
                    {
                        return apt;
                    }
                    break;
                }
            }
        }
    }

    return NULL;
}

ndt_airway* ndt_navdata_get_airway(ndt_navdatabase *ndb, const char *idt, size_t *idx)
{
    if (idt)
    {
        for (size_t i = idx ? *idx : 0; i < ndt_list_count(ndb->airways); i++)
        {
            ndt_airway *awy = ndt_list_item(ndb->airways, i);
            if (awy)
            {
                int cmp  = strncasecmp(idt, awy->info.idnt, sizeof(awy->info.idnt));
                if (cmp <= 0)
                {
                    if (!cmp)
                    {
                        if (idx) *idx = i;
                        return awy;
                    }
                    break;
                }
            }
        }
    }

    return NULL;
}

ndt_waypoint* ndt_navdata_get_waypoint(ndt_navdatabase *ndb, const char *idt, size_t *idx)
{
    if (idt)
    {
        for (size_t i = idx ? *idx : 0; i < ndt_list_count(ndb->waypoints); i++)
        {
            ndt_waypoint *wpt = ndt_list_item(ndb->waypoints, i);
            if (wpt)
            {
                int cmp  = strncasecmp(idt, wpt->info.idnt, sizeof(wpt->info.idnt));
                if (cmp <= 0)
                {
                    if (!cmp)
                    {
                        if (idx) *idx = i;
                        return wpt;
                    }
                    break;
                }
            }
        }
    }

    return NULL;
}

ndt_waypoint* ndt_navdata_get_wptnear2(ndt_navdatabase *ndb, const char *idt, size_t *idx, ndt_position pos)
{
    if (idt)
    {
        ndt_waypoint *wpt = NULL, *next;
        int64_t       min = INT64_MAX;

        for (size_t i = idx ? *idx : 0; (next = ndt_navdata_get_waypoint(ndb, idt, &i)); i++)
        {
            int64_t dist = ndt_distance_get(ndt_position_calcdistance(pos, next->position), NDT_ALTUNIT_NA);

            if (dist < min)
            {
                if (idx)
                {
                    *idx = i;
                }
                min = dist;
                wpt = next;
            }
        }

        return wpt;
    }

    return NULL;
}

ndt_waypoint* ndt_navdata_get_wpt4pos(ndt_navdatabase *ndb, const char *idt, size_t *idx, ndt_position pos)
{
    if (idt)
    {
        ndt_waypoint *wpt;

        for (size_t i = idx ? *idx : 0; (wpt = ndt_navdata_get_waypoint(ndb, idt, &i)); i++)
        {
            if (!ndt_distance_get(ndt_position_calcdistance(wpt->position, pos), NDT_ALTUNIT_NA))
            {
                if (idx) *idx = i;
                return wpt;
            }
        }
    }

    return NULL;
}

ndt_waypoint* ndt_navdata_get_wpt4aws(ndt_navdatabase *ndb, ndt_waypoint *src, const char *awy2id, const char *awyidt, ndt_airway **_awy, ndt_airway_leg **_in, ndt_airway_leg **_out)
{
    ndt_airway_leg *out,  *in, *last_in = NULL;
    ndt_airway     *awy1, *awy2;
    ndt_waypoint   *dst;

    if (!ndb || !awy2id || !awyidt)
    {
        goto end;
    }

    for (size_t awy1idx = 0; (awy1 = ndt_navdata_get_airway(ndb, awyidt, &awy1idx)); awy1idx++)
    {
        if ((in = ndt_airway_startpoint(awy1, src->info.idnt, src->position)))
        {
            for (size_t awy2idx = 0; (awy2 = ndt_navdata_get_airway(ndb, awy2id, &awy2idx)); awy2idx++)
            {
                if ((out = ndt_airway_intersect(in, awy2)))
                {
                    if ((dst = ndt_navdata_get_wpt4pos(ndb, out->out.info.idnt, NULL, out->out.position)))
                    {
                        if (_awy) *_awy = awy1;
                        if (_in)  *_in  =   in;
                        if (_out) *_out =  out;
                        return dst;
                    }
                }
            }
            last_in = in;
        }
    }

    if (last_in)
    {
        fprintf(stderr,
                "ndt_navdata_get_wpt4aws: airways '%s', '%s' have no intersection\n",
                awyidt, awy2id);
    }
    else
    {
        fprintf(stderr,
                "ndt_navdata_get_wpt4aws: invalid startpoint '%s' for airway '%s'\n",
                src->info.idnt, awyidt);
    }

end:
    return NULL;
}

ndt_waypoint* ndt_navdata_get_wpt4awy(ndt_navdatabase *ndb, ndt_waypoint *src, const char *dstidt, const char *awyidt, ndt_airway **_awy, ndt_airway_leg **_in, ndt_airway_leg **_out)
{
    ndt_airway_leg *out, *in, *last_in = NULL;
    ndt_airway     *awy;
    ndt_waypoint   *dst;

    if (!ndb || !src || !dstidt || !awyidt)
    {
        goto end;
    }

    for (size_t awyidx = 0; (awy = ndt_navdata_get_airway(ndb, awyidt, &awyidx)); awyidx++)
    {
        if ((in = ndt_airway_startpoint(awy, src->info.idnt, src->position)))
        {
            for (size_t dstidx = 0; (dst = ndt_navdata_get_waypoint(ndb, dstidt, &dstidx)); dstidx++)
            {
                if ((out = ndt_airway_endpoint(in, dst->info.idnt, dst->position)))
                {
                    if (_awy) *_awy = awy;
                    if (_in)  *_in  =  in;
                    if (_out) *_out = out;
                    return dst;
                }
            }
            last_in = in;
        }
    }

    if (last_in)
    {
        fprintf(stderr,
                "ndt_navdata_get_wpt4awy: invalid endpoint '%s' for airway '%s'\n",
                dstidt, awyidt);
    }
    else
    {
        fprintf(stderr,
                "ndt_navdata_get_wpt4awy: invalid startpoint '%s' for airway '%s'\n",
                src->info.idnt, awyidt);
    }

end:
    return NULL;
}

static int compare_apt(const void *p1, const void *p2)
{
    ndt_airport *apt1 = *(ndt_airport**)p1;
    ndt_airport *apt2 = *(ndt_airport**)p2;

    // there shouldn't be any duplicates, but use pointers for determinism
    int cmp = strncasecmp(apt1->info.idnt, apt2->info.idnt, sizeof(apt2->info.idnt));
    return cmp ? cmp : (apt1 < apt2 ? -1 : 1);
}

static int compare_awy(const void *p1, const void *p2)
{
    ndt_airway *awy1 = *(ndt_airway**)p1;
    ndt_airway *awy2 = *(ndt_airway**)p2;

    // duplicates handled by the getter, but use pointers for determinism
    int cmp = strncasecmp(awy1->info.idnt, awy2->info.idnt, sizeof(awy2->info.idnt));
    return cmp ? cmp : (awy1 < awy2 ? -1 : 1);
}

static int compare_wpt(const void *p1, const void *p2)
{
    ndt_waypoint *wpt1 = *(ndt_waypoint**)p1;
    ndt_waypoint *wpt2 = *(ndt_waypoint**)p2;

    // duplicates handled by the getter, but use pointers for determinism
    int cmp = strncasecmp(wpt1->info.idnt, wpt2->info.idnt, sizeof(wpt2->info.idnt));
    return cmp ? cmp : (wpt1 < wpt2 ? -1 : 1);
}
