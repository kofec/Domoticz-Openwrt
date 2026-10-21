#ifndef PVOUTPUT_H
#define PVOUTPUT_H

#include <time.h>

struct pvoutput {
	char system_id[16];
	char api_key[64];	/* empty = upload disabled */
	int interval;		/* seconds between uploads */
	int timeout;		/* per request, whole transfer */
};

/* One status: v2 power [W], v1 energy today [Wh], v6 voltage [0.1 V] */
struct pvo_point {
	time_t t;		/* router local time is used for d and t */
	long power_w;
	long energy_wh;		/* < 0 = not sent */
	long voltage_dv;	/* <= 0 = not sent */
};

/* What PVOutput has for one day, per minute of the day */
struct pvo_day {
	unsigned char have[1440];
	long energy_wh[1440];	/* -1 when the status has no energy (NaN) */
	int count;
};

/* POST addstatus.jsp. With dry set, only log what would be sent. */
int pvoutput_add_status(const struct pvoutput *p, const struct pvo_point *pt, int dry);

/*
 * POST addbatchstatus.jsp, 30 statuses per request. Returns the number
 * PVOutput accepted, or -1 when a request failed.
 */
int pvoutput_add_batch(const struct pvoutput *p, const struct pvo_point *pts, int n);

/* GET getstatus.jsp?h=1 for day "YYYYMMDD"; a day without data is no error */
int pvoutput_get_day(const struct pvoutput *p, const char *ymd, struct pvo_day *day);

#endif
