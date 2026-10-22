/*
 * Minimal HTTP/1.0 client for the Domoticz JSON API. Domoticz normally runs
 * on the same router or in the LAN, so plain HTTP without TLS is enough.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

#include "domoticz.h"
#include "solarman.h"

void domoticz_set_auth(struct domoticz *dz, const char *user, const char *pass)
{
	static const char b64[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	char in[96];
	size_t n, i, o = 0;

	dz->auth[0] = 0;
	if (!user || !*user)
		return;

	n = snprintf(in, sizeof(in), "%s:%s", user, pass ? pass : "");
	if (n >= sizeof(in) || (n + 2) / 3 * 4 >= sizeof(dz->auth))
		return;

	for (i = 0; i < n; i += 3) {
		unsigned v = (unsigned char)in[i] << 16;

		if (i + 1 < n)
			v |= (unsigned char)in[i + 1] << 8;
		if (i + 2 < n)
			v |= (unsigned char)in[i + 2];
		dz->auth[o++] = b64[v >> 18 & 63];
		dz->auth[o++] = b64[v >> 12 & 63];
		dz->auth[o++] = i + 1 < n ? b64[v >> 6 & 63] : '=';
		dz->auth[o++] = i + 2 < n ? b64[v & 63] : '=';
	}
	dz->auth[o] = 0;
}

static void urlencode(char *dst, size_t len, const char *src)
{
	static const char hex[] = "0123456789ABCDEF";
	size_t o = 0;

	for (; *src && o + 4 < len; src++) {
		unsigned char c = *src;

		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		    (c >= '0' && c <= '9') || strchr("-._~;", c)) {
			dst[o++] = c;
		} else {
			dst[o++] = '%';
			dst[o++] = hex[c >> 4];
			dst[o++] = hex[c & 15];
		}
	}
	dst[o] = 0;
}

/*
 * GET /json.htm?<query>, "what" names the request in the log. The whole
 * reply (headers and body) is copied to out when it is given.
 */
static int request(const struct domoticz *dz, const char *query, const char *what,
		   char *out, size_t outlen)
{
	char req[1280], resp[2048];
	size_t got = 0;
	ssize_t n;
	int fd, len;

	len = snprintf(req, sizeof(req),
		       "GET /json.htm?%s HTTP/1.0\r\n"
		       "Host: %s:%d\r\n"
		       "%s%s%s"
		       "Connection: close\r\n\r\n",
		       query, dz->host, dz->port,
		       *dz->auth ? "Authorization: Basic " : "", dz->auth,
		       *dz->auth ? "\r\n" : "");
	if (len < 0 || (size_t)len >= sizeof(req))
		return -1;

	fd = solarman_connect(dz->host, dz->port, dz->timeout);
	if (fd < 0) {
		syslog(LOG_WARNING, "domoticz %s:%d: cannot connect", dz->host, dz->port);
		return -1;
	}
	if (send(fd, req, len, MSG_NOSIGNAL) != len) {
		close(fd);
		return -1;
	}
	while (got < sizeof(resp) - 1) {
		n = recv(fd, resp + got, sizeof(resp) - 1 - got, 0);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			break;
		got += n;
	}
	close(fd);
	resp[got] = 0;

	if (strncmp(resp, "HTTP/1.", 7) || strncmp(resp + 8, " 200", 4)) {
		syslog(LOG_WARNING, "domoticz %s: %.40s", what,
		       got ? resp : "no response");
		return -1;
	}
	if (!strstr(resp, "\"OK\"")) {
		syslog(LOG_WARNING, "domoticz %s: rejected", what);
		return -1;
	}
	if (out)
		snprintf(out, outlen, "%s", resp);
	return 0;
}

/* "07:13" after "key" : in a JSON reply -> minutes since midnight */
static int json_hhmm(const char *json, const char *key)
{
	const char *p = strstr(json, key);
	int h, m;

	if (!p || !(p = strchr(p + strlen(key), ':')) || !(p = strchr(p, '"')))
		return -1;
	if (sscanf(p + 1, "%d:%d", &h, &m) != 2 || h < 0 || h > 23 || m < 0 || m > 59)
		return -1;
	return h * 60 + m;
}

int domoticz_sun(const struct domoticz *dz, int *sunrise, int *sunset)
{
	char resp[2048];

	if (request(dz, "type=command&param=getSunRiseSet", "sunrise/sunset",
		    resp, sizeof(resp)))
		return -1;
	*sunrise = json_hhmm(resp, "\"Sunrise\"");
	*sunset = json_hhmm(resp, "\"Sunset\"");
	return *sunrise < 0 || *sunset < 0 ? -1 : 0;
}

int domoticz_udevice(const struct domoticz *dz, int idx, int nvalue,
		     const char *svalue)
{
	char query[640], val[512], what[24];

	urlencode(val, sizeof(val), svalue);
	snprintf(query, sizeof(query),
		 "type=command&param=udevice&idx=%d&nvalue=%d&svalue=%s",
		 idx, nvalue, val);
	snprintf(what, sizeof(what), "idx %d", idx);
	if (request(dz, query, what, NULL, 0))
		return -1;
	syslog(LOG_DEBUG, "domoticz idx %d <- %s", idx, svalue);
	return 0;
}

int domoticz_notify(const struct domoticz *dz, const char *subject,
		    const char *body)
{
	char query[1024], subj[256], text[512];

	urlencode(subj, sizeof(subj), subject);
	urlencode(text, sizeof(text), body);
	snprintf(query, sizeof(query),
		 "type=command&param=sendnotification&subject=%s&body=%s",
		 subj, text);
	if (request(dz, query, "notification", NULL, 0))
		return -1;
	syslog(LOG_INFO, "notification sent: %s - %s", subject, body);
	return 0;
}
