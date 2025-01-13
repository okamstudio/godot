/* $Id: upnpdev.h,v 1.4 2021/08/21 09:45:01 nanard Exp $ */
/* Project : miniupnp
 * Web : http://miniupnp.free.fr/ or https://miniupnp.tuxfamily.org/
 * Author : Thomas BERNARD
 * copyright (c) 2005-2021 Thomas Bernard
 * This software is subjet to the conditions detailed in the
 * provided LICENSE file. */
#ifndef UPNPDEV_H_INCLUDED
#define UPNPDEV_H_INCLUDED

#include "miniupnpc_declspec.h"

#ifdef __cplusplus
extern "C" {
#endif

struct UPNPDev {
	struct UPNPDev * pNext;
	char * descURL;
	char * st;
	char * usn;
	unsigned int scope_id;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
	/* C99 flexible array member */
	char buffer[];
#elif defined(__GNUC__)
	char buffer[0];
#else
	/* Fallback to a hack */
	char buffer[1];
#endif
};

/* freeUPNPDevlist()
 * free list returned by upnpDiscover() */
MINIUPNP_LIBSPEC void freeUPNPDevlist(struct UPNPDev * devlist);


#ifdef __cplusplus
}
#endif


#endif /* UPNPDEV_H_INCLUDED */
