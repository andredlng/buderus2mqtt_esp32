# buderus2mqtt_esp32

Read data from a Buderus Logamatic 4000 series heating controller via serial interface and publish to MQTT. ESP32 firmware for the [Olimex ESP32-POE-ISO-IND](https://www.olimex.com/Products/IoT/ESP32/ESP32-POE-ISO/open-source-hardware).

Based on [l4000-daemon.pl](https://www.holzleitner.com/el/buderus-monitor/index-de.html) v2.0 by Peter G. Holzleitner, ported from [buderus2mqtt](https://c0d3.sh/andre/buderus2mqtt) (Python).

## Hardware

- Olimex ESP32-POE-ISO-IND (Ethernet with PoE, galvanic isolation, industrial temp range)
- Level shifter circuit (10 KOhm, 12V Zener diode, transistor, pull-up resistor) connected to the Logamatic's room sensor interface
- Level shifter output -> ESP32 GPIO13 (UEXT header, RX only)
- Communication runs at 1200 bps, 8N1

## Building & Flashing

Requires [PlatformIO](https://platformio.org/).

```sh
# Build firmware
uv run --with platformio pio run -e esp32-poe-iso

# Upload filesystem (config.json)
uv run --with platformio pio run -t uploadfs -e esp32-poe-iso

# Upload firmware
uv run --with platformio pio run -t upload -e esp32-poe-iso

# Monitor serial output
uv run --with platformio pio device monitor -e esp32-poe-iso
```

## Configuration

Edit `data/config.json` and upload via `pio run -t uploadfs`:

| Option | Default | Description |
|--------|---------|-------------|
| `mqtt_host` | `localhost` | MQTT broker host |
| `mqtt_port` | `1883` | MQTT broker port |
| `mqtt_clientid` | `buderus2mqtt` | MQTT client ID |
| `mqtt_user` | | MQTT username |
| `mqtt_password` | | MQTT password |
| `mqtt_topic` | `heating` | MQTT topic root |
| `mqtt_keepalive` | `30` | MQTT keepalive interval (seconds) |

## MQTT Topics

All values are published under `{mqtt_topic}/{key}`:

| Key | Description |
|-----|-------------|
| `status` | Device status (`online`/`offline` via LWT) |
| `hk{1-9}` | Heating zone room temperature |
| `hk{1-9}_s` | Zone setpoint |
| `hk{1-9}_sg` | Zone actuator position |
| `hk{1-9}_v` | Zone flow temperature |
| `hk{1-9}_vs` | Zone flow setpoint |
| `hk{1-9}_pu` | Zone pump |
| `hk{1-9}_err` | Zone error |
| `ww` | Hot water temperature |
| `ww_s` | Hot water setpoint |
| `ww_l` | Hot water loading |
| `ww_laden` | Charge pump |
| `ww_zirk` | Circulation pump |
| `ww_err` | Hot water error |
| `kessel` | Boiler temperature |
| `kessel_s` | Boiler setpoint |
| `brenner` | Burner status |
| `k_ein` | Boiler on threshold |
| `k_aus` | Boiler off threshold |
| `kessel_err` | Boiler error |
| `aussen` | Outdoor temperature |
| `aussen_d` | Damped outdoor temperature |
| `aussen_err` | Outdoor sensor error |
| `energie` | Energy pulse counter |
| `sol_coll` | Solar collector temperature |
| `sol_t1` | Solar tank 1 temperature |
| `sol_t2` | Solar tank 2 temperature |
| `sol_pump` | Solar pump status |
| `sol_err` | Solar error |
| `diag_reclen_err` | Last record rejected for its length, with its data as hex, e.g. `rec=88 len=36 expected=42 data=…` |
| `diag_reclen_count` | Number of records rejected for their length since boot |
| `diag_discard_{recnum}` | Serial bytes last skipped before a frame of record `recnum` (e.g. a corrupt frame), with that frame's `recnum:payofs` and the payload offset the record expected |

## Tests

```sh
uv run --with platformio pio test -e native
```

## Acknowledgements

The Buderus Logamatic 4000 serial protocol implementation is based on the work of [Peter G. Holzleitner](https://www.holzleitner.com/el/buderus-monitor/index-de.html), whose l4000-daemon.pl (2015-2022) reverse-engineered and documented the binary protocol.

## License

BSD-2-Clause
