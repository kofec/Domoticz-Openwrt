#ifndef GOODWE_H
#define GOODWE_H

#include <time.h>

/* One reading, from the inverter in the LAN or from SEMS+ */
struct gw_reading {
	time_t t;		/* router clock (LAN) or SEMS+ refreshTime */
	long power_w;
	long today_wh;
	long total_wh;		/* -1 = unknown */
	long voltage_dv;	/* grid voltage [0.1 V] */
	long current_da;	/* grid current [0.1 A], -1 = unknown */
	long freq_ch;		/* grid frequency [0.01 Hz], -1 = unknown */
	long pv1_dv;		/* PV1 voltage [0.1 V], -1 = unknown */
	long temp_dc;		/* inverter temperature [0.1 C], LONG_MIN = unknown */
	int mode;		/* 0 Wait, 1 Normal, 2 Error, 4 Check, -1 unknown */
	char clock[12];		/* inverter clock "HH:MM:SS", LAN only */
	const char *source;	/* "local" or "SEMS+" */
};

/*
 * Modbus RTU over UDP 8899 (GoodWe DT family, e.g. GW3000-XS): read the
 * running data block 30100-30172 from address 0x7F. Only function 03 is
 * ever sent. Retries tries times with timeout_s each, UDP loses packets.
 * Returns 0, or -1 with err set.
 */
int goodwe_read(const char *host, int port, int timeout_s, int tries,
		struct gw_reading *r, char *err, size_t errlen);

const char *goodwe_mode(int mode);

extern int goodwe_debug;

#endif
