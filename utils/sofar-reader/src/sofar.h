#ifndef SOFAR_H
#define SOFAR_H

#include <stddef.h>
#include <stdint.h>

/* Register blocks of Sofar K-TLX (same as config.cfg of Sofar_LSW3) */
#define SOFAR_MAIN_START	0x0000
#define SOFAR_MAIN_COUNT	0x28	/* 0x0000 - 0x0027 */
#define SOFAR_STRING_START	0x0105
#define SOFAR_STRING_COUNT	0x10	/* 0x0105 - 0x0114 */

enum sofar_type {
	T_U16,		/* unsigned 16 bit */
	T_S16,		/* signed 16 bit (two's complement) */
	T_U32,		/* reg = high word, reg + 1 = low word */
	T_ENUM,		/* index into names[] */
	T_BITS,		/* fault bit mask, names[] per bit */
};

struct sofar_item {
	const char *key;	/* JSON name and UCI option suffix idx_<key> */
	uint16_t reg;
	uint8_t type;
	int8_t exp;		/* value = raw * 10^exp */
	const char *unit;
	const char *const *names;
	uint8_t nnames;
};

struct sofar_data {
	uint16_t main[SOFAR_MAIN_COUNT];
	uint16_t string[SOFAR_STRING_COUNT];
	int have_main;
	int have_string;
};

extern const struct sofar_item sofar_items[];
extern const size_t sofar_nitems;

/* Scaled value as integer with @dec decimal places; -1 if not read */
int sofar_value(const struct sofar_data *d, const struct sofar_item *it,
		long *val, int *dec);

/* Format a scaled value ("12.34") */
char *sofar_format(char *buf, size_t len, long val, int dec);

/* Status and active faults as text, e.g. "Normal" or "FAULT: ID01 ..." */
void sofar_status_text(const struct sofar_data *d, char *buf, size_t len);

const struct sofar_item *sofar_find(const char *key);

#endif
