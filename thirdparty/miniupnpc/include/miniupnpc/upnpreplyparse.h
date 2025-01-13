/* $Id: upnpreplyparse.h,v 1.19 2014/10/27 16:33:19 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2013 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#ifndef UPNPREPLYPARSE_H_INCLUDED
#define UPNPREPLYPARSE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

struct NameValue {
	struct NameValue * l_next;
	char name[64];
	char value[128];
};

struct NameValueParserData {
	struct NameValue * l_head;
	char curelt[64];
	char * portListing;
	int portListingLength;
	int topelt;
	const char * cdata;
	int cdatalen;
};

/* ParseNameValue() */
void
ParseNameValue(const char * buffer, int bufsize,
               struct NameValueParserData * data);

/* ClearNameValueList() */
void
ClearNameValueList(struct NameValueParserData * pdata);

/* GetValueFromNameValueList() */
char *
GetValueFromNameValueList(struct NameValueParserData * pdata,
                          const char * Name);

#if 0
/* GetValueFromNameValueListIgnoreNS() */
char *
GetValueFromNameValueListIgnoreNS(struct NameValueParserData * pdata,
                                  const char * Name);
#endif

/* DisplayNameValueList() */
#ifdef DEBUG
void
DisplayNameValueList(char * buffer, int bufsize);
#endif

#ifdef __cplusplus
}
#endif

#endif

