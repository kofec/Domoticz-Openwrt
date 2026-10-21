/*
 * GoodWe DT family (XS, DT, DNS) in the LAN: Modbus RTU over UDP 8899, the
 * same protocol as the goodwe Python library. The request is a plain RTU
 * frame to address 0x7F, the reply has an AA 55 header in front of it and
 * its CRC covers the bytes after that header. Checked on a GW3000-XS
 * (ARM 15): addresses 0xF7 and the old AA55 frames are ignored.
 *
 * Only reads (function 03) are sent: there is nothing to set here and a
 * write to a wrong register could reconfigure the inverter.
 */
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>

#include "goodwe.h"

#define GW_ADDR		0x7F
#define GW_START	0x7594		/* 30100 */
#define GW_COUNT	0x49		/* 30100-30172 */

/* Byte offsets in the register block (goodwe/dt.py) */
#define OFF_CLOCK	0		/* year-2000, month, day, h, min, s */
#define OFF_VPV1	6		/* 30103 [0.1 V] */
#define OFF_VGRID1	36		/* 30118 [0.1 V] */
#define OFF_IGRID1	42		/* 30121 [0.1 A] */
#define OFF_FGRID1	48		/* 30124 [0.01 Hz] */
#define OFF_POWER	54		/* 30127-30128, signed 32 bit [W] */
#define OFF_MODE	58		/* 30129 */
#define OFF_TEMP	82		/* 30141, signed [0.1 C] */
#define OFF_TODAY	88		/* 30144 [0.1 kWh] */
#define OFF_TOTAL	90		/* 30145-30146 [0.1 kWh] */

int goodwe_debug;

static uint16_t crc16(const uint8_t *p, size_t n)
{
	uint16_t crc = 0xFFFF;

	while (n--) {
		crc ^= *p++;
		for (int i = 0; i < 8; i++)
			crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
	}
	return crc;
}

static uint16_t be16(const uint8_t *p) { return p[0] << 8 | p[1]; }
static uint32_t be32(const uint8_t *p) { return (uint32_t)be16(p) << 16 | be16(p + 2); }

static int ask(const struct addrinfo *ai, const uint8_t *req, size_t reqlen,
	       int timeout_s, uint8_t *buf, size_t buflen, ssize_t *got)
{
	struct timeval tv = { .tv_sec = timeout_s };
	int fd = socket(ai->ai_family, SOCK_DGRAM | SOCK_CLOEXEC, 0);

	if (fd < 0)
		return -1;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if (sendto(fd, req, reqlen, 0, ai->ai_addr, ai->ai_addrlen) != (ssize_t)reqlen) {
		close(fd);
		return -1;
	}
	do
		*got = recv(fd, buf, buflen, 0);
	while (*got < 0 && errno == EINTR);
	close(fd);
	return *got < 0 ? -1 : 0;
}

/* Data bytes of "AA 55 addr 03 len data.. crc", NULL with err set */
static const uint8_t *check_reply(const uint8_t *b, ssize_t n, char *err, size_t errlen)
{
	uint16_t crc;

	if (n < 7 || b[0] != 0xAA || b[1] != 0x55) {
		snprintf(err, errlen, "reply without AA55 header (%zd B)", n);
		return NULL;
	}
	if (b[2] != GW_ADDR || b[3] != 0x03) {
		snprintf(err, errlen, "reply from address 0x%02x, function 0x%02x", b[2], b[3]);
		return NULL;
	}
	if (b[4] != 2 * GW_COUNT || n < 5 + 2 * GW_COUNT + 2) {
		snprintf(err, errlen, "reply has %u data bytes instead of %u", b[4], 2 * GW_COUNT);
		return NULL;
	}
	crc = b[5 + 2 * GW_COUNT] | b[6 + 2 * GW_COUNT] << 8;
	if (crc != crc16(b + 2, 3 + 2 * GW_COUNT)) {
		snprintf(err, errlen, "bad CRC in reply");
		return NULL;
	}
	return b + 5;
}

int goodwe_read(const char *host, int port, int timeout_s, int tries,
		struct gw_reading *r, char *err, size_t errlen)
{
	struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_DGRAM };
	struct addrinfo *res;
	uint8_t req[8], buf[512];
	const uint8_t *d = NULL;
	char service[12];
	ssize_t got = 0;
	uint16_t crc;

	snprintf(service, sizeof(service), "%d", port);
	if (getaddrinfo(host, service, &hints, &res)) {
		snprintf(err, errlen, "cannot resolve %s", host);
		return -1;
	}
	req[0] = GW_ADDR;
	req[1] = 0x03;
	req[2] = GW_START >> 8;
	req[3] = GW_START & 0xFF;
	req[4] = GW_COUNT >> 8;
	req[5] = GW_COUNT & 0xFF;
	crc = crc16(req, 6);
	req[6] = crc;		/* Modbus CRC is sent low byte first */
	req[7] = crc >> 8;

	snprintf(err, errlen, "%s:%d does not answer (%d x %d s)", host, port, tries, timeout_s);
	for (int i = 0; i < tries && !d; i++) {
		if (ask(res, req, sizeof(req), timeout_s, buf, sizeof(buf), &got)) {
			if (errno != EAGAIN && errno != EWOULDBLOCK)
				snprintf(err, errlen, "%s:%d: %s", host, port, strerror(errno));
			continue;
		}
		if (goodwe_debug)
			syslog(LOG_DEBUG, "reply %zd B: %02x %02x %02x %02x %02x ...", got,
			       buf[0], buf[1], buf[2], buf[3], buf[4]);
		d = check_reply(buf, got, err, errlen);
	}
	freeaddrinfo(res);
	if (!d)
		return -1;

	memset(r, 0, sizeof(*r));
	r->t = time(NULL);	/* the inverter clock is set by the cloud, may drift */
	r->power_w = (int32_t)be32(d + OFF_POWER);
	r->today_wh = be16(d + OFF_TODAY) * 100L;
	r->total_wh = be32(d + OFF_TOTAL) * 100L;
	r->voltage_dv = be16(d + OFF_VGRID1);
	r->current_da = be16(d + OFF_IGRID1);
	r->freq_ch = be16(d + OFF_FGRID1);
	r->pv1_dv = be16(d + OFF_VPV1);
	r->temp_dc = (int16_t)be16(d + OFF_TEMP);
	r->mode = be16(d + OFF_MODE);
	snprintf(r->clock, sizeof(r->clock), "%02u:%02u:%02u",
		 d[OFF_CLOCK + 3] % 100u, d[OFF_CLOCK + 4] % 100u, d[OFF_CLOCK + 5] % 100u);
	r->source = "local";
	return 0;
}

const char *goodwe_mode(int mode)
{
	switch (mode) {
	case 0: return "Wait";
	case 1: return "Normal";
	case 2: return "Error";
	case 4: return "Check";
	}
	return "unknown";
}
