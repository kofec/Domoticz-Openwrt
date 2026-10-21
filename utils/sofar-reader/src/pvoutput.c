/*
 * PVOutput upload (addstatus.jsp) over HTTPS with libcurl. OpenWrt routers
 * running Domoticz already have libcurl and OpenSSL loaded, so this costs
 * no flash and little RAM. The easy handle is freed after every request:
 * one upload per 5 minutes does not need a kept-alive connection.
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <curl/curl.h>

#include "pvoutput.h"

#define PVO_URL "https://pvoutput.org/service/r2/addstatus.jsp"

struct reply {
	char data[128];
	size_t len;
};

static size_t on_data(char *ptr, size_t size, size_t n, void *ud)
{
	struct reply *r = ud;
	size_t len = size * n, room = sizeof(r->data) - 1 - r->len;

	if (len < room)
		room = len;
	memcpy(r->data + r->len, ptr, room);
	r->len += room;
	r->data[r->len] = 0;
	return len;
}

static char *trim(char *s)
{
	char *e;

	while (isspace((unsigned char)*s))
		s++;
	e = s + strlen(s);
	while (e > s && isspace((unsigned char)e[-1]))
		*--e = 0;
	/* secrets.conf values may be quoted */
	if (e - s >= 2 && (*s == '"' || *s == '\'') && e[-1] == *s) {
		e[-1] = 0;
		s++;
	}
	return s;
}

int pvoutput_key_from_file(struct pvoutput *p, const char *file, const char *name)
{
	char line[256], *s, *eq;
	size_t nlen = name ? strlen(name) : 0;
	FILE *f = fopen(file, "r");
	int ret = -1;

	if (!f)
		return -1;
	while (fgets(line, sizeof(line), f)) {
		s = trim(line);
		if (!*s || *s == '#')
			continue;
		if (nlen) {
			if (strncmp(s, name, nlen))
				continue;
			eq = trim(s + nlen);
			if (*eq != '=')
				continue;
			s = trim(eq + 1);
		}
		if (*s && strlen(s) < sizeof(p->api_key)) {
			strcpy(p->api_key, s);
			ret = 0;
		}
		break;
	}
	fclose(f);
	return ret;
}

int pvoutput_add_status(const struct pvoutput *p, time_t t, long power_w,
			long energy_wh, const char *voltage, int dry)
{
	static int curl_ready;
	char body[160], key[96], sys[48], err[CURL_ERROR_SIZE];
	struct curl_slist *hdrs = NULL;
	struct reply r;
	struct tm lt;
	long code = 0;
	size_t o;
	CURLcode rc = CURLE_FAILED_INIT;
	CURL *c;

	localtime_r(&t, &lt);
	o = strftime(body, sizeof(body), "d=%Y%m%d&t=%H:%M", &lt);
	o += snprintf(body + o, sizeof(body) - o, "&v2=%ld", power_w);
	if (energy_wh >= 0 && o < sizeof(body))
		o += snprintf(body + o, sizeof(body) - o, "&v1=%ld", energy_wh);
	if (voltage && o < sizeof(body))
		snprintf(body + o, sizeof(body) - o, "&v6=%s", voltage);

	if (dry) {
		syslog(LOG_INFO, "pvoutput (not sent): %s", body);
		return 0;
	}

	if (!curl_ready) {
		if (curl_global_init(CURL_GLOBAL_DEFAULT))
			return -1;
		curl_ready = 1;
	}

	snprintf(key, sizeof(key), "X-Pvoutput-Apikey: %s", p->api_key);
	snprintf(sys, sizeof(sys), "X-Pvoutput-SystemId: %s", p->system_id);
	hdrs = curl_slist_append(hdrs, key);
	hdrs = curl_slist_append(hdrs, sys);

	/* A TLS handshake on a busy router can time out, try once more */
	for (int tries = 0; tries < 2; tries++) {
		if (tries)
			sleep(5);
		c = curl_easy_init();
		if (!c)
			break;
		memset(&r, 0, sizeof(r));
		err[0] = 0;
		curl_easy_setopt(c, CURLOPT_URL, PVO_URL);
		curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
		curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
		curl_easy_setopt(c, CURLOPT_USERAGENT, "sofar-reader");
		curl_easy_setopt(c, CURLOPT_TIMEOUT, (long)p->timeout);
		curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(c, CURLOPT_ERRORBUFFER, err);
		curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_data);
		curl_easy_setopt(c, CURLOPT_WRITEDATA, &r);
		rc = curl_easy_perform(c);
		curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
		curl_easy_cleanup(c);
		if (rc == CURLE_OK)
			break;
		syslog(LOG_DEBUG, "pvoutput: %s", *err ? err : curl_easy_strerror(rc));
	}
	curl_slist_free_all(hdrs);

	if (rc != CURLE_OK) {
		syslog(LOG_WARNING, "pvoutput: %s", *err ? err : curl_easy_strerror(rc));
		return -1;
	}
	/* Errors come back as text, e.g. "Bad request 400: Invalid power" */
	if (code != 200) {
		syslog(LOG_WARNING, "pvoutput: HTTP %ld %.80s", code, trim(r.data));
		return -1;
	}
	syslog(LOG_DEBUG, "pvoutput <- %s", body);
	return 0;
}
