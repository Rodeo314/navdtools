/*
 * fmt_aibxt.c
 *
 * This file is part of the navdtools source code.
 *
 * (C) Copyright 2014-2016 Timothy D. Walker and others.
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
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "common/common.h"
#include "common/list.h"

#include "compat/compat.h"

#include "airway.h"
#include "flightplan.h"
#include "fmt_aibxt.h"
#include "waypoint.h"

static int print_directto(FILE *fd, size_t idx, ndt_waypoint *dst);

int ndt_fmt_aibxt_flightplan_set_route(ndt_flightplan *flp, const char *rte)
{
    ndt_waypoint *src = NULL;
    char         *start, *pos;
    double        latitude, longitude;
    char         *line = NULL, buf[23];
    int           linecap, header = 0, err = 0, init = 0;
    char         *dep_apt = NULL, *dep_rwy = NULL;
    char         *arr_apt = NULL, *arr_rwy = NULL;
    char         *sid_idt = NULL, *sid_trn = NULL;
    char         *star_id = NULL, *star_tr = NULL;
    char         *awyidt  = NULL, *dstidt  = NULL, *srcidt = NULL;

    if (!flp || !rte)
    {
        err = ENOMEM;
        goto end;
    }

    start = pos = strdup(rte);
    if (!start)
    {
        err = ENOMEM;
        goto end;
    }

    while ((err = ndt_file_getline(&line, &linecap, &pos)) > 0)
    {
        if (*line == '\r' || *line == '\n')
        {
            continue; // skip blank lines
        }

        if (!header)
        {
            if (strncmp(line, "[CoRte]", 7))
            {
                ndt_log("[fmt_aibxt]: invalid header\n");
                err = EINVAL;
                goto end;
            }
            header = 1;
            continue;
        }
        else if (!strncmp(line, "ArptDep=", 8))
        {
            if (sscanf(line, "ArptDep=%8s", buf) != 1)
            {
                err = EINVAL;
                goto end;
            }
            dep_apt = strdup(buf);
            continue;
        }
        else if (!strncmp(line, "ArptArr=", 8))
        {
            if (sscanf(line, "ArptArr=%8s", buf) != 1)
            {
                err = EINVAL;
                goto end;
            }
            arr_apt = strdup(buf);
            continue;
        }
        else if (!strncmp(line, "RwyDep=", 7))
        {
            // valid if empty
            if (strnlen(line, 8) == 8)
            {
                if (sscanf(line, "RwyDep=%8s", buf) != 1)
                {
                    err = EINVAL;
                    goto end;
                }
                dep_rwy = strdup(buf);
            }
            continue;
        }
        else if (!strncmp(line, "RwyArr=", 7))
        {
            // valid if empty
            if (strnlen(line, 8) == 8)
            {
                if (sscanf(line, "RwyArr=%8s", buf) != 1)
                {
                    err = EINVAL;
                    goto end;
                }
                arr_rwy = strdup(buf);
            }
            continue;
        }
        else if (!strncmp(line, "SID=", 4))
        {
            // valid if empty
            if (strnlen(line, 5) == 5)
            {
                if (sscanf(line, "SID=%8s", buf) != 1)
                {
                    err = EINVAL;
                    goto end;
                }
                sid_idt = strdup(buf);
            }
            continue;
        }
        else if (!strncmp(line, "SID_Trans=", 10))
        {
            // valid if empty
            if (strnlen(line, 11) == 11)
            {
                if (sscanf(line, "SID_Trans=%8s", buf) != 1)
                {
                    err = EINVAL;
                    goto end;
                }
                sid_trn = strdup(buf);
            }
            continue;
        }
        else if (!strncmp(line, "STAR=", 5))
        {
            // valid if empty
            if (strnlen(line, 6) == 6)
            {
                if (sscanf(line, "STAR=%8s", buf) != 1)
                {
                    err = EINVAL;
                    goto end;
                }
                star_id = strdup(buf);
            }
            continue;
        }
        else if (!strncmp(line, "STAR_Trans=", 11))
        {
            // valid if empty
            if (strnlen(line, 12) == 12)
            {
                if (sscanf(line, "STAR_Trans=%8s", buf) != 1)
                {
                    err = EINVAL;
                    goto end;
                }
                star_tr = strdup(buf);
            }
            continue;
        }
        else if (!strncmp(line, "APPR_Trans=",  11) ||
                 !strncmp(line, "RwyArrFINAL=", 12))
        {
            continue; // skip (approach names may not match our DB)
        }
        else if (!strncmp(line, "Airway", 6))
        {
            if (!awyidt)
            {
                if (sscanf(line, "Airway%*d=%8s", buf) != 1)
                {
                    err = EINVAL;
                    goto end;
                }
                awyidt = strdup(buf);
                continue;
            }
            else if (!srcidt)
            {
                if (sscanf(line, "Airway%*dFROM=%8s", buf) != 1)
                {
                    err = EINVAL;
                    goto end;
                }
                srcidt = strdup(buf);
                continue;
            }
            else
            {
                if (sscanf(line, "Airway%*dTO=%8s", buf) != 1)
                {
                    err = EINVAL;
                    goto end;
                }
                dstidt = strdup(buf);
            }
        }
        else if (!strncmp(line, "DctWpt", 6))
        {
            if (!dstidt)
            {
                if (sscanf(line, "DctWpt%*d=%8s", buf) != 1)
                {
                    err = EINVAL;
                    goto end;
                }
                dstidt = strdup(buf);
                continue;
            }
            else if (sscanf(line, "DctWpt%*dCoordinates=%lf,%lf", &latitude, &longitude) != 2)
            {
                err = EINVAL;
                goto end;
            }
        }

        /* Departure */
        if (!init)
        {
            if (flp->dep.apt)
            {
                if (dep_apt && strcmp(dep_apt, flp->dep.apt->info.idnt))
                {
                    ndt_log("[fmt_aibxt]: departure airport mismatch ('%s', '%s')\n",
                            dep_apt, flp->dep.apt->info.idnt);
                    err = EINVAL;
                    goto end;
                }
            }
            if (flp->dep.rwy)
            {
                if (dep_rwy && strcmp(dep_rwy, flp->dep.rwy->info.idnt))
                {
                    ndt_log("[fmt_aibxt]: departure runway mismatch ('%s', '%s')\n",
                            dep_rwy, flp->dep.rwy->info.idnt);
                    err = EINVAL;
                    goto end;
                }
            }
            if ((err = ndt_flightplan_set_departure(flp, dep_apt,
                                                    dep_rwy ? dep_rwy + strnlen(dep_apt, strlen(dep_rwy)) :
                                                    flp->dep.rwy ? flp->dep.rwy->info.idnt : NULL)))
            {
                goto end;
            }

            /* SID must be set before the flight route for proper sequencing */
            if (sid_idt)
            {
                if (flp->dep.sid.proc)
                {
                    if (sid_trn)
                    {
                        ndt_log("[fmt_aibxt]: warning: ignoring SID '%s.%s'\n", sid_idt, sid_trn);
                    }
                    else
                    {
                        ndt_log("[fmt_aibxt]: warning: ignoring SID '%s'\n", sid_idt);
                    }
                }
                else if ((err = ndt_flightplan_set_departsid(flp, sid_idt, sid_trn)))
                {
                    goto end;
                }
            }

            /* Set the initial source waypoint (SID, runway or airport). */
            src = (flp->dep.sid.enroute.rsgt ? flp->dep.sid.enroute.rsgt->dst :
                   flp->dep.sid.        rsgt ? flp->dep.sid.        rsgt->dst :
                   flp->dep.rwy              ? flp->dep.rwy->waypoint         : flp->dep.apt->waypoint);

            init = 1;//done
        }

        if (!dstidt)
        {
            ndt_log("[fmt_aibxt]: next waypoint not set\n");
            err = EINVAL;
            goto end;
        }

        ndt_route_segment *rsg = NULL;

        if (!awyidt) // direct to
        {
            ndt_waypoint *dst = ndt_navdata_get_wptnear2(flp->ndb, dstidt, NULL,
                                                         ndt_position_init(latitude,
                                                                           longitude,
                                                                           ndt_distance_init(0, NDT_ALTUNIT_NA)));
            if (!dst || ndt_distance_get(ndt_position_calcdistance(dst->position,
                                                                   ndt_position_init(latitude,
                                                                                     longitude,
                                                                                     ndt_distance_init(0, NDT_ALTUNIT_NA))), NDT_ALTUNIT_NM) > 1)
            {
                // not found or too far, use coordinates
                if (round(fabs(latitude))  >  90. ||
                    round(fabs(longitude)) > 180.)
                {
                    ndt_log("[fmt_aibxt]: invalid coordinates (%lf, %lf)\n",
                            latitude, longitude);
                    ndt_waypoint_close(&dst);
                    err = EINVAL;
                    goto end;
                }
                else
                {
                    // we're done with the identifier, we can use buf
                    snprintf(buf, sizeof(buf), "%+010.6lf/%+011.6lf",
                             latitude, longitude);
                }
                if (!(dst = ndt_waypoint_llc(buf)))
                {
                    err = ENOMEM;
                    goto end;
                }
                ndt_list_add(flp->cws, dst);
            }
            rsg = ndt_route_segment_direct(src, dst);
        }
        else if (!strcmp(awyidt, "DCT") || (!strncmp(awyidt, "NAT", 3) && strlen(awyidt) == 4)) // direct to coded as airway
        {
            /*
             * Note: since we can't decode North Atlantic Tracks,
             * treat them as a direct to rather than bailing out.
             *
             * Out source waypoint must match srcidt; it may or may not be src
             * (in which case we simply insert a route discontinuity below).
             */
            ndt_position   posn = src ? src->position : flp->dep.apt->coordinates;
            ndt_waypoint  *wpt1 = ndt_navdata_get_wptnear2(flp->ndb, srcidt, NULL, posn);
            if (!wpt1 && !(wpt1 = ndt_waypoint_llc(srcidt)))
            {
                ndt_log("[fmt_aibxt]: invalid waypoint '%s'\n", srcidt);
                err = EINVAL;
                goto end;
            }

            ndt_waypoint  *wpt2 = ndt_navdata_get_wptnear2(flp->ndb, dstidt, NULL, wpt1->position);
            if (!wpt2 && !(wpt2 = ndt_waypoint_llc(dstidt)))
            {
                ndt_log("[fmt_aibxt]: invalid waypoint '%s'\n", dstidt);
                err = EINVAL;
                goto end;
            }
            rsg = ndt_route_segment_direct(wpt1, wpt2);
        }
        else // airway
        {
            ndt_waypoint   *wpt;
            ndt_airway     *awy1 = NULL;
            ndt_airway_leg *out1 = NULL, *in1  = NULL;
            ndt_waypoint   *src1 = NULL, *dst1 = NULL;

            /*
             * Check all valid segments matching "srcdit awyidt dstidt". If
             * possible, pick the first one where the source waypoint matches
             * that of the previous segment, else pick the first valid segment.
             */
            for (size_t wptidx = 0; (wpt = ndt_navdata_get_waypoint(flp->ndb, srcidt, &wptidx)); wptidx++)
            {
                ndt_airway *awy;
                ndt_waypoint *dst;
                ndt_airway_leg *out, *in;

                if ((dst = ndt_navdata_get_wpt4awy(flp->ndb, wpt, dstidt, awyidt, &awy, &in, &out)))
                {
                    if (!src1)
                    {
                        src1 = wpt;
                        dst1 = dst;
                        awy1 = awy;
                        out1 = out;
                        in1  = in;
                    }
                    if (src == wpt)
                    {
                        src1 = wpt;
                        dst1 = dst;
                        awy1 = awy;
                        out1 = out;
                        in1  = in;
                        break;
                    }
                }
            }
            if (!src1 || !dst1 || !awy1 || !in1 || !out1)
            {
                err = EINVAL;
                goto end;
            }
            rsg = ndt_route_segment_airway(src1, dst1, awy1, in1, out1, flp->ndb);
        }

        if (!rsg)
        {
            err = ENOMEM;
            goto end;
        }

        /* Check for discontinuities */
        if (src && src != rsg->src)
        {
            ndt_route_segment *dsc = ndt_route_segment_discon();
            ndt_route_segment *dct = ndt_route_segment_direct(NULL, rsg->src);
            if (!dsc || !dct)
            {
                err = ENOMEM;
                goto end;
            }
            ndt_list_add(flp->rte, dsc);
            ndt_list_add(flp->rte, dct);
        }

        /* We have a leg, our last endpoint becomes our new startpoint */
        src = rsg->dst;

        /* Let's not forget to add our new segment to the route */
        ndt_list_add(flp->rte, rsg);

        /* Reset state for next route segment */
        free(awyidt);
        free(srcidt);
        free(dstidt);
        awyidt = srcidt = dstidt = NULL;
    }

    if (err < 0)
    {
        err = EIO;
        goto end;
    }

    /* Arrival, must be set last for proper sequencing */
    if (flp->arr.apt)
    {
        if (arr_apt && strcmp(arr_apt, flp->arr.apt->info.idnt))
        {
            ndt_log("[fmt_aibxt]: arrival airport mismatch ('%s', '%s')\n",
                    arr_apt, flp->arr.apt->info.idnt);
            err = EINVAL;
            goto end;
        }
    }
    if (flp->arr.rwy)
    {
        if (arr_rwy && strcmp(dep_rwy, flp->dep.rwy->info.idnt))
        {
            ndt_log("[fmt_aibxt]: arrival runway mismatch ('%s', '%s')\n",
                    arr_rwy, flp->arr.rwy->info.idnt);
            err = EINVAL;
            goto end;
        }
    }
    if ((err = ndt_flightplan_set_arrival(flp, arr_apt,
                                          arr_rwy ? arr_rwy + strnlen(arr_apt, strlen(arr_rwy)) :
                                          flp->arr.rwy ? flp->arr.rwy->info.idnt : NULL)))
    {
        goto end;
    }
    if (star_id)
    {
        if (flp->arr.star.proc)
        {
            if (star_tr)
            {
                ndt_log("[fmt_aibxt]: warning: ignoring STAR '%s.%s'\n", star_tr, star_id);
            }
            else
            {
                ndt_log("[fmt_aibxt]: warning: ignoring STAR '%s'\n", star_id);
            }
        }
        else if ((err = ndt_flightplan_set_arrivstar(flp, star_id, star_tr)))
        {
            goto end;
        }
    }

end:
    if (err == EINVAL)
    {
        ndt_log("[fmt_aibxt]: failed to parse \"");
        for (int i = 0; line[i] != '\r' && line[i] != '\n'; i++)
        {
            ndt_log("%c", line[i]);
        }
        ndt_log("\"\n");
    }
    free(dep_apt);
    free(dep_rwy);
    free(sid_idt);
    free(sid_trn);
    free(star_tr);
    free(star_id);
    free(arr_rwy);
    free(arr_apt);
    free(awyidt);
    free(dstidt);
    free(srcidt);
    free(start);
    free(line);
    return err;
}

int ndt_fmt_aibxt_flightplan_write(ndt_flightplan *flp, FILE *fd)
{
    int err = 0;

    if (!flp || !fd)
    {
        err = ENOMEM;
        goto end;
    }

    if (!flp->dep.apt || !flp->arr.apt)
    {
        ndt_log("[fmt_aibxt]: departure or arrival airport not set\n");
        err = EINVAL;
        goto end;
    }

    // header
    err = ndt_fprintf(fd, "[CoRte]\n");
    if (err)
    {
        goto end;
    }

    // departure & arrival airports
    err = ndt_fprintf(fd, "ArptDep=%s\n", flp->dep.apt->info.idnt);
    if (err)
    {
        goto end;
    }
    err = ndt_fprintf(fd, "ArptArr=%s\n", flp->arr.apt->info.idnt);
    if (err)
    {
        goto end;
    }

    // departure & arrival runways (optional)
    if (flp->dep.rwy)
    {
        err = ndt_fprintf(fd, "RwyDep=%s%s\n", flp->dep.apt->info.idnt, flp->dep.rwy->info.idnt);
        if (err)
        {
            goto end;
        }
    }
    if (flp->arr.rwy)
    {
        err = ndt_fprintf(fd, "RwyArr=%s%s\n", flp->arr.apt->info.idnt, flp->arr.rwy->info.idnt);
        if (err)
        {
            goto end;
        }
    }

    // SID/STAR and transitions (optional)
    if (flp->dep.sid.proc)
    {
        err = ndt_fprintf(fd, "SID=%s\n", flp->dep.sid.proc->info.idnt);
        if (err)
        {
            goto end;
        }
        if (flp->dep.sid.enroute.proc)
        {
            err = ndt_fprintf(fd, "SID_Trans=%s\n", flp->dep.sid.enroute.proc->info.misc);
            if (err)
            {
                goto end;
            }
        }
    }
    if (flp->arr.star.proc)
    {
        err = ndt_fprintf(fd, "STAR=%s\n", flp->arr.star.proc->info.idnt);
        if (err)
        {
            goto end;
        }
        if (flp->arr.star.enroute.proc)
        {
            err = ndt_fprintf(fd, "STAR_Trans=%s\n", flp->arr.star.enroute.proc->info.misc);
            if (err)
            {
                goto end;
            }
        }
    }

    // route segments (directs and airways only)
    for (size_t i = 0, j = 1; i < ndt_list_count(flp->rte); i++)
    {
        ndt_route_segment *rsg = ndt_list_item(flp->rte, i);
        if (!rsg)
        {
            err = ENOMEM;
            goto end;
        }

        switch (rsg->type)
        {
            case NDT_RSTYPE_AWY:
                err = ndt_fprintf(fd,
                                  "Airway%zu=%s\n"
                                  "Airway%zuFROM=%s\n"
                                  "Airway%zuTO=%s\n",
                                  j, rsg->awy.awy->info.idnt,
                                  j, rsg->src->info.idnt,
                                  j, rsg->dst->info.idnt);
                break;

            case NDT_RSTYPE_DCT:
                err = print_directto(fd, j, rsg->dst);
                break;

            case NDT_RSTYPE_DSC: // skip discontinuities
                continue;

            default:
                ndt_log("[fmt_aibxt]: unknown segment type '%d'\n", rsg->type);
                err = EINVAL;
                break;
        }
        if (err)
        {
            goto end;
        }
        j++;
    }

end:
    return err;
}

static int print_directto(FILE *fd, size_t idx, ndt_waypoint *dst)
{
    double latlf = ndt_position_getlatitude (dst->position, NDT_ANGUNIT_DEG);
    double lonlf = ndt_position_getlongitude(dst->position, NDT_ANGUNIT_DEG);
    int    latrd = round(latlf);
    int    lonrd = round(lonlf);

    switch (dst->type)
    {
        // use the identifier
        case NDT_WPTYPE_APT:
        case NDT_WPTYPE_DME:
        case NDT_WPTYPE_FIX:
        case NDT_WPTYPE_NDB:
        case NDT_WPTYPE_VOR:
            return ndt_fprintf(fd,
                               "DctWpt%zu=%s\n"
                               "DctWpt%zuCoordinates=%.6lf,%.6lf\n",
                               idx, dst->info.idnt, idx, latlf, lonlf);

        default:
            break;
    }

    // use the coordinates
    if (lonrd / 100)
    {
        return ndt_fprintf(fd,
                           "DctWpt%zu=%02d%c%02d\n"
                           "DctWpt%zuCoordinates=%.6lf,%.6lf\n",
                           idx,
                           (latrd < 0) ? -latrd : latrd,
                           (latrd < 0  && lonrd < 0) ? 'W' : (latrd < 0) ? 'S' : (lonrd < 0) ? 'N' : 'E',
                           (lonrd < 0) ? -lonrd % 100 : lonrd % 100,
                           idx, latlf, lonlf);
    }
    else
    {
        return ndt_fprintf(fd,
                           "DctWpt%zu=%02d%02d%c\n"
                           "DctWpt%zuCoordinates=%.6lf,%.6lf\n",
                           idx,
                           (latrd < 0) ? -latrd : latrd,
                           (lonrd < 0) ? -lonrd : lonrd,
                           (latrd < 0  && lonrd < 0) ? 'W' : (latrd < 0) ? 'S' : (lonrd < 0) ? 'N' : 'E',
                           idx, latlf, lonlf);
    }
}
