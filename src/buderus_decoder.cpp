#include "buderus_decoder.h"
#include <cstdio>
#include <cstring>

bool BuderusDecoder::reclenCheck(int expected, size_t actual) {
    if (expected > 0 && (int)actual != expected) return false;
    if (expected < 0 && (int)actual < -expected) return false;
    return true;
}

size_t BuderusDecoder::addMsg(MqttMessage* out, size_t idx, size_t max, const char* key, const char* value) {
    if (idx >= max) return idx;
    strlcpy(out[idx].key, key, sizeof(out[idx].key));
    strlcpy(out[idx].value, value, sizeof(out[idx].value));
    return idx + 1;
}

size_t BuderusDecoder::addMsgInt(MqttMessage* out, size_t idx, size_t max, const char* key, int value) {
    if (idx >= max) return idx;
    strlcpy(out[idx].key, key, sizeof(out[idx].key));
    snprintf(out[idx].value, sizeof(out[idx].value), "%d", value);
    return idx + 1;
}

size_t BuderusDecoder::addMsgFloat(MqttMessage* out, size_t idx, size_t max, const char* key, float value) {
    if (idx >= max) return idx;
    strlcpy(out[idx].key, key, sizeof(out[idx].key));
    snprintf(out[idx].value, sizeof(out[idx].value), "%.1f", value);
    return idx + 1;
}

static int expectedRecLen(uint8_t recnum) {
    if ((recnum >= 0x80 && recnum <= 0x83) || (recnum >= 0x8a && recnum <= 0x8e)) return 18;
    switch (recnum) {
        case 0x84: return 12;
        case 0x88: return 42;
        case 0x89: return -18;
        case 0x9B: return 36;
        case 0x9E: return -10;
        default: return 0;
    }
}

size_t BuderusDecoder::dispatch(uint8_t recnum, const uint8_t* data, size_t len, MqttMessage* out, size_t max_out) {
    // Publish rejected records so framing problems are visible without a serial console.
    int expected = expectedRecLen(recnum);
    if (expected != 0 && !reclenCheck(expected, len)) {
        reclen_errors_++;
        char val[sizeof(out[0].value)];
        int pos = snprintf(val, sizeof(val), "rec=%02x len=%u %s=%d data=",
                           recnum, (unsigned)len, expected < 0 ? "min" : "expected", expected < 0 ? -expected : expected);
        for (size_t i = 0; i < len && pos + 3 <= (int)sizeof(val); i++) {
            pos += snprintf(val + pos, sizeof(val) - pos, "%02x", data[i]);
        }
        size_t n = 0;
        n = addMsg(out, n, max_out, "diag_reclen_err", val);
        char cnt[16];
        snprintf(cnt, sizeof(cnt), "%u", (unsigned)reclen_errors_);
        n = addMsg(out, n, max_out, "diag_reclen_count", cnt);
        return n;
    }

    // Zone records: 0x80-0x83 (zones 1-4), 0x8a-0x8e (zones 5-9)
    if (recnum >= 0x80 && recnum <= 0x83) {
        uint8_t zone = recnum - 0x80 + 1; // 1-4
        uint32_t run = run_zone_[zone - 1]++;
        return decodeZone(zone, data, len, out, max_out, run);
    }
    if (recnum >= 0x8a && recnum <= 0x8e) {
        uint8_t zone = recnum - 0x8a + 5; // 5-9
        uint32_t run = run_zone_[zone - 1]++;
        return decodeZone(zone, data, len, out, max_out, run);
    }

    switch (recnum) {
        case 0x84: { uint32_t run = run_water_++;  return decodeWater(data, len, out, max_out, run); }
        case 0x87: return 0; // error log not implemented
        case 0x88: { uint32_t run = run_boiler_++; return decodeBoiler(data, len, out, max_out, run); }
        case 0x89: { uint32_t run = run_config_++; return decodeConfig(data, len, out, max_out, run); }
        case 0x9B: { uint32_t run = run_energy_++; return decodeEnergy(data, len, out, max_out, run); }
        case 0x9E: { uint32_t run = run_solar_++;  return decodeSolar(data, len, out, max_out, run); }
        default: return 0;
    }
}

size_t BuderusDecoder::decodeZone(uint8_t zone, const uint8_t* data, size_t len, MqttMessage* out, size_t max_out, uint32_t run) {
    if (!reclenCheck(18, len)) return 0;

    // Throttle: publish when run % 2 == zone % 2
    if (run % 2 != zone % 2) return 0;

    uint8_t vs = data[2];
    uint8_t vi = data[3];
    float rs = data[4] / 2.0f;
    float ri = data[5] / 2.0f;
    int8_t sg = signedByte(data[9]);
    uint8_t pu = data[8];

    // Error flags
    char err[256] = "";
    size_t err_len = 0;
    auto appendErr = [&](const char* msg) {
        if (err_len > 0) { err_len += snprintf(err + err_len, sizeof(err) - err_len, "; "); }
        err_len += snprintf(err + err_len, sizeof(err) - err_len, "%s", msg);
    };
    if (data[1] & 0x04) appendErr("Fernbedienungs-Kommunikation gestoert");
    if (data[1] & 0x08) appendErr("Fernbedienungs-Fehler");
    if (data[1] & 0x10) appendErr("Fehler Vorlauffuehler");
    if (data[1] & 0x20) appendErr("Maximale Vorlauftemperatur");
    if (data[1] & 0x40) appendErr("Externe Fehlermeldung");
    if (data[10] & 0x20) appendErr("Betriebsartschalter: AUS");
    if (data[10] & 0x40) appendErr("Betriebsartschalter: MANUELL");

    char key[24];
    size_t n = 0;

    snprintf(key, sizeof(key), "hk%d_s", zone);
    n = addMsgFloat(out, n, max_out, key, rs);

    snprintf(key, sizeof(key), "hk%d_sg", zone);
    n = addMsgInt(out, n, max_out, key, sg);

    snprintf(key, sizeof(key), "hk%d_vs", zone);
    n = addMsgInt(out, n, max_out, key, vs);

    snprintf(key, sizeof(key), "hk%d_err", zone);
    n = addMsg(out, n, max_out, key, err);

    if (vi != 110) {
        snprintf(key, sizeof(key), "hk%d_v", zone);
        n = addMsgInt(out, n, max_out, key, vi);
    }

    if (ri != 55.0f) {  // data[5] == 110 -> 55.0 = invalid sensor
        snprintf(key, sizeof(key), "hk%d", zone);
        n = addMsgFloat(out, n, max_out, key, ri);

        snprintf(key, sizeof(key), "hk%d_pu", zone);
        n = addMsgInt(out, n, max_out, key, pu);
    }

    return n;
}

size_t BuderusDecoder::decodeWater(const uint8_t* data, size_t len, MqttMessage* out, size_t max_out, uint32_t run) {
    if (!reclenCheck(12, len)) return 0;

    // Throttle: publish on even runs
    if (run % 2 != 0) return 0;

    uint8_t ws = data[2];
    uint8_t wi = data[3];
    int la = (data[1] & 0x01) ? 1 : 0;
    int p0 = (data[5] & 0x01) ? 1 : 0;
    int p1 = (data[5] & 0x02) ? 1 : 0;

    // Error flags
    char err[256] = "";
    size_t err_len = 0;
    auto appendErr = [&](const char* msg) {
        if (err_len > 0) { err_len += snprintf(err + err_len, sizeof(err) - err_len, "; "); }
        err_len += snprintf(err + err_len, sizeof(err) - err_len, "%s", msg);
    };
    if (data[0] & 0x10) appendErr("Fehler bei Desinfektion");
    if (data[0] & 0x20) appendErr("Fehler in Temperatursensor");
    if (data[0] & 0x40) appendErr("Fehler - Warmwasser bleibt kalt");
    if (data[0] & 0x80) appendErr("Fehler in Inertanode");
    if (data[6] & 0x20) appendErr("Betriebsartschalter: AUS");
    if (data[6] & 0x40) appendErr("Betriebsartschalter: MANUELL");
    if (data[7] & 0x01) appendErr("Externe Fehlermeldung");

    size_t n = 0;
    n = addMsgInt(out, n, max_out, "ww", wi);
    n = addMsgInt(out, n, max_out, "ww_s", ws);
    n = addMsgInt(out, n, max_out, "ww_l", la);
    n = addMsgInt(out, n, max_out, "ww_laden", p0);
    n = addMsgInt(out, n, max_out, "ww_zirk", p1);
    n = addMsg(out, n, max_out, "ww_err", err);

    return n;
}

size_t BuderusDecoder::decodeBoiler(const uint8_t* data, size_t len, MqttMessage* out, size_t max_out, uint32_t run) {
    if (!reclenCheck(42, len)) return 0;

    // Throttle: publish on even runs
    if (run % 2 != 0) return 0;

    uint8_t ks = data[0];
    uint8_t ki = data[1];
    uint8_t k1 = data[2];
    uint8_t k0 = data[3];
    uint8_t br = data[8];

    // Error flags
    char err[256] = "";
    size_t err_len = 0;
    auto appendErr = [&](const char* msg) {
        if (err_len > 0) { err_len += snprintf(err + err_len, sizeof(err) - err_len, "; "); }
        err_len += snprintf(err + err_len, sizeof(err) - err_len, "%s", msg);
    };
    if (data[6] & 0x01) appendErr("BRENNERSTOERUNG");
    if (data[6] & 0x02) appendErr("Fehler: KESSELFUEHLER");
    if (data[6] & 0x04) appendErr("Fehler: ZUS.-FUEHLER");
    if (data[6] & 0x08) appendErr("Fehler: KESSEL KALT");
    if (data[6] & 0x10) appendErr("Fehler: ABGAS-FUEHLER");
    if (data[6] & 0x20) appendErr("Fehler: ABGAS-GRENZTEMPERATUR");
    if (data[6] & 0x40) appendErr("Fehler: Sicherheitskette hat abgeschaltet");
    if (data[6] & 0x80) appendErr("Externe Fehlermeldung");
    if (data[34] & 0x20) appendErr("Betriebsartschalter: AUS");
    if (data[34] & 0x40) appendErr("Betriebsartschalter: MANUELL");

    // Operating status from byte 7
    int betrieb = (data[7] & 0x08) ? 1 : 0;
    int stufe1  = (data[7] & 0x02) ? 1 : 0;
    int stufe2  = (data[7] & 0x40) ? 1 : 0;

    // Operating mode from byte 34
    char modus[64] = "";
    if (data[34] & 0x02) strlcat(modus, "0", sizeof(modus));
    else if (data[34] & 0x04) strlcat(modus, "Auto", sizeof(modus));
    else if (data[34] & 0x08) strlcat(modus, "1", sizeof(modus));
    else if (data[34] & 0x10) strlcat(modus, "2", sizeof(modus));

    size_t n = 0;
    n = addMsgInt(out, n, max_out, "kessel", ki);
    n = addMsgInt(out, n, max_out, "kessel_s", ks);
    n = addMsgInt(out, n, max_out, "brenner", br);
    n = addMsgInt(out, n, max_out, "brenner_betrieb", betrieb);
    n = addMsgInt(out, n, max_out, "brenner_stufe1", stufe1);
    n = addMsgInt(out, n, max_out, "brenner_stufe2", stufe2);
    n = addMsg(out, n, max_out, "brenner_modus", modus);
    n = addMsgInt(out, n, max_out, "k_ein", k1);
    n = addMsgInt(out, n, max_out, "k_aus", k0);
    n = addMsg(out, n, max_out, "kessel_err", err);

    return n;
}

size_t BuderusDecoder::decodeConfig(const uint8_t* data, size_t len, MqttMessage* out, size_t max_out, uint32_t run) {
    if (!reclenCheck(-18, len)) return 0;

    int at1 = signedByte(data[0]);
    int at2 = signedByte(data[1]);

    const char* err = "";
    bool sensor_fault = false;
    if (at1 == 110) {
        sensor_fault = true;
        err = "Aussentemperatur-Sensor defekt";
    }

    // Plausibility checks
    if (!sensor_fault && (at1 < -40 || at1 > 50)) return 0;
    if (at2 < -40 || at2 > 50) return 0;

    // Throttle: publish every 5th run, only if sensor is valid
    if (run % 5 != 0 || sensor_fault) return 0;

    size_t n = 0;
    n = addMsgInt(out, n, max_out, "aussen", at1);
    n = addMsgInt(out, n, max_out, "aussen_d", at2);
    n = addMsg(out, n, max_out, "aussen_err", err);

    return n;
}

size_t BuderusDecoder::decodeEnergy(const uint8_t* data, size_t len, MqttMessage* out, size_t max_out, uint32_t run) {
    if (!reclenCheck(36, len)) return 0;

    // Throttle: publish every 13th run
    if (run % 13 != 0) return 0;

    uint32_t wm = ((uint32_t)data[30] << 24) | ((uint32_t)data[31] << 16) |
                  ((uint32_t)data[32] << 8)  | (uint32_t)data[33];

    size_t n = 0;
    char val[16];
    snprintf(val, sizeof(val), "%u", wm);
    n = addMsg(out, n, max_out, "energie", val);

    return n;
}

size_t BuderusDecoder::decodeSolar(const uint8_t* data, size_t len, MqttMessage* out, size_t max_out, uint32_t run) {
    if (len < 10) return 0;
    (void)run; // solar publishes every run

    float ct = ((uint16_t)data[3] * 256 + data[4]) / 10.0f;
    uint8_t pu = data[5];
    uint8_t t1 = data[6];
    uint8_t t2 = data[8];

    // Error flags
    char err[128] = "";
    size_t err_len = 0;
    auto appendErr = [&](const char* msg) {
        if (err_len > 0) { err_len += snprintf(err + err_len, sizeof(err) - err_len, "; "); }
        err_len += snprintf(err + err_len, sizeof(err) - err_len, "%s", msg);
    };
    if (data[0] & 0x01) appendErr("Hyst Error");
    if (data[0] & 0x02) appendErr("Tank 2 Temp Limit");
    if (data[0] & 0x04) appendErr("Tank 1 Temp Limit");
    if (data[0] & 0x08) appendErr("Collector Temp Limit");

    size_t n = 0;
    n = addMsgFloat(out, n, max_out, "sol_coll", ct);
    n = addMsgInt(out, n, max_out, "sol_t1", t1);
    n = addMsgInt(out, n, max_out, "sol_t2", t2);
    n = addMsgInt(out, n, max_out, "sol_pump", pu);
    n = addMsg(out, n, max_out, "sol_err", err);

    return n;
}
