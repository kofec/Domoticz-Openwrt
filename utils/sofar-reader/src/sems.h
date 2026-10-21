#ifndef SEMS_H
#define SEMS_H

#include <stddef.h>

#include "goodwe.h"
#include "pvoutput.h"

struct json_object;

/*
 * Client of the undocumented SEMS+ web API (eu-semsplus.goodwe.com), the
 * successor of the old SEMS API that went dark on 2026-09-23 for migrated
 * accounts. Taken from the site's code and the Home Assistant integration
 * goodwe-sems-home-assistant 11.0.0.
 */
struct sems {
	char url[96];
	char account[96];
	char pwd[64];		/* base64(md5hex(password)), as the login form */
	char station[64];
	char api[192];		/* gateway from the login reply */
	char sn[48];		/* inverter S/N, looked up once */
	int freshness;		/* older data is an echo, not a reading [s] */
	int timeout;
	struct json_object *token;
	void *share;		/* CURLSH: cookies across requests */
};

int sems_init(struct sems *s, const char *url, const char *account,
	      const char *password, const char *station, int freshness);
void sems_free(struct sems *s);

/*
 * Current values. Returns 0 for fresh data, 1 when the newest data is
 * older than freshness (err says from when), -1 on errors (err set).
 */
int sems_read(struct sems *s, struct gw_reading *r, char *err, size_t errlen);

/*
 * 5-minute points of a past day "YYYY-MM-DD" for PVOutput: power, voltage
 * and the energy integrated from the power, scaled to the day total, as
 * "gw2pvo --date" did. *pts is malloc()ed. Returns 0 or -1 (err set).
 */
int sems_history(struct sems *s, const char *day, struct pvo_point **pts, int *n,
		 double *total_kwh, char *err, size_t errlen);

#endif
