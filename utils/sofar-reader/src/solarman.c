/*
 * Solarman V5 protocol (LSW-3/LSE data loggers, TCP port 8899).
 *
 * Request: A5 | len(2) | 0x4510(2) | seq(2) | logger S/N(4)
 *          | 02 + 14 zero bytes | Modbus RTU frame | checksum | 15
 * Reply:   same header with control code 0x1510, the payload starts with
 *          frame type, status and three 32-bit timers (14 bytes) followed
 *          by the Modbus RTU reply. All V5 fields are little endian,
 *          Modbus fields big endian.
 */
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>

#include "solarman.h"

#define V5_START	0xA5
#define V5_END		0x15
#define V5_REQUEST	0x4510
#define V5_RESPONSE	0x1510
#define V5_HDR_LEN	11
#define V5_PAYLOAD_HDR	14

int solarman_debug;

static uint16_t seq;

static uint16_t modbus_crc(const uint8_t *p, size_t n)
{
	uint16_t crc = 0xFFFF;

	while (n--) {
		crc ^= *p++;
		for (int i = 0; i < 8; i++)
			crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
	}
	return crc;
}

static uint8_t v5_checksum(const uint8_t *p, size_t n)
{
	uint8_t sum = 0;

	while (n--)
		sum += *p++;
	return sum;
}

static void put_le16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static void put_be16(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = v; }
static uint16_t get_le16(const uint8_t *p) { return p[0] | p[1] << 8; }
static uint16_t get_be16(const uint8_t *p) { return p[0] << 8 | p[1]; }

static void dump(const char *what, const uint8_t *p, size_t n)
{
	char line[3 * 64 + 1];
	size_t i, o = 0;

	if (!solarman_debug)
		return;
	for (i = 0; i < n && o + 4 < sizeof(line); i++)
		o += snprintf(line + o, sizeof(line) - o, "%02x ", p[i]);
	syslog(LOG_DEBUG, "%s (%zu B): %s%s", what, n, line, i < n ? "..." : "");
}

int solarman_connect(const char *host, int port, int timeout_s)
{
	struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM };
	struct addrinfo *res, *ai;
	struct timeval tv = { .tv_sec = timeout_s };
	char service[12];	/* any int from UCI, not just 1-65535 */
	int fd = -1;

	snprintf(service, sizeof(service), "%d", port);
	if (getaddrinfo(host, service, &hints, &res))
		return SM_ERESOLVE;

	for (ai = res; ai; ai = ai->ai_next) {
		fd = socket(ai->ai_family, ai->ai_socktype | SOCK_CLOEXEC, ai->ai_protocol);
		if (fd < 0)
			continue;
		/* On Linux SO_SNDTIMEO also bounds connect() */
		setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
		setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
			break;
		close(fd);
		fd = -1;
	}
	freeaddrinfo(res);

	return fd < 0 ? SM_ECONNECT : fd;
}

static int recv_all(int fd, uint8_t *buf, size_t len)
{
	while (len) {
		ssize_t n = recv(fd, buf, len, 0);

		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return SM_EIO;
		buf += n;
		len -= n;
	}
	return SM_OK;
}

int solarman_read(int fd, uint32_t logger_sn, uint8_t slave,
		  uint16_t start, uint16_t count, uint16_t *out)
{
	uint8_t req[V5_HDR_LEN + V5_PAYLOAD_HDR + 1 + 8 + 2];
	uint8_t hdr[V5_HDR_LEN], buf[512];
	const uint8_t *mb;
	size_t len, mlen;
	uint16_t crc;
	int tries;

	if (count == 0 || count > 125)
		return SM_EMODBUS;

	/* V5 header */
	req[0] = V5_START;
	put_le16(req + 1, V5_PAYLOAD_HDR + 1 + 8);
	put_le16(req + 3, V5_REQUEST);
	put_le16(req + 5, ++seq);
	put_le16(req + 7, logger_sn);
	put_le16(req + 9, logger_sn >> 16);
	/* payload: frame type 2 (inverter data), sensor type and timers zero */
	memset(req + 11, 0, V5_PAYLOAD_HDR + 1);
	req[11] = 0x02;
	/* Modbus RTU read holding registers */
	mb = req + 26;
	req[26] = slave;
	req[27] = 0x03;
	put_be16(req + 28, start);
	put_be16(req + 30, count);
	crc = modbus_crc(mb, 6);
	req[32] = crc;		/* Modbus CRC is sent low byte first */
	req[33] = crc >> 8;
	req[34] = v5_checksum(req + 1, sizeof(req) - 3);
	req[35] = V5_END;

	dump("request", req, sizeof(req));
	if (send(fd, req, sizeof(req), MSG_NOSIGNAL) != (ssize_t)sizeof(req))
		return SM_EIO;

	/* The logger may interleave other frames (heartbeat), skip them */
	for (tries = 0; tries < 4; tries++) {
		if (recv_all(fd, hdr, sizeof(hdr)))
			return SM_EIO;
		if (hdr[0] != V5_START)
			return SM_EFRAME;
		len = get_le16(hdr + 1);
		if (len + 2 > sizeof(buf))
			return SM_EFRAME;
		if (recv_all(fd, buf, len + 2))
			return SM_EIO;
		dump("reply", buf, len + 2);
		if (buf[len + 1] != V5_END ||
		    (uint8_t)(v5_checksum(hdr + 1, sizeof(hdr) - 1) +
			      v5_checksum(buf, len)) != buf[len])
			return SM_EFRAME;
		if (get_le16(hdr + 3) == V5_RESPONSE)
			break;
	}
	if (tries == 4)
		return SM_EFRAME;

	/* A reply without a Modbus frame means the inverter did not answer */
	if (len < V5_PAYLOAD_HDR + 5)
		return SM_ENODATA;
	mb = buf + V5_PAYLOAD_HDR;
	mlen = len - V5_PAYLOAD_HDR;

	if (mb[0] != slave)
		return SM_EMODBUS;
	if (mb[1] == (0x03 | 0x80))
		return SM_EEXCEPT;
	/* Some loggers append two padding bytes after the CRC, so use >= */
	if (mb[1] != 0x03 || mb[2] != count * 2 || mlen < 3u + mb[2] + 2)
		return SM_EMODBUS;
	crc = modbus_crc(mb, 3 + mb[2]);
	if (get_le16(mb + 3 + mb[2]) != crc)
		return SM_EMODBUS;

	for (int i = 0; i < count; i++)
		out[i] = get_be16(mb + 3 + 2 * i);

	return SM_OK;
}

const char *solarman_strerror(int err)
{
	switch (err) {
	case SM_OK:		return "ok";
	case SM_ERESOLVE:	return "cannot resolve logger address";
	case SM_ECONNECT:	return "cannot connect to logger";
	case SM_EIO:		return "no reply from logger";
	case SM_EFRAME:		return "malformed Solarman V5 frame";
	case SM_ENODATA:	return "logger returned no inverter data";
	case SM_EMODBUS:	return "invalid Modbus reply";
	case SM_EEXCEPT:	return "Modbus exception (register range not supported)";
	}
	return "unknown error";
}
