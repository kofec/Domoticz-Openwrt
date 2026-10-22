/*
 * HTTPS client for PVOutput and SEMS+. OpenWrt routers running Domoticz
 * already have libcurl and OpenSSL loaded, so this costs no flash and
 * little RAM. The easy handle is freed after every request: one request
 * per 5 minutes does not need a kept-alive connection.
 */
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <curl/curl.h>

#include "http.h"

#define HTTP_MAX_REPLY	(2 << 20)

static size_t on_data(char *ptr, size_t size, size_t n, void *ud)
{
	struct http_reply *r = ud;
	size_t len = size * n;
	char *p;

	if (r->len + len > HTTP_MAX_REPLY)
		return 0;
	p = realloc(r->data, r->len + len + 1);
	if (!p)
		return 0;
	memcpy(p + r->len, ptr, len);
	r->data = p;
	r->len += len;
	r->data[r->len] = 0;
	return len;
}

int http_request(const char *url, const char *post, const char *const *headers,
		 long timeout, void *share, struct http_reply *r)
{
	static int curl_ready;
	struct curl_slist *hdrs = NULL;
	char err[CURL_ERROR_SIZE];
	CURLcode rc = CURLE_FAILED_INIT;
	CURL *c;

	memset(r, 0, sizeof(*r));
	if (!curl_ready) {
		if (curl_global_init(CURL_GLOBAL_DEFAULT)) {
			snprintf(r->err, sizeof(r->err), "curl_global_init failed");
			return -1;
		}
		curl_ready = 1;
	}
	for (; headers && *headers; headers++)
		hdrs = curl_slist_append(hdrs, *headers);

	/* A TLS handshake on a busy router can time out, try once more */
	for (int tries = 0; tries < 2; tries++) {
		if (tries)
			sleep(5);
		c = curl_easy_init();
		if (!c)
			break;
		free(r->data);
		r->data = NULL;
		r->len = 0;
		err[0] = 0;
		curl_easy_setopt(c, CURLOPT_URL, url);
		if (post)
			curl_easy_setopt(c, CURLOPT_POSTFIELDS, post);
		curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
		curl_easy_setopt(c, CURLOPT_TIMEOUT, timeout);
		curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(c, CURLOPT_ERRORBUFFER, err);
		curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_data);
		curl_easy_setopt(c, CURLOPT_WRITEDATA, r);
		curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
		if (share) {
			curl_easy_setopt(c, CURLOPT_SHARE, share);
			curl_easy_setopt(c, CURLOPT_COOKIEFILE, "");
		}
		rc = curl_easy_perform(c);
		curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &r->code);
		curl_easy_cleanup(c);
		if (rc == CURLE_OK)
			break;
		syslog(LOG_DEBUG, "%s: %s", url, *err ? err : curl_easy_strerror(rc));
	}
	curl_slist_free_all(hdrs);

	if (rc != CURLE_OK) {
		snprintf(r->err, sizeof(r->err), "%s",
			 *err ? err : curl_easy_strerror(rc));
		return -1;
	}
	return 0;
}

void http_reply_free(struct http_reply *r)
{
	free(r->data);
	r->data = NULL;
	r->len = 0;
}
