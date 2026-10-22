/*
 * UCI helpers and the "name = value" file reader shared by both readers.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "domoticz.h"
#include "pvoutput.h"
#include "uciutil.h"

const char *opt_str(struct uci_context *ctx, struct uci_section *s,
		    const char *name, const char *def)
{
	const char *v = s ? uci_lookup_option_string(ctx, s, name) : NULL;

	return v && *v ? v : def;
}

long opt_num(struct uci_context *ctx, struct uci_section *s,
	     const char *name, long def)
{
	const char *v = opt_str(ctx, s, name, NULL);

	return v ? strtol(v, NULL, 0) : def;
}

struct uci_section *find_section(struct uci_package *pkg,
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

static char *trim(char *s)
{
	char *e;

	while (isspace((unsigned char)*s))
		s++;
	e = s + strlen(s);
	while (e > s && isspace((unsigned char)e[-1]))
		*--e = 0;
	if (e - s >= 2 && (*s == '"' || *s == '\'') && e[-1] == *s) {
		e[-1] = 0;
		s++;
	}
	return s;
}

int file_value(const char *file, const char *name, char *buf, size_t len)
{
	char line[512], *s, *eq;
	size_t nlen = name ? strlen(name) : 0;
	FILE *f = fopen(file, "r");
	int ret = -1;

	if (!f)
		return -1;
	while (fgets(line, sizeof(line), f)) {
		s = trim(line);
		if (!*s || *s == '#' || *s == ';' || *s == '[')
			continue;
		if (nlen) {
			if (strncmp(s, name, nlen))
				continue;
			eq = trim(s + nlen);
			if (*eq != '=')
				continue;
			s = trim(eq + 1);
		}
		if (*s && strlen(s) < len) {
			strcpy(buf, s);
			ret = 0;
		}
		break;
	}
	fclose(f);
	return ret;
}

void load_domoticz(struct uci_context *ctx, struct uci_package *pkg,
		   struct domoticz *dz)
{
	struct uci_section *s = find_section(pkg, "domoticz", NULL);

	snprintf(dz->host, sizeof(dz->host), "%s", opt_str(ctx, s, "host", "127.0.0.1"));
	dz->port = opt_num(ctx, s, "port", 8080);
	dz->timeout = opt_num(ctx, s, "timeout", 5);
	domoticz_set_auth(dz, opt_str(ctx, s, "username", NULL),
			  opt_str(ctx, s, "password", NULL));
}

void load_pvoutput(struct uci_context *ctx, struct uci_section *s,
		   const char *name, struct pvoutput *pvo)
{
	const char *file = opt_str(ctx, s, "pvo_api_key_file", NULL);

	snprintf(pvo->system_id, sizeof(pvo->system_id), "%s",
		 opt_str(ctx, s, "pvo_system_id", ""));
	snprintf(pvo->api_key, sizeof(pvo->api_key), "%s",
		 opt_str(ctx, s, "pvo_api_key", ""));
	pvo->interval = opt_num(ctx, s, "pvo_interval", 300);
	pvo->timeout = 20;

	if (!*pvo->system_id) {
		pvo->api_key[0] = 0;
		return;
	}
	if (!*pvo->api_key && file &&
	    file_value(file, opt_str(ctx, s, "pvo_api_key_name", NULL),
		       pvo->api_key, sizeof(pvo->api_key)))
		syslog(LOG_WARNING, "%s: no PVOutput API key in %s", name, file);
	if (!*pvo->api_key)
		syslog(LOG_WARNING, "%s: PVOutput upload disabled, no API key", name);
}
