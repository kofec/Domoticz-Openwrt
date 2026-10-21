/*
 * goodwe-reader - read a GoodWe inverter (DT family, e.g. GW3000-XS) in the
 * LAN, fall back to SEMS+ when it does not answer, push the values to
 * Domoticz and PVOutput, and once a day fill gaps of yesterday in PVOutput
 * from the SEMS+ power curve.
 *
 * Configuration: /etc/config/goodwe-reader, one process per inverter section.
 */
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "domoticz.h"
#include "goodwe.h"
#include "pvoutput.h"
#include "sems.h"
#include "uciutil.h"

/* Grid voltage outside this range is a bad reading, not sent anywhere */
#define VOLT_MIN_DV	1800
#define VOLT_MAX_DV	2600

struct cfg {
	char name[32];
	char host[64];
	int port;
	int interval;
	int timeout;
	int tries;
	int idx_power;		/* Custom sensor or Usage, W */
	int idx_voltage;	/* Voltage */
	int idx_energy;		/* Electric (Instant+Counter): W;Wh total */
	int idx_today_energy;	/* Custom sensor, Wh */
	int idx_total_energy;	/* Custom sensor, kWh */
	int idx_temperature;
	int idx_frequency;
	int idx_status;		/* Text: Wait/Normal/Error/Check */
	int zero_delay;
	int notify_after;
	char notify_subject[64];
	char notify_text[128];
	char notify_back_text[128];
	char notify_error_text[64];
	int night;		/* skip the night, sunrise/sunset from Domoticz */
	int night_margin;	/* minutes after sunset / before sunrise */
	int night_interval;	/* longest sleep at night [s] */
	int sems;		/* SEMS+ as fallback and for the daily check */
	int check_yesterday;
	char sems_url[96];
	int sems_freshness;
	char sems_file[128];
};

/* Monotonic clocks and what has been reported, see check_state() */
struct state {
	long last_ok;		/* last reading: zeroing the power */
	long last_contact;	/* last reading or alarm: the alarm */
	long alarm_since;	/* -1 = no alarm sent */
	int zeroed;
	char err[256];		/* last error without a reading */
	long err_since, err_logged;
	int err_count;
	char local_err[256];	/* set while readings come from SEMS+ */
	time_t pvo_last;	/* time of the last status sent */
	int last_mode;
	int sun_yday, sunrise, sunset, sun_warned;
	char checked[12];	/* yesterday that is complete in PVOutput */
	time_t next_check;
};

static volatile sig_atomic_t stop;

static void on_signal(int sig)
{
	(void)sig;
	stop = 1;
}

static long mono(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec;
}

static void duration(char *buf, size_t len, long s)
{
	snprintf(buf, len, "%ld h %ld min", s / 3600, s / 60 % 60);
}

static int load_config(const char *confdir, const char *section, struct cfg *c,
		       struct domoticz *dz, struct pvoutput *pvo)
{
	struct uci_context *ctx = uci_alloc_context();
	struct uci_package *pkg = NULL;
	struct uci_section *s;
	int ret = -1;

	if (!ctx)
		return -1;
	if (confdir)
		uci_set_confdir(ctx, confdir);
	if (uci_load(ctx, "goodwe-reader", &pkg)) {
		fprintf(stderr, "cannot load config goodwe-reader\n");
		goto out;
	}
	load_domoticz(ctx, pkg, dz);

	s = find_section(pkg, "inverter", section);
	if (!s) {
		fprintf(stderr, "no inverter section %s\n", section ? section : "");
		goto out;
	}
#define STR(f, key, def) snprintf(c->f, sizeof(c->f), "%s", opt_str(ctx, s, key, def))
	snprintf(c->name, sizeof(c->name), "%s", s->e.name);
	STR(host, "host", "");
	c->port = opt_num(ctx, s, "port", 8899);
	c->interval = opt_num(ctx, s, "interval", 300);
	c->timeout = opt_num(ctx, s, "timeout", 3);
	c->tries = opt_num(ctx, s, "tries", 3);
	c->idx_power = opt_num(ctx, s, "idx_power", 0);
	c->idx_voltage = opt_num(ctx, s, "idx_voltage", 0);
	c->idx_energy = opt_num(ctx, s, "idx_energy", 0);
	c->idx_today_energy = opt_num(ctx, s, "idx_today_energy", 0);
	c->idx_total_energy = opt_num(ctx, s, "idx_total_energy", 0);
	c->idx_temperature = opt_num(ctx, s, "idx_temperature", 0);
	c->idx_frequency = opt_num(ctx, s, "idx_frequency", 0);
	c->idx_status = opt_num(ctx, s, "idx_status", 0);
	c->zero_delay = opt_num(ctx, s, "zero_delay", 900);
	c->notify_after = opt_num(ctx, s, "notify_after", 86400);
	STR(notify_subject, "notify_subject", "goodwe-reader");
	STR(notify_text, "notify_text", "No data from the inverter for");
	STR(notify_back_text, "notify_back_text", "Inverter data back after");
	STR(notify_error_text, "notify_error_text", "Last error:");
	c->night = opt_num(ctx, s, "night", 1);
	c->night_margin = opt_num(ctx, s, "night_margin", 30);
	c->night_interval = opt_num(ctx, s, "night_interval", 3600);
	c->sems = opt_num(ctx, s, "sems", 1);
	c->check_yesterday = opt_num(ctx, s, "check_yesterday", 1);
	STR(sems_url, "sems_url", "https://eu-semsplus.goodwe.com");
	c->sems_freshness = opt_num(ctx, s, "sems_freshness", 1800);
	STR(sems_file, "sems_file", "/root/gw2pvo.cfg");
#undef STR
	load_pvoutput(ctx, s, c->name, pvo);

	if (!*c->host) {
		fprintf(stderr, "%s: host (inverter address) is required\n", c->name);
		goto out;
	}
	if (c->interval < 30)
		c->interval = 30;
	ret = 0;
out:
	uci_free_context(ctx);
	return ret;
}

/* Account, password and station from gw2pvo.cfg, as the Python monitor */
static int open_sems(const struct cfg *c, struct sems *sems)
{
	char account[96], password[128], station[64];

	if (file_value(c->sems_file, "gw_account", account, sizeof(account)) ||
	    file_value(c->sems_file, "gw_password", password, sizeof(password)) ||
	    file_value(c->sems_file, "gw_station_id", station, sizeof(station))) {
		syslog(LOG_WARNING, "%s: no gw_account/gw_password/gw_station_id in %s, "
		       "SEMS+ disabled", c->name, c->sems_file);
		return -1;
	}
	if (sems_init(sems, c->sems_url, account, password, station, c->sems_freshness)) {
		memset(password, 0, sizeof(password));
		return -1;
	}
	memset(password, 0, sizeof(password));
	return 0;
}

static void print_reading(const struct gw_reading *r)
{
	char t[24];

	strftime(t, sizeof(t), "%H:%M:%S", localtime(&r->t));
	printf("%s %s: %ld W, %ld.%02ld kWh today, %ld.%ld V", r->source, t, r->power_w,
	       r->today_wh / 1000, r->today_wh % 1000 / 10, r->voltage_dv / 10, r->voltage_dv % 10);
	if (r->current_da >= 0)
		printf(", %ld.%ld A", r->current_da / 10, r->current_da % 10);
	if (r->freq_ch >= 0)
		printf(", %ld.%02ld Hz", r->freq_ch / 100, r->freq_ch % 100);
	if (r->pv1_dv >= 0)
		printf(", PV %ld.%ld V", r->pv1_dv / 10, r->pv1_dv % 10);
	if (r->temp_dc != LONG_MIN)
		printf(", %.1f C", r->temp_dc / 10.0);
	if (r->total_wh >= 0)
		printf(", total %ld.%ld kWh", r->total_wh / 1000, r->total_wh % 1000 / 100);
	if (r->mode >= 0)
		printf(", %s, inverter clock %s", goodwe_mode(r->mode), r->clock);
	printf("\n");
}

static void send_value(const struct domoticz *dz, int idx, const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));

static void send_value(const struct domoticz *dz, int idx, const char *fmt, ...)
{
	char v[48];
	va_list ap;

	if (!idx)
		return;
	va_start(ap, fmt);
	vsnprintf(v, sizeof(v), fmt, ap);
	va_end(ap);
	domoticz_udevice(dz, idx, 0, v);
}

/* An error without a reading: logged once, repeats once an hour with a count */
static void report_error(struct state *st, const char *name, const char *text)
{
	long now = mono();

	if (strcmp(text, st->err)) {
		snprintf(st->err, sizeof(st->err), "%s", text);
		st->err_since = now;
		st->err_count = 0;
		st->err_logged = -1;
	}
	st->err_count++;
	if (st->err_logged >= 0 && now - st->err_logged < 3600) {
		syslog(LOG_DEBUG, "%s: %s", name, text);
		return;
	}
	st->err_logged = now;
	if (st->err_count > 1) {
		char d[32];

		duration(d, sizeof(d), now - st->err_since);
		syslog(LOG_WARNING, "%s: %s (repeating for %s, %d times)", name, text, d,
		       st->err_count);
	} else {
		syslog(LOG_WARNING, "%s: %s", name, text);
	}
}

static void report_reading(const struct cfg *c, const struct domoticz *dz,
			   const struct pvoutput *pvo, struct state *st,
			   const struct gw_reading *r)
{
	long now = mono();
	char text[256], d[32];

	if (st->alarm_since >= 0) {
		duration(d, sizeof(d), now - st->alarm_since);
		syslog(LOG_INFO, "%s: inverter data back after %s", c->name, d);
		snprintf(text, sizeof(text), "%s %s.", c->notify_back_text, d);
		domoticz_notify(dz, c->notify_subject, text);
		st->alarm_since = -1;
	}
	if (*st->err) {
		syslog(LOG_INFO, "%s: readings back after %d messages without one (last: %s)",
		       c->name, st->err_count, st->err);
		st->err[0] = 0;
	}
	st->last_ok = st->last_contact = now;
	st->zeroed = 0;

	if (r->voltage_dv > VOLT_MIN_DV && r->voltage_dv < VOLT_MAX_DV)
		send_value(dz, c->idx_voltage, "%ld.%ld", r->voltage_dv / 10, r->voltage_dv % 10);
	else
		syslog(LOG_WARNING, "%s: voltage out of range %ld.%ld V", c->name,
		       r->voltage_dv / 10, r->voltage_dv % 10);
	send_value(dz, c->idx_power, "%ld", r->power_w);
	send_value(dz, c->idx_today_energy, "%ld", r->today_wh);
	if (r->total_wh >= 0) {
		send_value(dz, c->idx_total_energy, "%ld.%ld", r->total_wh / 1000,
			   r->total_wh % 1000 / 100);
		send_value(dz, c->idx_energy, "%ld;%ld", r->power_w, r->total_wh);
	}
	if (r->temp_dc != LONG_MIN)
		send_value(dz, c->idx_temperature, "%.1f", r->temp_dc / 10.0);
	if (r->freq_ch >= 0)
		send_value(dz, c->idx_frequency, "%ld.%02ld", r->freq_ch / 100, r->freq_ch % 100);
	/* Text devices keep a log entry per update, send only changes */
	if (c->idx_status && r->mode >= 0 && r->mode != st->last_mode &&
	    !domoticz_udevice(dz, c->idx_status, 0, goodwe_mode(r->mode)))
		st->last_mode = r->mode;

	syslog(LOG_DEBUG, "%s: %s %ld W, %ld Wh today, %ld.%ld V", c->name, r->source,
	       r->power_w, r->today_wh, r->voltage_dv / 10, r->voltage_dv % 10);

	/* SEMS+ repeats its last data until the next refresh, send it once */
	if (*pvo->api_key && r->t != st->pvo_last) {
		struct pvo_point pt = {
			.t = r->t, .power_w = r->power_w, .energy_wh = r->today_wh,
			.voltage_dv = r->voltage_dv > VOLT_MIN_DV && r->voltage_dv < VOLT_MAX_DV ?
				      r->voltage_dv : 0,
		};

		if (!pvoutput_add_status(pvo, &pt, 0))
			st->pvo_last = r->t;
	}
}

static void zero_power(const struct cfg *c, const struct domoticz *dz, struct state *st,
		       const char *why)
{
	if (st->zeroed)
		return;
	syslog(LOG_INFO, "%s: %s, sending 0 W", c->name, why);
	send_value(dz, c->idx_power, "0");
	st->zeroed = 1;
}

/*
 * Zeroing after zero_delay without readings, a notification after
 * notify_after without contact, repeated every notify_after.
 */
static void check_state(const struct cfg *c, const struct domoticz *dz, struct state *st)
{
	long now = mono();
	char text[512], d[32];

	if (now - st->last_ok > c->zero_delay) {
		duration(d, sizeof(d), now - st->last_ok);
		snprintf(text, sizeof(text), "no reading for %s", d);
		zero_power(c, dz, st, text);
	}
	if (c->notify_after <= 0 || now - st->last_contact < c->notify_after)
		return;
	if (st->alarm_since < 0)
		st->alarm_since = st->last_contact;
	duration(d, sizeof(d), now - st->alarm_since);
	syslog(LOG_ERR, "%s: no data from the inverter for %s", c->name, d);
	snprintf(text, sizeof(text), "%s %s.", c->notify_text, d);
	if (*st->err)
		snprintf(text + strlen(text), sizeof(text) - strlen(text), " %s %s",
			 c->notify_error_text, st->err);
	domoticz_notify(dz, c->notify_subject, text);
	st->last_contact = now;
}

/*
 * Seconds to sleep instead of asking the inverter, 0 = daytime. The night
 * runs from sunset + margin to sunrise - margin, wrapping past midnight.
 * Without sunrise/sunset from Domoticz every hour counts as day: better ask
 * for nothing than sleep through production.
 */
static int night_sleep(const struct cfg *c, const struct domoticz *dz, struct state *st)
{
	time_t now = time(NULL);
	struct tm lt;
	int minute, start, end, length;

	if (!c->night || c->night_interval <= 0)
		return 0;
	localtime_r(&now, &lt);
	if (st->sun_yday != lt.tm_yday) {
		int rise, set;

		if (domoticz_sun(dz, &rise, &set)) {
			if (!st->sun_warned)
				syslog(LOG_WARNING, "%s: no sunrise/sunset from Domoticz, "
				       "night mode off", c->name);
			st->sun_warned = 1;
			/* yesterday's times are still good for a day or two */
			if (st->sun_yday < 0)
				return 0;
		} else {
			st->sunrise = rise;
			st->sunset = set;
			st->sun_yday = lt.tm_yday;
			st->sun_warned = 0;
		}
	}
	length = (st->sunrise - st->sunset + 1440) % 1440;
	if (length - 2 * c->night_margin < 60)
		return 0;
	start = (st->sunset + c->night_margin) % 1440;
	end = (st->sunrise - c->night_margin + 1440) % 1440;
	minute = lt.tm_hour * 60 + lt.tm_min;
	if (start <= end ? minute < start || minute >= end : minute < start && minute >= end)
		return 0;
	minute = (end - minute + 1440) % 1440;
	/* wake up at the end of the night at the latest */
	return minute * 60 < c->night_interval ? (minute * 60 > 60 ? minute * 60 : 60)
					       : c->night_interval;
}

static int read_once(const struct cfg *c, struct sems *sems, struct state *st,
		     struct gw_reading *r)
{
	char err[256], serr[256];
	int rc;

	if (!goodwe_read(c->host, c->port, c->timeout, c->tries, r, err, sizeof(err))) {
		if (*st->local_err) {
			syslog(LOG_INFO, "%s: local reading back (was: %s)", c->name, st->local_err);
			st->local_err[0] = 0;
		}
		return 0;
	}
	if (!sems) {
		report_error(st, c->name, err);
		return -1;
	}
	if (!*st->local_err)
		syslog(LOG_WARNING, "%s: local: %s, reading SEMS+", c->name, err);
	snprintf(st->local_err, sizeof(st->local_err), "%s", err);
	rc = sems_read(sems, r, serr, sizeof(serr));
	if (rc) {
		/* stale SEMS+ data is not contact: the alarm clock keeps running */
		report_error(st, c->name, serr);
		return -1;
	}
	return 0;
}

/* Statuses SEMS+ has and PVOutput has not; energy kept between neighbours */
static int missing(const struct pvo_point *pts, int n, const struct pvo_day *day,
		   struct pvo_point *out)
{
	int m = 0;

	for (int i = 0; i < n; i++) {
		struct tm lt;
		int minute;
		long wh = pts[i].energy_wh;

		localtime_r(&pts[i].t, &lt);
		minute = lt.tm_hour * 60 + lt.tm_min;
		if (day->have[minute])
			continue;
		/* PVOutput has the inverter counter, SEMS+ an integral: no going back */
		for (int j = minute - 1; j >= 0; j--)
			if (day->have[j] && day->energy_wh[j] >= 0) {
				if (wh < day->energy_wh[j])
					wh = day->energy_wh[j];
				break;
			}
		for (int j = minute + 1; j < 1440; j++)
			if (day->have[j] && day->energy_wh[j] >= 0) {
				if (wh > day->energy_wh[j])
					wh = day->energy_wh[j];
				break;
			}
		out[m] = pts[i];
		out[m].energy_wh = wh;
		m++;
	}
	return m;
}

/* "16:20-16:45, 17:05": consecutive 5-minute statuses joined, for the log */
static void ranges(const struct pvo_point *pts, int n, char *buf, size_t len)
{
	size_t o = 0;

	buf[0] = 0;
	for (int i = 0; i < n && o < len; i++) {
		int j = i;
		char a[8], b[8];

		while (j + 1 < n && pts[j + 1].t - pts[j].t <= 300)
			j++;
		strftime(a, sizeof(a), "%H:%M", localtime(&pts[i].t));
		strftime(b, sizeof(b), "%H:%M", localtime(&pts[j].t));
		o += snprintf(buf + o, len - o, "%s%s%s%s", o ? ", " : "", a,
			      j > i ? "-" : "", j > i ? b : "");
		i = j;
	}
}

/* Yesterday in PVOutput against the SEMS+ curve, gaps sent as a batch */
static int check_day(const struct cfg *c, const struct pvoutput *pvo, struct sems *sems,
		     const char *day, char *err, size_t errlen)
{
	struct pvo_point *pts, *gaps = NULL;
	struct pvo_day have;
	char ymd[12], list[256];
	double total;
	int n, m, ok, ret = -1;

	if (sems_history(sems, day, &pts, &n, &total, err, errlen))
		return -1;
	if (!n) {
		snprintf(err, errlen, "SEMS+ has no power curve for that day");
		goto out;
	}
	snprintf(ymd, sizeof(ymd), "%.4s%.2s%.2s", day, day + 5, day + 8);
	if (pvoutput_get_day(pvo, ymd, &have)) {
		snprintf(err, errlen, "PVOutput getstatus failed");
		goto out;
	}
	gaps = calloc(n, sizeof(*gaps));
	if (!gaps)
		goto out;
	m = missing(pts, n, &have, gaps);
	if (!m) {
		syslog(LOG_INFO, "%s: %s complete in PVOutput: %d statuses (SEMS+ %d, %.1f kWh)",
		       c->name, day, have.count, n, total);
		ret = 0;
		goto out;
	}
	ok = pvoutput_add_batch(pvo, gaps, m);
	if (ok < 0) {
		snprintf(err, errlen, "PVOutput addbatchstatus failed");
		goto out;
	}
	ranges(gaps, m, list, sizeof(list));
	syslog(ok == m ? LOG_INFO : LOG_WARNING,
	       "%s: %s in PVOutput: %d of %d SEMS+ statuses missing (%s), %d sent",
	       c->name, day, m, n, list, ok);
	ret = 0;
out:
	free(pts);
	free(gaps);
	return ret;
}

static void daily_check(const struct cfg *c, const struct pvoutput *pvo, struct sems *sems,
			struct state *st)
{
	time_t now = time(NULL), y = now - 86400;
	char day[12], err[256];

	if (!sems || !*pvo->api_key || !c->check_yesterday)
		return;
	strftime(day, sizeof(day), "%Y-%m-%d", localtime(&y));
	if (!strcmp(day, st->checked) || now < st->next_check)
		return;
	if (check_day(c, pvo, sems, day, err, sizeof(err))) {
		syslog(LOG_WARNING, "%s: %s in PVOutput not checked (%s), next try in 1 h",
		       c->name, day, err);
		st->next_check = now + 3600;
		return;
	}
	snprintf(st->checked, sizeof(st->checked), "%s", day);
}

/* -u DAY: the whole day from SEMS+ to PVOutput (with -n only shown) */
static int fill_day(const struct cfg *c, const struct pvoutput *pvo, struct sems *sems,
		    const char *day, int dry)
{
	struct pvo_point *pts;
	char err[256], a[8], b[8];
	double total;
	int n, ok, peak = 0;

	if (sems_history(sems, day, &pts, &n, &total, err, sizeof(err))) {
		fprintf(stderr, "%s: %s\n", day, err);
		return 1;
	}
	if (!n) {
		fprintf(stderr, "%s: SEMS+ has no power curve for that day\n", day);
		free(pts);
		return 1;
	}
	for (int i = 1; i < n; i++)
		if (pts[i].power_w > pts[peak].power_w)
			peak = i;
	strftime(a, sizeof(a), "%H:%M", localtime(&pts[0].t));
	strftime(b, sizeof(b), "%H:%M", localtime(&pts[n - 1].t));
	printf("%s: %d statuses %s-%s, %ld Wh (SEMS+: %.1f kWh), peak %ld W\n", day, n, a, b,
	       pts[n - 1].energy_wh, total, pts[peak].power_w);
	if (dry || !*pvo->api_key) {
		free(pts);
		return dry ? 0 : 1;
	}
	ok = pvoutput_add_batch(pvo, pts, n);
	printf("%s: PVOutput accepted %d of %d\n", day, ok, n);
	(void)c;
	free(pts);
	return ok == n ? 0 : 1;
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [options]\n"
		"  -s <section>  inverter section in /etc/config/goodwe-reader\n"
		"                (default: first 'inverter' section)\n"
		"  -1            one cycle and exit\n"
		"  -n            read the inverter and SEMS+, print, send nothing\n"
		"  -u <day>      send day YYYY-MM-DD from SEMS+ to PVOutput (-n: only show)\n"
		"  -v            log to stderr too, -vv adds frame dumps\n"
		"  -C <dir>      UCI config directory (default /etc/config)\n",
		prog);
}

int main(int argc, char **argv)
{
	const char *section = NULL, *confdir = NULL, *fill = NULL;
	struct cfg c = { 0 };
	struct domoticz dz = { 0 };
	struct pvoutput pvo = { 0 };
	struct sems sems_store, *sems = NULL;
	struct state st = { .alarm_since = -1, .last_mode = -1, .sun_yday = -1 };
	struct gw_reading r;
	int once = 0, dry = 0, verbose = 0, ch, rc = 0;
	long t0, slept;

	while ((ch = getopt(argc, argv, "s:1nu:vC:h")) != -1) {
		switch (ch) {
		case 's': section = optarg; break;
		case '1': once = 1; break;
		case 'n': dry = once = 1; break;
		case 'u': fill = optarg; break;
		case 'v': verbose++; break;
		case 'C': confdir = optarg; break;
		default: usage(argv[0]); return ch == 'h' ? 0 : 1;
		}
	}

	openlog("goodwe-reader", LOG_PID | (verbose ? LOG_PERROR : 0), LOG_DAEMON);
	setlogmask(LOG_UPTO(verbose ? LOG_DEBUG : LOG_INFO));
	goodwe_debug = verbose > 1;

	if (load_config(confdir, section, &c, &dz, &pvo))
		return 1;
	if (c.sems && !open_sems(&c, &sems_store))
		sems = &sems_store;

	signal(SIGTERM, on_signal);
	signal(SIGINT, on_signal);
	signal(SIGPIPE, SIG_IGN);

	if (fill) {
		if (!sems) {
			fprintf(stderr, "-u needs SEMS+\n");
			return 1;
		}
		rc = fill_day(&c, &pvo, sems, fill, dry);
		sems_free(sems);
		return rc;
	}

	if (dry) {
		char err[256];

		if (!goodwe_read(c.host, c.port, c.timeout, c.tries, &r, err, sizeof(err))) {
			print_reading(&r);
		} else {
			printf("local: %s\n", err);
			rc = 2;
		}
		if (sems) {
			int s = sems_read(sems, &r, err, sizeof(err));

			if (!s)
				print_reading(&r);
			else
				printf("%s\n", err);
			if (!s)
				rc = 0;
			sems_free(sems);
		}
		return rc;
	}

	syslog(LOG_INFO, "%s: inverter %s:%d every %d s, SEMS+ %s, PVOutput %s", c.name,
	       c.host, c.port, c.interval, sems ? "yes" : "no", *pvo.api_key ? pvo.system_id : "no");

	st.last_ok = st.last_contact = mono();
	while (!stop) {
		int nap = once ? 0 : night_sleep(&c, &dz, &st);

		if (nap) {
			char at[8];
			time_t wake = time(NULL) + nap;

			/* 0 W once when the night starts: an energy device integrates */
			zero_power(&c, &dz, &st, "night");
			strftime(at, sizeof(at), "%H:%M", localtime(&wake));
			syslog(LOG_INFO, "%s: night, not asking the inverter until %s", c.name, at);
			t0 = mono();
			sleep(nap);
			/* silence we chose is no sign of a failure */
			slept = mono() - t0;
			st.last_ok += slept;
			st.last_contact += slept;
			continue;
		}

		t0 = mono();
		rc = read_once(&c, sems, &st, &r);
		if (!rc)
			report_reading(&c, &dz, &pvo, &st, &r);
		if (once)
			break;
		check_state(&c, &dz, &st);
		/* after the reading: the morning value must not wait for SEMS+ */
		daily_check(&c, &pvo, sems, &st);

		slept = mono() - t0;
		if (!stop && c.interval > slept)
			sleep(c.interval - slept);
	}
	if (sems)
		sems_free(sems);
	return rc ? 2 : 0;
}
