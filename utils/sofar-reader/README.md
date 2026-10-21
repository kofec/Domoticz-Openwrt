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

The same source tree builds a second package, **goodwe-reader**, for GoodWe
inverters; see [goodwe-reader](#goodwe-reader) below.

## Build

Add this repository as a feed and select `Utilities -> sofar-reader`
and/or `Utilities -> goodwe-reader`, or build the packages alone:

```sh
make package/sofar-reader/compile V=s
```

Only the selected programs are compiled, so sofar-reader alone needs no
json-c or OpenSSL headers.

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
	option interval '300'
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
| `interval` | 300 | seconds between reads (min. 10); each read is also the PVOutput upload |
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

## goodwe-reader

Daemon for GoodWe DT family inverters (checked on a GW3000-XS), replacing
the Python `monitor_Goodwe.py`. Configured in `/etc/config/goodwe-reader`,
one `inverter` section per inverter, like sofar-reader. Depends on `libuci`,
`libcurl`, `libjson-c` and `libopenssl` (libcrypto for the SEMS+ login
hashes), all already present on a router running Domoticz.

- **LAN**: Modbus RTU over UDP 8899 to address 0x7F, one block of
  registers 30100-30172 (power 30127-30128, grid voltage 30118, energy
  today 30144, total 30145-30146, temperature, frequency, mode). Only
  function 03 is ever sent. UDP loses packets: `tries` x `timeout` s.
  The time of a reading is the router clock, the inverter clock is set by
  the cloud and drifts without it.
- **SEMS+ fallback**: when the inverter does not answer in the LAN, the
  cycle reads `eu-semsplus.goodwe.com` (login, signed requests, as the web
  site does). Data older than `sems_freshness` (30 min) is an echo: logged,
  not sent, and it does not reset the alarm clock. Account, password and
  station id come from `sems_file` (`gw2pvo.cfg`: `gw_account`,
  `gw_password`, `gw_station_id`); the plain password is not kept, only
  its base64(md5) as the login form sends it.
- **Night**: from sunset + `night_margin` to sunrise - `night_margin`
  (`getSunRiseSet` from Domoticz) the inverter is not asked; 0 W is sent
  once when the night starts and the clocks are shifted by the time slept,
  so a night never counts towards the alarm. Without sunrise/sunset every
  hour is day.
- **Yesterday in PVOutput**: on the first daytime cycle of a day the daemon
  compares yesterday in PVOutput (`getstatus.jsp`) with the SEMS+ power
  curve and sends only the missing 5-minute statuses (`addbatchstatus.jsp`,
  30 per request). Energy is integrated from the power, scaled to the SEMS+
  day total and kept between the neighbouring PVOutput values. A failure is
  retried after an hour.
- **Clocks**: 0 W after `zero_delay` s without a reading, a notification
  after `notify_after` s, repeated while it lasts, with the last error, and
  one more when data comes back. Texts are options, so they can be Polish.

| Option | Default | |
|---|---|---|
| `host`, `port` | -, 8899 | inverter (its Wi-Fi module) in the LAN |
| `interval` | 300 | seconds between reads |
| `timeout`, `tries` | 3, 3 | per UDP question |
| `idx_power`, `idx_voltage` | - | Custom sensor/Usage (W), Voltage (180-260 V only) |
| `idx_today_energy`, `idx_total_energy`, `idx_energy` | - | Wh, kWh, Electric (Instant+Counter) `W;Wh` |
| `idx_temperature`, `idx_frequency`, `idx_status` | - | LAN only; status is a Text device, sent on change |
| `zero_delay`, `notify_after` | 900, 86400 | see Clocks |
| `notify_subject`, `notify_text`, `notify_back_text`, `notify_error_text` | English | notification texts, a duration is appended |
| `night`, `night_margin`, `night_interval` | 1, 30, 3600 | night mode, minutes, longest sleep in s |
| `sems`, `sems_file`, `sems_url`, `sems_freshness` | 1, `/root/gw2pvo.cfg`, EU, 1800 | SEMS+ |
| `check_yesterday` | 1 | fill yesterday's gaps in PVOutput |
| `pvo_*` | | as for sofar-reader |

```sh
goodwe-reader -s home -n -v              # LAN and SEMS+ once, print, send nothing
goodwe-reader -s home -u 2026-10-20 -n   # show a day from SEMS+
goodwe-reader -s home -u 2026-10-20      # send that day to PVOutput (up to 14 days back)
logread -e goodwe-reader
```
