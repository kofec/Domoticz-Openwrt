# sofar-reader

Small C daemon for OpenWrt (written for 24.10, e.g. Netgear R6220) that
reads a Sofar K-TLX inverter through a Solarman LSW-3/LSE Wi-Fi data logger
(protocol V5, TCP port 8899) and pushes the values to Domoticz over its HTTP
JSON API. It replaces the Python `InverterData.py` from
[Sofar_LSW3](https://github.com/kofec/Sofar_LSW3).

Only dependency is `libuci`. No Python, no TLS: uploading to PVOutput is
left to Domoticz (see [PVOutput](#pvoutput)).

| | Python 3 + gw2pvo | sofar-reader |
|---|---|---|
| RSS (x86_64 glibc, measured) | 12 MB stdlib only, 28.6 MB with `requests` | 2.2 MB (mostly shared libc pages) |
| Flash | python3 + modules, several MB | 10 KB .ipk, 15 KB of code (mipsel_24kc, OpenWrt 24.10; the 64 KB file is mostly segment alignment) |

## Build

Add this repository as a feed and select
`Utilities -> sofar-reader`, or build the package alone:

```sh
make package/sofar-reader/compile V=s
```

## Configuration

`/etc/config/sofar-reader`, one `inverter` section per inverter; every
enabled section runs as its own procd instance.

```
config domoticz 'domoticz'
	option host '127.0.0.1'
	option port '8080'
	option username ''
	option password ''

config inverter 'ludynia'
	option enabled '1'
	option host '192.168.1.50'
	option port '8899'
	option serial '1712345678'
	option interval '60'
	option read_strings '0'
	option zero_offline '1'
	option idx_energy '10'
	option idx_status '11'
	option idx_pv1_power '12'
```

| Option | Default | |
|---|---|---|
| `host`, `port` | -, 8899 | data logger address |
| `serial` | - | **logger** S/N (decimal, on the logger sticker) |
| `slave` | 1 | Modbus address of the inverter |
| `interval` | 60 | seconds between reads (min. 10) |
| `timeout` | 10 | connect/receive timeout in seconds |
| `read_strings` | 0 | also read 0x0105-0x0114 (string 1-8 V/A) |
| `zero_offline` | 1 | send 0 W when the logger disappears (it is powered by the inverter, so it is off at night) |
| `idx_energy` | - | Domoticz *Electric (Instant+Counter)*, gets `output_power;total_energy*1000` (W;Wh) |
| `idx_status` | - | Domoticz *Text*, gets `Normal` or `FAULT: ID01 ...`; sent only when it changes |
| `idx_<name>` | - | any numeric value listed by `sofar-reader -n`, e.g. `idx_pv1_power`, `idx_l1_voltage`, `idx_module_temp` |

Domoticz device types that fit: *Voltage* for `*_voltage`, *Current
(Ampere 1 phase)* for `*_current`, *Temperature* for `*_temp`, *Custom
sensor* for the rest. If Domoticz requires a login, either add 127.0.0.1
to *Settings -> Local Networks* or set `username`/`password` (HTTP basic
auth, *Allow Basic-Auth authentication over plain HTTP* must be enabled).

## PVOutput

Domoticz has no PVOutput upload hardware (only *PVOutput (Input)*), so
[files/pvoutput.lua](files/pvoutput.lua) is a dzVents script doing what
gw2pvo did: every 5 minutes it posts energy today (v1), power (v2) and
L1 voltage (v6). It needs three devices fed by sofar-reader: `idx_energy`,
`idx_today_energy` (Custom sensor, Wh) and `idx_l1_voltage` (Voltage).
Domoticz does the HTTPS request with its own libcurl, the router side
needs no TLS. Paste the script in *Setup -> More Options -> Events* and
fill in the device names, system id and API key.

## Testing

```sh
sofar-reader -s ludynia -n -v      # read once, print JSON, send nothing
sofar-reader -s ludynia -1 -v      # read once and send to Domoticz
sofar-reader -s ludynia -1 -vv     # plus hex dumps of the V5 frames
/etc/init.d/sofar-reader restart
logread -e sofar-reader
```

## Differences from the Python script

- The Modbus CRC is computed. `InverterData.py` used two hardcoded CRCs,
  which are only valid for the default register ranges.
- The second block really reads 0x0105-0x0114. In `InverterData.py`
  `hex(pini)[2:4]` turns 0x0105 into 0x0010, so it requested 0x0010-0x001F
  and printed those values as "String 1-8 voltage/current".
- `total_energy` and `total_time` are read as 32-bit values (high word,
  low word).
- Replies are validated (V5 checksum, control code, Modbus CRC, length);
  heartbeat frames from the logger are skipped.
- Fault registers are decoded bit by bit into the status text.
