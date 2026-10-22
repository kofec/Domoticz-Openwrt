/*
 * PVOutput API: addstatus.jsp for the live value, getstatus.jsp and
 * addbatchstatus.jsp to fill gaps of a past day. HTTPS goes through http.c.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "http.h"
#include "pvoutput.h"

#define PVO_URL		"https://pvoutput.org/service/r2/"
#define PVO_BATCH	30

static void auth_headers(const struct pvoutput *p, char *key, size_t klen,
			 char *sys, size_t slen, const char *hdrs[3])
{
	snprintf(key, klen, "X-Pvoutput-Apikey: %s", p->api_key);
	snprintf(sys, slen, "X-Pvoutput-SystemId: %s", p->system_id);
	hdrs[0] = key;
	hdrs[1] = sys;
	hdrs[2] = NULL;
}

static void date_time(time_t t, char d[12], char hm[8])
{
	struct tm lt;

	localtime_r(&t, &lt);
	strftime(d, 12, "%Y%m%d", &lt);
	strftime(hm, 8, "%H:%M", &lt);
}

/* Errors come back as text, e.g. "Bad request 400: Invalid power" */
static void log_failure(const char *what, int rc, const struct http_reply *r)
{
	if (rc)
		syslog(LOG_WARNING, "pvoutput %s: %s", what, r->err);
	else
		syslog(LOG_WARNING, "pvoutput %s: HTTP %ld %.80s", what, r->code,
		       r->data ? r->data : "");
}

int pvoutput_add_status(const struct pvoutput *p, const struct pvo_point *pt, int dry)
{
	char body[160], key[96], sys[48], d[12], hm[8];
	const char *hdrs[3];
	struct http_reply r;
	size_t o;
	int rc;

	date_time(pt->t, d, hm);
	o = snprintf(body, sizeof(body), "d=%s&t=%s&v2=%ld", d, hm, pt->power_w);
	if (pt->energy_wh >= 0)
		o += snprintf(body + o, sizeof(body) - o, "&v1=%ld", pt->energy_wh);
	if (pt->voltage_dv > 0 && o < sizeof(body))
		snprintf(body + o, sizeof(body) - o, "&v6=%ld.%ld",
			 pt->voltage_dv / 10, pt->voltage_dv % 10);

	if (dry) {
		syslog(LOG_INFO, "pvoutput (not sent): %s", body);
		return 0;
	}
	auth_headers(p, key, sizeof(key), sys, sizeof(sys), hdrs);
	rc = http_request(PVO_URL "addstatus.jsp", body, hdrs, p->timeout, NULL, &r);
	if (rc || r.code != 200) {
		log_failure("addstatus", rc, &r);
		http_reply_free(&r);
		return -1;
	}
	http_reply_free(&r);
	syslog(LOG_DEBUG, "pvoutput <- %s", body);
	return 0;
}

int pvoutput_add_batch(const struct pvoutput *p, const struct pvo_point *pts, int n)
{
	char body[PVO_BATCH * 80 + 8], key[96], sys[48], d[12], hm[8], *rec, *save;
	const char *hdrs[3];
	struct http_reply r;
	int accepted = 0, rc;

	auth_headers(p, key, sizeof(key), sys, sizeof(sys), hdrs);
	for (int start = 0; start < n; start += PVO_BATCH) {
		/* data=d,t,v1,v2,v3,v4,v5,v6;... with "," and ";" URL encoded */
		size_t o = snprintf(body, sizeof(body), "data=");

		for (int i = start; i < n && i < start + PVO_BATCH; i++) {
			const struct pvo_point *pt = &pts[i];

			date_time(pt->t, d, hm);
			o += snprintf(body + o, sizeof(body) - o, "%s%s%%2C%s%%2C",
				      i > start ? "%3B" : "", d, hm);
			if (pt->energy_wh >= 0)
				o += snprintf(body + o, sizeof(body) - o, "%ld", pt->energy_wh);
			o += snprintf(body + o, sizeof(body) - o, "%%2C%ld%%2C%%2C%%2C%%2C",
				      pt->power_w);
			if (pt->voltage_dv > 0)
				o += snprintf(body + o, sizeof(body) - o, "%ld.%ld",
					      pt->voltage_dv / 10, pt->voltage_dv % 10);
		}
		rc = http_request(PVO_URL "addbatchstatus.jsp", body, hdrs, p->timeout,
				  NULL, &r);
		if (rc || r.code != 200) {
			log_failure("addbatchstatus", rc, &r);
			http_reply_free(&r);
			return -1;
		}
		/* one "d,t,1" (accepted) or "d,t,0" per status */
		for (rec = r.data ? strtok_r(r.data, ";", &save) : NULL; rec;
		     rec = strtok_r(NULL, ";", &save)) {
			size_t l = strlen(rec);

			while (l && isspace((unsigned char)rec[l - 1]))
				l--;
			if (l >= 2 && rec[l - 2] == ',' && rec[l - 1] == '1')
				accepted++;
		}
		http_reply_free(&r);
	}
	return accepted;
}

int pvoutput_get_day(const struct pvoutput *p, const char *ymd, struct pvo_day *day)
{
	char url[160], key[96], sys[48], *rec, *save;
	const char *hdrs[3];
	struct http_reply r;
	int rc;

	memset(day, 0, sizeof(*day));
	snprintf(url, sizeof(url), PVO_URL "getstatus.jsp?d=%s&h=1&limit=288&asc=1", ymd);
	auth_headers(p, key, sizeof(key), sys, sizeof(sys), hdrs);
	rc = http_request(url, NULL, hdrs, p->timeout, NULL, &r);
	/* A day without any status is "Bad request 400: No status found" */
	if (!rc && r.code == 400 && r.data && strstr(r.data, "o status")) {
		http_reply_free(&r);
		return 0;
	}
	if (rc || r.code != 200) {
		log_failure("getstatus", rc, &r);
		http_reply_free(&r);
		return -1;
	}
	/* "d,t,energy,efficiency,power,...;..." */
	for (rec = r.data ? strtok_r(r.data, ";", &save) : NULL; rec;
	     rec = strtok_r(NULL, ";", &save)) {
		char *t = strchr(rec, ','), *e;
		int h, m;

		if (!t || sscanf(t + 1, "%d:%d", &h, &m) != 2 ||
		    h < 0 || h > 23 || m < 0 || m > 59)
			continue;
		e = strchr(t + 1, ',');
		day->have[h * 60 + m] = 1;
		day->energy_wh[h * 60 + m] = e && isdigit((unsigned char)e[1]) ?
			strtol(e + 1, NULL, 10) : -1;
		day->count++;
	}
	http_reply_free(&r);
	return 0;
}
