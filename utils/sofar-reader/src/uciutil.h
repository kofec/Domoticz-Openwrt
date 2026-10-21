#ifndef UCIUTIL_H
#define UCIUTIL_H

#include <uci.h>

/* Option value, def when unset or empty */
const char *opt_str(struct uci_context *ctx, struct uci_section *s,
		    const char *name, const char *def);
long opt_num(struct uci_context *ctx, struct uci_section *s,
	     const char *name, long def);

/* Section of the given type, by name or the first one when name is NULL */
struct uci_section *find_section(struct uci_package *pkg,
				 const char *type, const char *name);

/*
 * Value of "name = value" from a plain file such as /root/lib/secrets.conf
 * or the [Defaults] section of gw2pvo.cfg. Without a name, the first line
 * that is not a comment or a [section]. Quotes around the value are removed.
 */
int file_value(const char *file, const char *name, char *buf, size_t len);

struct domoticz;
struct pvoutput;

/* The "domoticz" section: host, port, timeout, username, password */
void load_domoticz(struct uci_context *ctx, struct uci_package *pkg,
		   struct domoticz *dz);

/*
 * pvo_system_id, pvo_interval and the key: pvo_api_key, or pvo_api_key_name
 * read from pvo_api_key_file. No system id or no key = upload disabled.
 */
void load_pvoutput(struct uci_context *ctx, struct uci_section *s,
		   const char *name, struct pvoutput *pvo);

#endif
