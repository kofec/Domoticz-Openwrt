/*
 * sofar-reader - read Sofar K-TLX inverter data through a Solarman LSW-3/LSE
 * data logger and push it to Domoticz.
 *
 * Configuration: /etc/config/sofar-reader, one process per inverter section.
 */
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <uci.h>

#include "domoticz.h"
#include "sofar.h"
#include "solarman.h"

struct inverter {
	char name[32];
	char host[64];
	int port;
	uint32_t serial;
	int slave;
	int interval;
	int timeout;
	int read_strings;
	int zero_offline;
	int idx[64];		/* per sofar_items[], 0 = not sent */
	int idx_energy;		/* Electric (Instant+Counter): W;Wh */
	int idx_status;		/* Text: status and active faults */
};

static volatile sig_atomic_t stop;

static void on_signal(int sig)
{
	(void)sig;
	stop = 1;
}

static const char *opt_str(struct uci_context *ctx, struct uci_section *s,
			   const char *name, const char *def)
{
	const char *v = s ? uci_lookup_option_string(ctx, s, name) : NULL;

	return v && *v ? v : def;
}

static long opt_num(struct uci_context *ctx, struct uci_section *s,
		    const char *name, long def)
{
	const char *v = opt_str(ctx, s, name, NULL);

	return v ? strtol(v, NULL, 0) : def;
}

static struct uci_section *find_section(struct uci_package *pkg,
					const char *type, const char *name)
{
	struct uci_element *e;

	uci_foreach_element(&pkg->sections, e) {
		struct uci_section *s = uci_to_section(e);

		if (strcmp(s->type, type))
			continue;
		if (!name || !strcmp(e->name, name))
			return s;
	}
	return NULL;
}

static int load_config(const char *confdir, const char *section,
		       struct inverter *inv, struct domoticz *dz)
{
	struct uci_context *ctx = uci_alloc_context();
	struct uci_package *pkg = NULL;
	struct uci_section *s;
	char opt[48];
	int ret = -1;

	if (!ctx)
		return -1;
	if (confdir)
		uci_set_confdir(ctx, confdir);
	if (uci_load(ctx, "sofar-reader", &pkg)) {
		fprintf(stderr, "cannot load config sofar-reader\n");
		goto out;
	}

	s = find_section(pkg, "domoticz", NULL);
	snprintf(dz->host, sizeof(dz->host), "%s", opt_str(ctx, s, "host", "127.0.0.1"));
	dz->port = opt_num(ctx, s, "port", 8080);
	dz->timeout = opt_num(ctx, s, "timeout", 5);
	domoticz_set_auth(dz, opt_str(ctx, s, "username", NULL),
			  opt_str(ctx, s, "password", NULL));

	s = find_section(pkg, "inverter", section);
	if (!s) {
		fprintf(stderr, "no inverter section %s\n", section ? section : "");
		goto out;
	}
	snprintf(inv->name, sizeof(inv->name), "%s", s->e.name);
	snprintf(inv->host, sizeof(inv->host), "%s", opt_str(ctx, s, "host", ""));
	inv->port = opt_num(ctx, s, "port", 8899);
	inv->serial = strtoul(opt_str(ctx, s, "serial", "0"), NULL, 10);
	inv->slave = opt_num(ctx, s, "slave", 1);
	inv->interval = opt_num(ctx, s, "interval", 60);
	inv->timeout = opt_num(ctx, s, "timeout", 10);
	inv->read_strings = opt_num(ctx, s, "read_strings", 0);
	inv->zero_offline = opt_num(ctx, s, "zero_offline", 1);
	inv->idx_energy = opt_num(ctx, s, "idx_energy", 0);
	inv->idx_status = opt_num(ctx, s, "idx_status", 0);
	for (size_t i = 0; i < sofar_nitems; i++) {
		snprintf(opt, sizeof(opt), "idx_%s", sofar_items[i].key);
		inv->idx[i] = opt_num(ctx, s, opt, 0);
	}

	if (!*inv->host || !inv->serial) {
		fprintf(stderr, "%s: host and serial (logger S/N) are required\n", inv->name);
		goto out;
	}
	if (inv->interval < 10)
		inv->interval = 10;
	ret = 0;
out:
	uci_free_context(ctx);
	return ret;
}

static int read_inverter(const struct inverter *inv, struct sofar_data *d)
{
	int fd, rc;

	memset(d, 0, sizeof(*d));
	fd = solarman_connect(inv->host, inv->port, inv->timeout);
	if (fd < 0)
		return fd;

	rc = solarman_read(fd, inv->serial, inv->slave, SOFAR_MAIN_START,
			   SOFAR_MAIN_COUNT, d->main);
	d->have_main = rc == SM_OK;

	if (rc == SM_OK && inv->read_strings) {
		int rs = solarman_read(fd, inv->serial, inv->slave, SOFAR_STRING_START,
				       SOFAR_STRING_COUNT, d->string);

		d->have_string = rs == SM_OK;
		if (rs)
			syslog(LOG_DEBUG, "%s: string registers: %s", inv->name,
			       solarman_strerror(rs));
	}
	close(fd);
	return rc;
}

static void print_json(const struct sofar_data *d)
{
	const struct sofar_item *it;
	char num[24], text[512];
	long v;
	int dec;

	sofar_status_text(d, text, sizeof(text));
	printf("{\n\t\"status_text\": \"%s\"", text);
	for (size_t i = 0; i < sofar_nitems; i++) {
		it = &sofar_items[i];
		if (sofar_value(d, it, &v, &dec))
			continue;
		if (it->type == T_ENUM && v >= 0 && v < it->nnames)
			printf(",\n\t\"%s\": \"%s\"", it->key, it->names[v]);
		else
			printf(",\n\t\"%s\": %s", it->key, sofar_format(num, sizeof(num), v, dec));
	}
	printf("\n}\n");
}

static long get(const struct sofar_data *d, const char *key, int *ok)
{
	const struct sofar_item *it = sofar_find(key);
	long v;
	int dec;

	*ok = it && !sofar_value(d, it, &v, &dec);
	return *ok ? v : 0;
}

static void push(const struct inverter *inv, const struct domoticz *dz,
		 const struct sofar_data *d)
{
	static char last_status[512];
	char text[512], num[24];
	long v, power, total;
	int dec, ok1, ok2;

	for (size_t i = 0; i < sofar_nitems; i++) {
		const struct sofar_item *it = &sofar_items[i];

		/* status and faults go to the text device (idx_status) */
		if (it->type == T_ENUM || it->type == T_BITS)
			continue;
		if (!inv->idx[i] || sofar_value(d, it, &v, &dec))
			continue;
		domoticz_udevice(dz, inv->idx[i], 0, sofar_format(num, sizeof(num), v, dec));
	}

	if (inv->idx_energy) {
		power = get(d, "output_power", &ok1);
		total = get(d, "total_energy", &ok2);
		if (ok1 && ok2) {
			snprintf(text, sizeof(text), "%ld;%ld", power, total * 1000);
			domoticz_udevice(dz, inv->idx_energy, 0, text);
		}
	}

	/* Text devices keep a log entry per update, send only changes */
	if (inv->idx_status) {
		sofar_status_text(d, text, sizeof(text));
		if (*text && strcmp(text, last_status) &&
		    !domoticz_udevice(dz, inv->idx_status, 0, text))
			snprintf(last_status, sizeof(last_status), "%s", text);
	}
}

/* Logger is powered by the inverter: offline at night, show 0 W then */
static void push_offline(const struct inverter *inv, const struct domoticz *dz,
			 long last_total)
{
	char text[48];

	for (size_t i = 0; i < sofar_nitems; i++)
		if (inv->idx[i] && !strcmp(sofar_items[i].unit, "W"))
			domoticz_udevice(dz, inv->idx[i], 0, "0");

	if (inv->idx_energy && last_total >= 0) {
		snprintf(text, sizeof(text), "0;%ld", last_total * 1000);
		domoticz_udevice(dz, inv->idx_energy, 0, text);
	}
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [options]\n"
		"  -s <section>  inverter section in /etc/config/sofar-reader\n"
		"                (default: first 'inverter' section)\n"
		"  -1            read once and exit\n"
		"  -n            do not send to Domoticz, print JSON (implies -1)\n"
		"  -v            log to stderr too, -vv adds frame dumps\n"
		"  -C <dir>      UCI config directory (default /etc/config)\n",
		prog);
}

int main(int argc, char **argv)
{
	const char *section = NULL, *confdir = NULL;
	struct inverter inv = { 0 };
	struct domoticz dz = { 0 };
	struct sofar_data d;
	struct timespec t0, t1;
	int once = 0, dry = 0, verbose = 0, online = -1;
	long last_total = -1;
	int c, rc, ok;

	while ((c = getopt(argc, argv, "s:1nvC:h")) != -1) {
		switch (c) {
		case 's': section = optarg; break;
		case '1': once = 1; break;
		case 'n': dry = once = 1; break;
		case 'v': verbose++; break;
		case 'C': confdir = optarg; break;
		default: usage(argv[0]); return c == 'h' ? 0 : 1;
		}
	}

	openlog("sofar-reader", LOG_PID | (verbose ? LOG_PERROR : 0), LOG_DAEMON);
	setlogmask(LOG_UPTO(verbose ? LOG_DEBUG : LOG_INFO));
	solarman_debug = verbose > 1;

	if (sofar_nitems > sizeof(inv.idx) / sizeof(inv.idx[0]))
		return 1;
	if (load_config(confdir, section, &inv, &dz))
		return 1;

	signal(SIGTERM, on_signal);
	signal(SIGINT, on_signal);
	signal(SIGPIPE, SIG_IGN);

	syslog(LOG_INFO, "%s: logger %s:%d S/N %u, every %d s", inv.name,
	       inv.host, inv.port, (unsigned)inv.serial, inv.interval);

	while (!stop) {
		clock_gettime(CLOCK_MONOTONIC, &t0);

		rc = read_inverter(&inv, &d);
		if (rc == SM_OK) {
			if (online != 1)
				syslog(LOG_INFO, "%s: inverter online", inv.name);
			online = 1;
			last_total = get(&d, "total_energy", &ok);
			if (dry)
				print_json(&d);
			else
				push(&inv, &dz, &d);
		} else {
			if (online != 0)
				syslog(LOG_INFO, "%s: inverter offline: %s", inv.name,
				       solarman_strerror(rc));
			else
				syslog(LOG_DEBUG, "%s: %s", inv.name, solarman_strerror(rc));
			if (online == 1 && inv.zero_offline && !dry)
				push_offline(&inv, &dz, last_total);
			online = 0;
		}

		if (once)
			return rc == SM_OK ? 0 : 2;

		/* sleep() returns early on SIGTERM */
		clock_gettime(CLOCK_MONOTONIC, &t1);
		if (!stop && inv.interval > t1.tv_sec - t0.tv_sec)
			sleep(inv.interval - (t1.tv_sec - t0.tv_sec));
	}
	return 0;
}
