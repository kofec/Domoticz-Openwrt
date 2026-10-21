#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

struct http_reply {
	char *data;		/* NUL terminated, NULL when empty */
	size_t len;
	long code;		/* HTTP status */
	char err[256];		/* transport error, empty when none */
};

/*
 * HTTPS request with libcurl. post == NULL is a GET. headers is a NULL
 * terminated list of "Name: value" lines. share is a CURLSH * for cookies
 * or NULL. A transport error (timeout, TLS handshake) is retried once
 * after 5 s. Returns 0 when an HTTP answer came back (any status),
 * -1 with r->err set otherwise. Free r with http_reply_free().
 */
int http_request(const char *url, const char *post, const char *const *headers,
		 long timeout, void *share, struct http_reply *r);
void http_reply_free(struct http_reply *r);

#endif
