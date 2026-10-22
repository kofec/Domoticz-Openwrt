#ifndef SOLARMAN_H
#define SOLARMAN_H

#include <stdint.h>

/* Error codes returned by the solarman_* functions (negative) */
enum {
	SM_OK = 0,
	SM_ERESOLVE = -1,	/* host name lookup failed */
	SM_ECONNECT = -2,	/* logger not reachable (inverter off at night) */
	SM_EIO = -3,		/* send/recv failed or timed out */
	SM_EFRAME = -4,		/* malformed V5 frame or bad checksum */
	SM_ENODATA = -5,	/* logger answered without a Modbus frame */
	SM_EMODBUS = -6,	/* bad Modbus reply (CRC, length, function) */
	SM_EEXCEPT = -7,	/* Modbus exception, e.g. illegal address */
};

int solarman_connect(const char *host, int port, int timeout_s);

/*
 * Read @count holding registers starting at @start (Modbus function 0x03)
 * through a Solarman V5 data logger (LSW-3/LSE) and store them in @out.
 */
int solarman_read(int fd, uint32_t logger_sn, uint8_t slave,
		  uint16_t start, uint16_t count, uint16_t *out);

const char *solarman_strerror(int err);

extern int solarman_debug;

#endif
