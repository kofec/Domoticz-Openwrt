/*
 * Register map of Sofar K-TLX inverters, taken from SOFARMap.xml of
 * https://github.com/kofec/Sofar_LSW3
 */
#include <stdio.h>
#include <string.h>

#include "sofar.h"

#define N(a) (sizeof(a) / sizeof((a)[0]))

static const char *const status_names[] = {
	"Stand-by", "Self-checking", "Normal", "FAULT", "Permanent",
};

static const char *const country_names[] = {
	"Germany", "CEI0-21 Internal", "Australia", "Spain RD1699", "Turkey",
	"Denmark", "Greece", "Netherland", "Belgium", "UK-G59", "China",
	"France", "Poland", "Germany BDEW", "Germany VDE0126", "Italy CEI0-16",
	"UK-G83", "Greece Islands", "EU EN50438", "EU EN61727", "Korea",
	"Sweden", "Europe General", "CEI0-21 External", "Cyprus", "India",
	"Philippines", "New Zeland",
};

static const char *const fault1_names[] = {
	"ID01 Grid Over Voltage Protection",
	"ID02 Grid Under Voltage Protection",
	"ID03 Grid Over Frequency Protection",
	"ID04 Grid Under Frequency Protection",
	"ID05 PV Under Voltage Protection",
	"ID06 Grid Low Voltage Ride through",
	"ID07",
	"ID08",
	"ID09 PV Over Voltage Protection",
	"ID10 PV Input Current Unbalanced",
	"ID11 PV Input Mode wrong configuration",
	"ID12 Ground-Fault circuit interrupters fault",
	"ID13 Phase sequence fault",
	"ID14 Hardware boost over current protection",
	"ID15 Hardware AC over current protection",
	"ID16 Grid current is too high",
};

static const char *const fault2_names[] = {
	"ID17 Grid current sampling error",
	"ID18 DCI sampling error",
	"ID19 Grid voltage sampling error",
	"ID20 GFCI device sampling error",
	"ID21 Main chip fault",
	"ID22 Hardware auxiliary power fault",
	"ID23 Bus voltage zero fault",
	"ID24 Output current not balanced",
	"ID25 Bus under voltage protection",
	"ID26 Bus over voltage protection",
	"ID27 Bus voltage unbalanced",
	"ID28 DCI is too high",
	"ID29 Grid current is too high",
	"ID30 Input current is too high",
	"ID31",
	"ID32",
};

static const char *const fault3_names[] = {
	"ID33", "ID34", "ID35", "ID36", "ID37", "ID38", "ID39", "ID40",
	"ID41", "ID42", "ID43", "ID44", "ID45", "ID46", "ID47", "ID48",
};

static const char *const fault4_names[] = {
	"ID49 Grid voltage sampling differs between master and slave DSP",
	"ID50 Grid frequency sampling differs between master and slave DSP",
	"ID51 DCI sampling differs between master and slave DSP",
	"ID52 GFCI sampling differs between master and slave DSP",
	"ID53 Communication failure between master and slave DSP",
	"ID54 Communication failure between slave and communication board",
	"ID55 Relay fault",
	"ID56 Insulation resistance between PV array and earth too low",
	"ID57 Inverter temp is too high",
	"ID58 Boost temp is too high",
	"ID59 Environment temp is too high",
	"ID60 Inverter not connected to PE",
	"ID61", "ID62", "ID63", "ID64",
};

static const char *const fault5_names[] = {
	"ID65 Grid current too high, unrecoverable",
	"ID66 Bus voltage too high, unrecoverable",
	"ID67 Grid current unbalanced, unrecoverable",
	"ID68 Input current unbalanced, unrecoverable",
	"ID69 Bus voltage unbalanced, unrecoverable",
	"ID70 Grid current too high, unrecoverable",
	"ID71 PV Input Mode configuration wrong, unrecoverable",
	"ID72",
	"ID73",
	"ID74 Input current too high, unrecoverable",
	"ID75 Error reading from EEPROM",
	"ID76 Error writing to EEPROM",
	"ID77 Relay failure, unrecoverable",
	"ID78", "ID79", "ID80",
};

#define ENUM(k, r, n)	{ k, r, T_ENUM, 0, "", n, N(n) }
#define BITS(k, r, n)	{ k, r, T_BITS, 0, "", n, N(n) }
#define VAL(k, r, t, e, u)	{ k, r, t, e, u, NULL, 0 }

const struct sofar_item sofar_items[] = {
	ENUM("status",		0x0000, status_names),
	BITS("fault1",		0x0001, fault1_names),
	BITS("fault2",		0x0002, fault2_names),
	BITS("fault3",		0x0003, fault3_names),
	BITS("fault4",		0x0004, fault4_names),
	BITS("fault5",		0x0005, fault5_names),
	VAL("pv1_voltage",	0x0006, T_U16, -1, "V"),
	VAL("pv1_current",	0x0007, T_U16, -2, "A"),
	VAL("pv2_voltage",	0x0008, T_U16, -1, "V"),
	VAL("pv2_current",	0x0009, T_U16, -2, "A"),
	VAL("pv1_power",		0x000A, T_U16, 1, "W"),
	VAL("pv2_power",		0x000B, T_U16, 1, "W"),
	VAL("output_power",	0x000C, T_S16, 1, "W"),
	VAL("reactive_power",	0x000D, T_S16, -2, "kVar"),
	VAL("grid_frequency",	0x000E, T_U16, -2, "Hz"),
	VAL("l1_voltage",		0x000F, T_U16, -1, "V"),
	VAL("l1_current",		0x0010, T_U16, -2, "A"),
	VAL("l2_voltage",		0x0011, T_U16, -1, "V"),
	VAL("l2_current",		0x0012, T_U16, -2, "A"),
	VAL("l3_voltage",		0x0013, T_U16, -1, "V"),
	VAL("l3_current",		0x0014, T_U16, -2, "A"),
	VAL("total_energy",	0x0015, T_U32, 0, "kWh"),
	VAL("total_time",		0x0017, T_U32, 0, "h"),
	VAL("today_energy",	0x0019, T_U16, 1, "Wh"),
	VAL("today_time",		0x001A, T_U16, 0, "min"),
	VAL("module_temp",	0x001B, T_S16, 0, "C"),
	VAL("inner_temp",		0x001C, T_S16, 0, "C"),
	VAL("bus_voltage",	0x001D, T_U16, -1, "V"),
	VAL("pv1_voltage_slave",	0x001E, T_U16, -1, "V"),
	VAL("pv1_current_slave",	0x001F, T_U16, -1, "A"),
	VAL("countdown",		0x0020, T_U16, 0, "s"),
	VAL("alert",		0x0021, T_U16, 0, ""),
	VAL("input_mode",		0x0022, T_U16, 0, ""),
	VAL("board_message",	0x0023, T_U16, 0, ""),
	VAL("insulation_pv1",	0x0024, T_U16, 0, ""),
	VAL("insulation_pv2",	0x0025, T_U16, 0, ""),
	VAL("insulation_pv",	0x0026, T_U16, 0, ""),
	ENUM("country",		0x0027, country_names),
	VAL("string1_voltage",	0x0105, T_U16, -1, "V"),
	VAL("string1_current",	0x0106, T_U16, -2, "A"),
	VAL("string2_voltage",	0x0107, T_U16, -1, "V"),
	VAL("string2_current",	0x0108, T_U16, -2, "A"),
	VAL("string3_voltage",	0x0109, T_U16, -1, "V"),
	VAL("string3_current",	0x010A, T_U16, -2, "A"),
	VAL("string4_voltage",	0x010B, T_U16, -1, "V"),
	VAL("string4_current",	0x010C, T_U16, -2, "A"),
	VAL("string5_voltage",	0x010D, T_U16, -1, "V"),
	VAL("string5_current",	0x010E, T_U16, -2, "A"),
	VAL("string6_voltage",	0x010F, T_U16, -1, "V"),
	VAL("string6_current",	0x0110, T_U16, -2, "A"),
	VAL("string7_voltage",	0x0111, T_U16, -1, "V"),
	VAL("string7_current",	0x0112, T_U16, -2, "A"),
	VAL("string8_voltage",	0x0113, T_U16, -1, "V"),
	VAL("string8_current",	0x0114, T_U16, -2, "A"),
};

const size_t sofar_nitems = N(sofar_items);

static int reg_get(const struct sofar_data *d, uint16_t reg, uint16_t *v)
{
	if (d->have_main && reg < SOFAR_MAIN_START + SOFAR_MAIN_COUNT) {
		*v = d->main[reg - SOFAR_MAIN_START];
		return 0;
	}
	if (d->have_string && reg >= SOFAR_STRING_START &&
	    reg < SOFAR_STRING_START + SOFAR_STRING_COUNT) {
		*v = d->string[reg - SOFAR_STRING_START];
		return 0;
	}
	return -1;
}

int sofar_value(const struct sofar_data *d, const struct sofar_item *it,
		long *val, int *dec)
{
	uint16_t hi, lo;
	long v;
	int e;

	if (reg_get(d, it->reg, &hi))
		return -1;

	switch (it->type) {
	case T_S16:
		v = (int16_t)hi;
		break;
	case T_U32:
		if (reg_get(d, it->reg + 1, &lo))
			return -1;
		v = (long)((unsigned long)hi << 16 | lo);
		break;
	default:
		v = hi;
		break;
	}

	for (e = it->exp; e > 0; e--)
		v *= 10;
	*val = v;
	*dec = it->exp < 0 ? -it->exp : 0;
	return 0;
}

char *sofar_format(char *buf, size_t len, long val, int dec)
{
	long p = 1;
	int i;

	if (!dec) {
		snprintf(buf, len, "%ld", val);
		return buf;
	}
	for (i = 0; i < dec; i++)
		p *= 10;
	snprintf(buf, len, "%s%ld.%0*ld", val < 0 ? "-" : "",
		 (val < 0 ? -val : val) / p, dec, (val < 0 ? -val : val) % p);
	return buf;
}

static size_t append(char *buf, size_t len, size_t o, const char *s)
{
	int n;

	if (o >= len)
		return o;
	n = snprintf(buf + o, len - o, "%s", s);
	return o + (n > 0 ? (size_t)n : 0);
}

void sofar_status_text(const struct sofar_data *d, char *buf, size_t len)
{
	const struct sofar_item *it;
	uint16_t status, bits;
	size_t i, o = 0;
	int b, nfault = 0;
	char tmp[16];

	buf[0] = 0;
	if (reg_get(d, 0x0000, &status))
		return;
	if (status < N(status_names)) {
		o = append(buf, len, o, status_names[status]);
	} else {
		snprintf(tmp, sizeof(tmp), "Status %u", status);
		o = append(buf, len, o, tmp);
	}

	for (i = 0; i < sofar_nitems; i++) {
		it = &sofar_items[i];
		if (it->type != T_BITS || reg_get(d, it->reg, &bits))
			continue;
		for (b = 0; b < 16; b++) {
			if (!(bits & (1u << b)))
				continue;
			o = append(buf, len, o, nfault++ ? ", " : ": ");
			o = append(buf, len, o, it->names[b]);
		}
	}
}

const struct sofar_item *sofar_find(const char *key)
{
	for (size_t i = 0; i < sofar_nitems; i++)
		if (!strcmp(sofar_items[i].key, key))
			return &sofar_items[i];
	return NULL;
}
