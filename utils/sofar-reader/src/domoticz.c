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

int domoticz_udevice(const struct domoticz *dz, int idx, int nvalue,
		     const char *svalue)
{
	char req[1024], val[512], resp[1024];
	size_t got = 0;
	ssize_t n;
	int fd, len;

	urlencode(val, sizeof(val), svalue);
	len = snprintf(req, sizeof(req),
		       "GET /json.htm?type=command&param=udevice&idx=%d&nvalue=%d&svalue=%s HTTP/1.0\r\n"
		       "Host: %s:%d\r\n"
		       "%s%s%s"
		       "Connection: close\r\n\r\n",
		       idx, nvalue, val, dz->host, dz->port,
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
		syslog(LOG_WARNING, "domoticz idx %d: %.40s", idx,
		       got ? resp : "no response");
		return -1;
	}
	if (!strstr(resp, "\"OK\"")) {
		syslog(LOG_WARNING, "domoticz idx %d: update rejected (wrong idx?)", idx);
		return -1;
	}
	syslog(LOG_DEBUG, "domoticz idx %d <- %s", idx, svalue);
	return 0;
}
