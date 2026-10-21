#ifndef DOMOTICZ_H
#define DOMOTICZ_H

struct domoticz {
	char host[64];
	int port;
	char auth[128];		/* base64 "user:password", empty = no auth */
	int timeout;
};

void domoticz_set_auth(struct domoticz *dz, const char *user, const char *pass);

/* /json.htm?type=command&param=udevice&idx=..&nvalue=..&svalue=.. */
int domoticz_udevice(const struct domoticz *dz, int idx, int nvalue,
		     const char *svalue);

#endif
