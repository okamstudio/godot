/* $Id: igd_desc_parse.h,v 1.12 2014/11/17 17:19:13 nanard Exp $ */
/* Project : miniupnp
 * http://miniupnp.free.fr/
 * Author : Thomas Bernard
 * Copyright (c) 2005-2014 Thomas Bernard
 * This software is subject to the conditions detailed in the
 * LICENCE file provided in this distribution.
 * */
#ifndef IGD_DESC_PARSE_H_INCLUDED
#define IGD_DESC_PARSE_H_INCLUDED

/* Structure to store the result of the parsing of UPnP
 * descriptions of Internet Gateway Devices */
#define MINIUPNPC_URL_MAXSIZE (128)
struct IGDdatas_service {
	char controlurl[MINIUPNPC_URL_MAXSIZE];
	char eventsuburl[MINIUPNPC_URL_MAXSIZE];
	char scpdurl[MINIUPNPC_URL_MAXSIZE];
	char servicetype[MINIUPNPC_URL_MAXSIZE];
	/*char devicetype[MINIUPNPC_URL_MAXSIZE];*/
};

struct IGDdatas {
	char cureltname[MINIUPNPC_URL_MAXSIZE];
	char urlbase[MINIUPNPC_URL_MAXSIZE];
	char presentationurl[MINIUPNPC_URL_MAXSIZE];
	int level;
	/*int state;*/
	/* "urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1" */
	struct IGDdatas_service CIF;
	/* "urn:schemas-upnp-org:service:WANIPConnection:1"
	 * "urn:schemas-upnp-org:service:WANPPPConnection:1" */
	struct IGDdatas_service first;
	/* if both WANIPConnection and WANPPPConnection are present */
	struct IGDdatas_service second;
	/* "urn:schemas-upnp-org:service:WANIPv6FirewallControl:1" */
	struct IGDdatas_service IPv6FC;
	/* tmp */
	struct IGDdatas_service tmp;
};

void IGDstartelt(void *, const char *, int);
void IGDendelt(void *, const char *, int);
void IGDdata(void *, const char *, int);
#ifdef DEBUG
void printIGD(struct IGDdatas *);
#endif /* DEBUG */

#endif /* IGD_DESC_PARSE_H_INCLUDED */
