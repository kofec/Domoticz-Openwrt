/*
 * SEMS+ web API, see sems.h. In short:
 *
 *   login    POST {url}/web/sems/sems-user/api/v1/auth/cross-login with
 *            {account, pwd: base64(md5hex(password)), ...}; the reply has
 *            the token and "api", the gateway for everything else
 *   headers  token (the token as JSON), X-Signature =
 *            base64(sha256hex("ms@uid@token") + "@ms") and browser headers;
 *            without them the login answers "100004 parameter error"
 *   data     GET {api}/sems-plant/api/stations/flow, .../telemetry, ...
 *
 * An expired token (C0602, C0607, A0301, 100002 or HTTP 401) gets one new
 * login and a second try.
 */
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <time.h>

#include <curl/curl.h>
#include <json-c/json.h>
#include <openssl/evp.h>

#include "http.h"
#include "sems.h"

#define LOGIN_PATH	"/web/sems/sems-user/api/v1/auth/cross-login"
#define PLANT		"/sems-plant/api"

static const char *const browser_headers[] = {
	"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
		"(KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36",
	"Accept: application/json, text/plain, */*",
	"Content-Type: application/json",
	"Access-Control-Allow-Origin: *",
	"currentLang: en",
	"neutral: 0",
	"os: Windows 10.0 desktop",
	"brand: Chrome 140.0 en-US Europe/Warsaw",
};

#define N_BROWSER (sizeof(browser_headers) / sizeof(browser_headers[0]))

static void hex(const unsigned char *in, size_t n, char *out)
{
	for (size_t i = 0; i < n; i++)
		sprintf(out + 2 * i, "%02x", in[i]);
}

static void random_hex(char *out, size_t bytes)
{
	unsigned char buf[16];
	FILE *f = fopen("/dev/urandom", "r");

	if (!f || fread(buf, 1, bytes, f) != bytes)
		for (size_t i = 0; i < bytes; i++)
			buf[i] = rand();
	if (f)
		fclose(f);
	hex(buf, bytes, out);
}

int sems_init(struct sems *s, const char *url, const char *account,
	      const char *password, const char *station, int freshness)
{
	unsigned char md[EVP_MAX_MD_SIZE];
	unsigned int mdlen;
	char mdhex[2 * EVP_MAX_MD_SIZE + 1];

	memset(s, 0, sizeof(*s));
	snprintf(s->url, sizeof(s->url), "%s", url);
	if (*s->url && s->url[strlen(s->url) - 1] == '/')
		s->url[strlen(s->url) - 1] = 0;
	snprintf(s->account, sizeof(s->account), "%s", account);
	snprintf(s->station, sizeof(s->station), "%s", station);
	s->freshness = freshness;
	s->timeout = 20;

	/* the plain password is not kept, only what the login form sends */
	if (!EVP_Digest(password, strlen(password), md, &mdlen, EVP_md5(), NULL))
		return -1;
	hex(md, mdlen, mdhex);
	EVP_EncodeBlock((unsigned char *)s->pwd, (unsigned char *)mdhex, 2 * mdlen);

	s->share = curl_share_init();
	if (s->share)
		curl_share_setopt(s->share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
	return 0;
}

void sems_free(struct sems *s)
{
	if (s->token)
		json_object_put(s->token);
	if (s->share)
		curl_share_cleanup(s->share);
	s->token = NULL;
	s->share = NULL;
}

static const char *jstr(struct json_object *o, const char *key)
{
	struct json_object *v;

	if (!o || !json_object_object_get_ex(o, key, &v) || !v)
		return NULL;
	return json_object_get_string(v);
}

static struct json_object *jget(struct json_object *o, const char *key)
{
	struct json_object *v;

	return o && json_object_object_get_ex(o, key, &v) ? v : NULL;
}

/* Header lines for one request, kept until the request is done */
struct headers {
	char token[1024], sig[160], uuid[64], trace[96], origin[128], referer[128];
	const char *list[N_BROWSER + 8];
};

static void build_headers(const struct sems *s, struct json_object *token,
			  struct headers *h)
{
	unsigned char md[EVP_MAX_MD_SIZE];
	unsigned int mdlen = 0;
	char plain[512], mdhex[2 * EVP_MAX_MD_SIZE + 1] = "", sig[256], r[33], t[17];
	const char *uid = jstr(token, "uid"), *tok = jstr(token, "token");
	struct timespec now;
	long long ms;
	size_t n = 0;

	clock_gettime(CLOCK_REALTIME, &now);
	ms = (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
	snprintf(plain, sizeof(plain), "%lld@%s@%s", ms, uid ? uid : "", tok ? tok : "");
	if (EVP_Digest(plain, strlen(plain), md, &mdlen, EVP_sha256(), NULL))
		hex(md, mdlen, mdhex);
	snprintf(sig, sizeof(sig), "%s@%lld", mdhex, ms);
	n = snprintf(h->sig, sizeof(h->sig), "X-Signature: ");
	EVP_EncodeBlock((unsigned char *)h->sig + n, (unsigned char *)sig, strlen(sig));

	snprintf(h->token, sizeof(h->token), "token: %s",
		 json_object_to_json_string_ext(token, JSON_C_TO_STRING_PLAIN |
						JSON_C_TO_STRING_NOSLASHESCAPE));
	random_hex(r, 16);
	/* uuid4: version nibble 4, variant 8..b */
	snprintf(h->uuid, sizeof(h->uuid), "uuid: %.8s-%.4s-4%.3s-a%.3s-%.12s",
		 r, r + 8, r + 13, r + 17, r + 20);
	random_hex(r, 16);
	random_hex(t, 8);
	snprintf(h->trace, sizeof(h->trace), "traceparent: 00-%s-%s-01", r, t);
	snprintf(h->origin, sizeof(h->origin), "Origin: %s", s->url);
	snprintf(h->referer, sizeof(h->referer), "Referer: %s/", s->url);

	n = 0;
	for (size_t i = 0; i < N_BROWSER; i++)
		h->list[n++] = browser_headers[i];
	h->list[n++] = h->origin;
	h->list[n++] = h->referer;
	h->list[n++] = h->token;
	h->list[n++] = h->sig;
	h->list[n++] = h->uuid;
	h->list[n++] = h->trace;
	h->list[n] = NULL;
}

/* "code" may be a string or a number */
static int code_is(struct json_object *root, const char *const *codes)
{
	struct json_object *c = jget(root, "code");
	const char *v = c ? json_object_get_string(c) : NULL;

	for (; v && *codes; codes++)
		if (!strcmp(v, *codes))
			return 1;
	return 0;
}

static const char *const code_ok[] = { "00000", "0", NULL };
static const char *const code_logged_out[] = { "C0602", "C0607", "A0301", "100002", NULL };

static void describe(struct json_object *root, const char *what, char *err, size_t errlen)
{
	const char *d = jstr(root, "description");

	if (!d)
		d = jstr(root, "msg");
	if (!d)
		d = jstr(root, "translationCode");
	snprintf(err, errlen, "SEMS+ %s: %s %s", what, jstr(root, "code") ?: "?", d ?: "");
}

/* One request; *root is the parsed reply. -1 transport/HTTP/JSON error. */
static int raw(struct sems *s, const char *url, const char *post, struct json_object *token,
	       struct json_object **root, long *code, char *err, size_t errlen)
{
	struct headers h;
	struct http_reply r;
	const char *last = strrchr(url, '/');
	char what[64];
	int rc;

	/* last path element without the query (station id) for the log */
	snprintf(what, sizeof(what), "%.*s", last ? (int)strcspn(last, "?") : 0, last ? last : "");
	*root = NULL;
	build_headers(s, token, &h);
	rc = http_request(url, post, h.list, s->timeout, s->share, &r);
	*code = r.code;
	if (rc) {
		snprintf(err, errlen, "SEMS+%s: %s", what, r.err);
		http_reply_free(&r);
		return -1;
	}
	if (r.code == 200 && r.data)
		*root = json_tokener_parse(r.data);
	http_reply_free(&r);
	if (*code == 401)
		return 0;
	if (*code != 200 || !*root) {
		snprintf(err, errlen, "SEMS+%s: HTTP %ld%s", what, *code,
			 *code == 200 ? ", not JSON" : "");
		return -1;
	}
	return 0;
}

static int login(struct sems *s, char *err, size_t errlen)
{
	struct json_object *tok0, *body, *root, *data;
	const char *api;
	char url[160];
	long code;
	int ret = -1;

	tok0 = json_object_new_object();
	json_object_object_add(tok0, "uid", json_object_new_string(""));
	json_object_object_add(tok0, "timestamp", json_object_new_int(0));
	json_object_object_add(tok0, "token", json_object_new_string(""));
	json_object_object_add(tok0, "client", json_object_new_string("semsPlusWeb"));
	json_object_object_add(tok0, "version", json_object_new_string(""));
	json_object_object_add(tok0, "language", json_object_new_string("en"));

	body = json_object_new_object();
	json_object_object_add(body, "account", json_object_new_string(s->account));
	json_object_object_add(body, "pwd", json_object_new_string(s->pwd));
	json_object_object_add(body, "agreement", json_object_new_int(1));
	json_object_object_add(body, "isLocal", json_object_new_boolean(0));
	json_object_object_add(body, "isChinese", json_object_new_boolean(0));

	snprintf(url, sizeof(url), "%s" LOGIN_PATH, s->url);
	if (raw(s, url, json_object_to_json_string_ext(body, JSON_C_TO_STRING_PLAIN),
		tok0, &root, &code, err, errlen))
		goto out;
	data = jget(root, "data");
	if (code != 200 || !code_is(root, code_ok) || !json_object_is_type(data, json_type_object)) {
		if (code == 200)
			describe(root, "login rejected", err, errlen);
		else
			snprintf(err, errlen, "SEMS+ login: HTTP %ld", code);
		goto out;
	}
	if (jget(data, "mfaRequired") && json_object_get_boolean(jget(data, "mfaRequired"))) {
		snprintf(err, errlen, "SEMS+ account requires two-factor login");
		goto out;
	}
	json_object_object_add(data, "language", json_object_new_string("en"));
	api = jstr(data, "api");
	snprintf(s->api, sizeof(s->api), "%s", api && *api ? api : "");
	if (!*s->api)
		snprintf(s->api, sizeof(s->api), "%s/web/sems", s->url);
	if (s->api[strlen(s->api) - 1] == '/')
		s->api[strlen(s->api) - 1] = 0;
	if (s->token)
		json_object_put(s->token);
	s->token = json_object_get(data);
	syslog(LOG_INFO, "SEMS+: logged in (%s)", s->api);
	ret = 0;
out:
	if (root)
		json_object_put(root);
	json_object_put(body);
	json_object_put(tok0);
	return ret;
}

/* GET (post == NULL) or POST {api}path, *data gets a reference to "data" */
static int call(struct sems *s, const char *path, const char *post,
		struct json_object **data, char *err, size_t errlen)
{
	struct json_object *root;
	char url[384];
	long code;

	*data = NULL;
	for (int tries = 0; tries < 2; tries++) {
		if (!s->token && login(s, err, errlen))
			return -1;
		snprintf(url, sizeof(url), "%s%s", s->api, path);
		if (raw(s, url, post, s->token, &root, &code, err, errlen))
			return -1;
		if (code == 401 || code_is(root, code_logged_out)) {
			syslog(LOG_DEBUG, "SEMS+: token expired, logging in again");
			if (root)
				json_object_put(root);
			json_object_put(s->token);
			s->token = NULL;
			continue;
		}
		if (!code_is(root, code_ok)) {
			char what[64];

			snprintf(what, sizeof(what), "%.*s", (int)strcspn(path, "?"), path);
			describe(root, what, err, errlen);
			json_object_put(root);
			return -1;
		}
		*data = json_object_get(jget(root, "data"));
		json_object_put(root);
		return 0;
	}
	snprintf(err, errlen, "SEMS+: still logged out after a new login");
	return -1;
}

/* "2026-09-24T18:40:00" or with a space: local station time, as the router */
static time_t sems_time(const char *t)
{
	struct tm tm = { 0 };

	if (!t || sscanf(t, "%d-%d-%d%*c%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
			 &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6)
		return (time_t)-1;
	tm.tm_year -= 1900;
	tm.tm_mon -= 1;
	tm.tm_isdst = -1;
	return mktime(&tm);
}

static int find_sn(struct sems *s, char *err, size_t errlen)
{
	struct json_object *data, *groups;
	char path[160];

	if (*s->sn)
		return 0;
	snprintf(path, sizeof(path), PLANT "/stations/device/all-status?stationId=%s", s->station);
	if (call(s, path, NULL, &data, err, errlen))
		return -1;
	groups = jget(data, "deviceDetailList");
	for (size_t i = 0; groups && i < json_object_array_length(groups) && !*s->sn; i++) {
		struct json_object *g = json_object_array_get_idx(groups, i);
		struct json_object *st = jget(g, "statusDetailList");

		if (!jstr(g, "deviceType") || strcmp(jstr(g, "deviceType"), "INVERTER"))
			continue;
		for (size_t j = 0; st && j < json_object_array_length(st) && !*s->sn; j++) {
			struct json_object *sns = jget(json_object_array_get_idx(st, j), "snList");

			if (sns && json_object_array_length(sns))
				snprintf(s->sn, sizeof(s->sn), "%s",
					 json_object_get_string(json_object_array_get_idx(sns, 0)));
		}
	}
	json_object_put(data);
	if (!*s->sn) {
		snprintf(err, errlen, "SEMS+ sees no inverter in the station, check the station id");
		return -1;
	}
	syslog(LOG_INFO, "SEMS+: inverter %s", s->sn);
	return 0;
}

/* Value and unit of factor "code" in telemetry/telecounting groups */
static int factor(struct json_object *groups, const char *code, double *v, const char **unit)
{
	for (size_t i = 0; groups && i < json_object_array_length(groups); i++) {
		struct json_object *f = jget(json_object_array_get_idx(groups, i), "factors");

		for (size_t j = 0; f && j < json_object_array_length(f); j++) {
			struct json_object *x = json_object_array_get_idx(f, j), *d;

			if (!jstr(x, "code") || strcmp(jstr(x, "code"), code))
				continue;
			*unit = jstr(x, "unit") ?: "";
			/* at night telemetry comes without "data" */
			d = jget(x, "data");
			if (!d || json_object_is_type(d, json_type_null))
				return -1;
			*v = json_object_get_double(d);
			return 0;
		}
	}
	return -1;
}

int sems_read(struct sems *s, struct gw_reading *r, char *err, size_t errlen)
{
	struct json_object *flow, *tele = NULL, *count = NULL;
	const char *refresh, *unit;
	char path[256];
	double v;
	time_t t;
	int ret = -1;

	if (find_sn(s, err, errlen))
		return -1;
	snprintf(path, sizeof(path), PLANT "/stations/flow?stationId=%s", s->station);
	if (call(s, path, NULL, &flow, err, errlen))
		return -1;
	refresh = jstr(flow, "refreshTime");
	t = sems_time(refresh);
	if (t == (time_t)-1) {
		snprintf(err, errlen, "SEMS+ gave no data time (refreshTime: %s)", refresh ?: "null");
		goto out;
	}
	if (time(NULL) - t > s->freshness) {
		/* the inverter sends nothing (night, Wi-Fi down): an echo */
		snprintf(err, errlen, "SEMS+: newest inverter data from %s", refresh);
		ret = 1;
		goto out;
	}

	snprintf(path, sizeof(path), PLANT "/equipments/%s/telemetry?deviceType=INVERTER&pwId=%s",
		 s->sn, s->station);
	if (call(s, path, NULL, &tele, err, errlen))
		goto out;
	snprintf(path, sizeof(path), PLANT "/equipments/%s/telecounting?deviceType=INVERTER&pwId=%s",
		 s->sn, s->station);
	if (call(s, path, NULL, &count, err, errlen))
		goto out;

	memset(r, 0, sizeof(*r));
	r->t = t;
	r->total_wh = r->current_da = r->freq_ch = r->pv1_dv = -1;
	r->temp_dc = LONG_MIN;
	r->mode = -1;
	r->source = "SEMS+";
	if (factor(tele, "pAc", &v, &unit)) {
		snprintf(err, errlen, "SEMS+: no power (pAc) in the telemetry");
		goto out;
	}
	r->power_w = lround(!strcasecmp(unit, "kw") ? v * 1000 : v);
	if (!factor(tele, "Vac", &v, &unit) || !factor(tele, "PHASE-A:Vac", &v, &unit))
		r->voltage_dv = lround(v * 10);
	if (!factor(count, "proPvStatsToday", &v, &unit))
		r->today_wh = lround(v * 1000);
	ret = 0;
out:
	json_object_put(flow);
	if (tele)
		json_object_put(tele);
	if (count)
		json_object_put(count);
	return ret;
}

/* Points of metric "code" (pAc, Vac) of one day, as the site's chart */
static int series(struct sems *s, const char *day, const char *code, time_t **ts,
		  double **vals, int *n, const char **unit, struct json_object **keep,
		  char *err, size_t errlen)
{
	struct json_object *body, *data, *xs, *y0, *ys;
	char path[160], start[24], end[24];
	int rc;

	*n = 0;
	*ts = NULL;
	*vals = NULL;
	body = json_object_new_object();
	snprintf(start, sizeof(start), "%s 00:00:00", day);
	snprintf(end, sizeof(end), "%s 23:59:59", day);
	json_object_object_add(body, "sn", json_object_new_string(s->sn));
	json_object_object_add(body, "stationId", json_object_new_string(s->station));
	json_object_object_add(body, "deviceType", json_object_new_string("INVERTER"));
	json_object_object_add(body, "metricCode", json_object_new_array());
	json_object_array_add(jget(body, "metricCode"), json_object_new_string(code));
	json_object_object_add(body, "startDateTime", json_object_new_string(start));
	json_object_object_add(body, "endDateTime", json_object_new_string(end));
	snprintf(path, sizeof(path), PLANT "/portal/equipments/%s/metricTimeSeriesData", s->sn);
	rc = call(s, path, json_object_to_json_string_ext(body, JSON_C_TO_STRING_PLAIN),
		  &data, err, errlen);
	json_object_put(body);
	if (rc)
		return -1;
	*keep = data;

	xs = jget(data, "xAxis");
	y0 = jget(data, "yAxis") ? json_object_array_get_idx(jget(data, "yAxis"), 0) : NULL;
	ys = jget(y0, "data");
	*unit = jstr(y0, "unit") ?: "";
	if (!xs || !ys)
		return 0;
	*ts = calloc(json_object_array_length(xs) + 1, sizeof(**ts));
	*vals = calloc(json_object_array_length(xs) + 1, sizeof(**vals));
	if (!*ts || !*vals)
		return -1;
	for (size_t i = 0; i < json_object_array_length(xs) && i < json_object_array_length(ys); i++) {
		struct json_object *x = json_object_array_get_idx(xs, i);
		struct json_object *v = json_object_array_get_idx(ys, i);
		struct json_object *ce = jget(x, "isCE");
		time_t t;

		/* edge points (00:00, 23:55) added by the server, and gaps */
		if ((ce && json_object_get_boolean(ce)) || !v ||
		    json_object_is_type(v, json_type_null))
			continue;
		t = sems_time(jstr(x, "xAxi"));
		if (t == (time_t)-1)
			continue;
		(*ts)[*n] = t;
		(*vals)[*n] = json_object_get_double(v);
		(*n)++;
	}
	return 0;
}

static double day_total(struct sems *s, const char *day, char *err, size_t errlen)
{
	struct json_object *body, *data, *ys;
	char path[160], start[24], end[24];
	double total = -1;

	body = json_object_new_object();
	snprintf(start, sizeof(start), "%s 00:00:00", day);
	snprintf(end, sizeof(end), "%s 23:59:59", day);
	json_object_object_add(body, "sn", json_object_new_string(s->sn));
	json_object_object_add(body, "stationId", json_object_new_string(s->station));
	json_object_object_add(body, "deviceType", json_object_new_string("INVERTER"));
	json_object_object_add(body, "group", json_object_new_string("electricity_histogram"));
	json_object_object_add(body, "module", json_object_new_string("chart"));
	json_object_object_add(body, "timeGranularity", json_object_new_string("2"));
	json_object_object_add(body, "startDateTime", json_object_new_string(start));
	json_object_object_add(body, "endDateTime", json_object_new_string(end));
	snprintf(path, sizeof(path), PLANT "/portal/equipments/%s/timeSeriesData", s->sn);
	if (call(s, path, json_object_to_json_string_ext(body, JSON_C_TO_STRING_PLAIN),
		 &data, err, errlen)) {
		json_object_put(body);
		return -1;
	}
	json_object_put(body);
	ys = jget(data, "yAxis");
	for (size_t i = 0; ys && i < json_object_array_length(ys); i++) {
		struct json_object *y = json_object_array_get_idx(ys, i), *d = jget(y, "data");

		if (jstr(y, "code") && !strcmp(jstr(y, "code"), "proPvStats") &&
		    d && json_object_array_length(d))
			total = json_object_get_double(json_object_array_get_idx(d, 0));
	}
	json_object_put(data);
	return total;
}

int sems_history(struct sems *s, const char *day, struct pvo_point **pts, int *n,
		 double *total_kwh, char *err, size_t errlen)
{
	struct json_object *pj = NULL, *vj = NULL;
	time_t *pt = NULL, *vt = NULL, prev = 0;
	double *pv = NULL, *vv = NULL, wh = 0, scale = 1;
	const char *punit = "", *vunit = "";
	int np = 0, nv = 0, ret = -1;

	*pts = NULL;
	*n = 0;
	*total_kwh = -1;
	if (find_sn(s, err, errlen))
		return -1;
	if (series(s, day, "pAc", &pt, &pv, &np, &punit, &pj, err, errlen) ||
	    series(s, day, "Vac", &vt, &vv, &nv, &vunit, &vj, err, errlen))
		goto out;
	*total_kwh = day_total(s, day, err, errlen);
	if (!np) {
		ret = 0;
		goto out;
	}
	*pts = calloc(np, sizeof(**pts));
	if (!*pts)
		goto out;

	/* energy integrated from the power, as gw2pvo --date did */
	for (int i = 0; i < np; i++) {
		double w = !strcasecmp(punit, "kw") ? pv[i] * 1000 : pv[i];
		long step = i ? pt[i] - prev : 300;

		if (step > 600)
			step = 600;
		wh += w * step / 3600;
		prev = pt[i];
		(*pts)[i].t = pt[i];
		(*pts)[i].power_w = lround(w);
		(*pts)[i].energy_wh = lround(wh);
		for (int j = 0; j < nv; j++)
			if (vt[j] == pt[i])
				(*pts)[i].voltage_dv = lround(vv[j] * 10);
	}
	/* the integral of 5-minute samples is a few percent off the counter */
	if (*total_kwh > 0 && wh > 0)
		scale = *total_kwh * 1000 / wh;
	for (int i = 0; i < np; i++)
		(*pts)[i].energy_wh = lround((*pts)[i].energy_wh * scale);
	*n = np;
	ret = 0;
out:
	if (pj)
		json_object_put(pj);
	if (vj)
		json_object_put(vj);
	free(pt);
	free(pv);
	free(vt);
	free(vv);
	return ret;
}
