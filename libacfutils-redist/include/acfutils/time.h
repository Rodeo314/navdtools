/*
 * CDDL HEADER START
 *
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 *
 * CDDL HEADER END
*/
/*
 * Copyright 2017 Saso Kiselkov. All rights reserved.
 */

#ifndef	_ACF_UTILS_TIME_H_
#define	_ACF_UTILS_TIME_H_

#include <stdint.h>
#include <acfutils/core.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	USEC2SEC(usec)	((usec) / 1000000.0)
#define	SEC2USEC(sec)	((sec) * 1000000ll)
#define	microclock	ACFSYM(microclock)
API_EXPORT uint64_t microclock();

#ifdef	__cplusplus
}
#endif

#endif	/* _ACF_UTILS_TIME_H_ */
