#ifndef PVOUTPUT_H
#define PVOUTPUT_H

#include <time.h>

struct pvoutput {
	char system_id[16];
	char api_key[64];	/* empty = upload disabled */
	int interval;		/* seconds between uploads */
	int timeout;		/* per request, whole transfer */
};

/*
 * Read the API key from a file kept outside the package config, e.g.
 * /root/lib/secrets.conf. With a name, the line "name = key" is used,
 * without one the first line that is not a comment.
 */
int pvoutput_key_from_file(struct pvoutput *p, const char *file, const char *name);

/*
 * POST addstatus.jsp: v2 power [W], v1 energy today [Wh] (skipped when < 0),
 * v6 voltage [V] (skipped when NULL). Date and time are the router's local
 * time t. With dry set, only log what would be sent.
 */
int pvoutput_add_status(const struct pvoutput *p, time_t t, long power_w,
			long energy_wh, const char *voltage, int dry);

#endif
