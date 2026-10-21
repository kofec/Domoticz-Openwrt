# sofar-reader

Small C daemon for OpenWrt (written for 24.10, e.g. Netgear R6220) that
reads a Sofar K-TLX inverter through a Solarman LSW-3/LSE Wi-Fi data logger
(protocol V5, TCP port 8899) and pushes the values to Domoticz over its HTTP
JSON API. It replaces the Python `InverterData.py` from
[Sofar_LSW3](https://github.com/kofec/Sofar_LSW3).

Dependencies are `libuci` and `libcurl`. No Python. The optional PVOutput
upload uses libcurl over HTTPS; a router running Domoticz already has
libcurl and OpenSSL installed and loaded, so it adds no flash (see
[PVOutput](#pvoutput)).

| | Python 3 + gw2pvo | sofar-reader |
|---|---|---|
| RSS (x86_64 glibc, measured) | 12 MB stdlib only, 28.6 MB with `requests` | 2.2 MB (mostly shared libc pages) |
| Netgear R6220, 24.10, 15 min with PVOutput (measured) | 4 MB private RSS + 5.2 MB in swap, peak 15.7 MB (urllib monitor) | 0.8 MB private, 5 MB shared libraries, peak 6.2 MB |
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
| `zero_delay` | 900 | seconds without data before the 0 W; a logger with weak Wi-Fi drops single reads |
| `notify_after` | 86400 | Domoticz notification after this many seconds without data, repeated while it lasts; 0 = off |
| `notify_subject`, `notify_text` | `sofar-reader`, `No data from the inverter for` | notification subject and text, the text gets `<hours> h` appended |
| `idx_energy` | - | Domoticz *Electric (Instant+Counter)*, gets `output_power;total_energy*1000` (W;Wh) |
| `idx_status` | - | Domoticz *Text*, gets `Normal` or `FAULT: ID01 ...`; sent only when it changes |
| `idx_<name>` | - | any numeric value listed by `sofar-reader -n`, e.g. `idx_pv1_power`, `idx_l1_voltage`, `idx_module_temp` |
| `pvo_system_id` | - | PVOutput system id; empty = no upload |
| `pvo_api_key_file` | - | file with the PVOutput API key, e.g. `/root/lib/secrets.conf` |
| `pvo_api_key_name` | - | key name in that file (`name = key` lines); unset = first non-comment line |
| `pvo_api_key` | - | the key itself, if you do not mind it in `/etc/config` and its backups |
| `pvo_interval` | 300 | seconds between uploads, should match the PVOutput status interval |

Domoticz device types that fit: *Voltage* for `*_voltage`, *Current
(Ampere 1 phase)* for `*_current`, *Temperature* for `*_temp`, *Custom
sensor* for the rest. If Domoticz requires a login, either add 127.0.0.1
to *Settings -> Local Networks* or set `username`/`password` (HTTP basic
auth, *Allow Basic-Auth authentication over plain HTTP* must be enabled).

## PVOutput

With `pvo_system_id` and a key, every `pvo_interval` seconds sofar-reader
posts what gw2pvo did to `addstatus.jsp`: power (v2), energy today (v1)
and L1 voltage (v6), stamped with the router's local time. Nothing is sent
while the inverter is offline. A failed request (a TLS handshake on a busy
router can time out) is retried once after 5 s, then logged and dropped.

The libcurl handle is freed after each request. Measured on an R6220 with
the `curl` tool: one HTTPS request peaks at 1.4 MB private memory, the
4.2 MB of libcurl/libssl/libcrypto pages are shared with Domoticz.

`sofar-reader -n` logs the request it would send without sending it.

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
