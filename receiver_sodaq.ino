#include <Arduino.h>
#include <string.h>
#include <stdlib.h>
#include "event_state.h"

#define USB_STREAM SerialUSB
#define LORA_STREAM Serial2

static const uint32_t USB_BAUD = 115200;
static const uint32_t LORA_BAUD = 57600;

static const char* SHARED_TOKEN = "SODAQ2026";

static const char* RADIO_FREQ = "868100000";
static const char* RADIO_SF = "sf7";
static const char* RADIO_BW = "125";
static const char* RADIO_CR = "4/5";
static const char* RADIO_PWR = "14";
static const char* RADIO_PRLEN = "8";
static const char* RADIO_SYNC = "12";
static const char* RADIO_WDT = "0";

static char loraLine[320];
static size_t loraLineLen = 0;

static bool readSerialLine(HardwareSerial& serial, char* out, size_t outSize, uint32_t timeoutMs)
{
    uint32_t start = millis();
    size_t index = 0;

    while ((millis() - start) < timeoutMs) {
        while (serial.available() > 0) {
            char c = (char)serial.read();

            if (c == '\r') {
                continue;
            }

            if (c == '\n') {
                if (index >= outSize) index = outSize - 1;
                out[index] = '\0';
                return true;
            }

            if (index < outSize - 1) {
                out[index++] = c;
            }
        }
    }

    if (outSize > 0) {
        out[0] = '\0';
    }

    return false;
}

static void writeCommand(const char* cmd)
{
    LORA_STREAM.print(cmd);
    LORA_STREAM.print("\r\n");
}

static bool waitLineEquals(const char* expected, uint32_t timeoutMs)
{
    char buf[320];

    while (readSerialLine(LORA_STREAM, buf, sizeof(buf), timeoutMs)) {
        if (buf[0] == '\0') {
            continue;
        }

        if (strcmp(buf, expected) == 0) {
            return true;
        }
    }

    return false;
}

static bool sendCommandExpectOk(const char* cmd, uint32_t timeoutMs)
{
    writeCommand(cmd);
    return waitLineEquals("ok", timeoutMs);
}

static bool pauseMac(void)
{
    writeCommand("mac pause");

    char buf[320];

    if (!readSerialLine(LORA_STREAM, buf, sizeof(buf), 1000)) {
        return false;
    }

    if (buf[0] == '\0') {
        return false;
    }

    for (size_t i = 0; buf[i] != '\0'; i++) {
        if (buf[i] < '0' || buf[i] > '9') {
            return false;
        }
    }

    return true;
}

static bool hexNibble(char c, uint8_t* out)
{
    if (c >= '0' && c <= '9') {
        *out = (uint8_t)(c - '0');
        return true;
    }

    if (c >= 'A' && c <= 'F') {
        *out = (uint8_t)(10 + c - 'A');
        return true;
    }

    if (c >= 'a' && c <= 'f') {
        *out = (uint8_t)(10 + c - 'a');
        return true;
    }

    return false;
}

static bool hexToBytes(const char* hex, uint8_t* out, size_t outMax, size_t* outLen)
{
    size_t len = strlen(hex);

    if ((len % 2) != 0) {
        return false;
    }

    size_t byteLen = len / 2;

    if (byteLen > outMax) {
        return false;
    }

    for (size_t i = 0; i < byteLen; i++) {
        uint8_t hi = 0;
        uint8_t lo = 0;

        if (!hexNibble(hex[i * 2], &hi)) {
            return false;
        }

        if (!hexNibble(hex[i * 2 + 1], &lo)) {
            return false;
        }

        out[i] = (uint8_t)((hi << 4) | lo);
    }

    *outLen = byteLen;
    return true;
}

static void bytesToText(const uint8_t* in, size_t len, char* out, size_t outMax)
{
    size_t copyLen = len;

    if (copyLen >= outMax) {
        copyLen = outMax - 1;
    }

    memcpy(out, in, copyLen);
    out[copyLen] = '\0';
}

static bool parseBool01(const char* s, bool* out)
{
    if (strcmp(s, "0") == 0) {
        *out = false;
        return true;
    }

    if (strcmp(s, "1") == 0) {
        *out = true;
        return true;
    }

    return false;
}

static bool parseU16(const char* s, uint16_t* out)
{
    char* endPtr = NULL;
    unsigned long v = strtoul(s, &endPtr, 10);

    if (*s == '\0' || *endPtr != '\0' || v > 65535UL) {
        return false;
    }

    *out = (uint16_t)v;
    return true;
}

static bool parseFloat32(const char* s, float* out)
{
    char* endPtr = NULL;
    float v = strtof(s, &endPtr);

    if (*s == '\0' || *endPtr != '\0') {
        return false;
    }

    *out = v;
    return true;
}

static int splitLine(char* line, char** parts, int maxParts)
{
    int count = 0;
    char* token = strtok(line, ";");

    while (token != NULL && count < maxParts) {
        parts[count++] = token;
        token = strtok(NULL, ";");
    }

    return count;
}

static bool tokenOk(const char* token)
{
    return strcmp(token, SHARED_TOKEN) == 0;
}

static void logStatus(const char* status, const char* evt)
{
    USB_STREAM.print(status);
    USB_STREAM.print(";");
    USB_STREAM.println(evt);
}

static void writeJsonLog(const char* jsonLine)
{
    USB_STREAM.print("JSON_LOG=");
    USB_STREAM.print(jsonLine);
}

static void handleCreatePlayer(char** parts, int count)
{
    if (count != 7) {
        logStatus("ERR", "CP");
        return;
    }

    if (!tokenOk(parts[6])) {
        logStatus("DENY", "CP");
        return;
    }

    uint16_t playerId = 0;
    uint16_t team = 0;
    bool isDead = false;

    if (!parseU16(parts[1], &playerId)) {
        logStatus("ERR", "CP");
        return;
    }

    if (!parseU16(parts[2], &team)) {
        logStatus("ERR", "CP");
        return;
    }

    if (!parseBool01(parts[5], &isDead)) {
        logStatus("ERR", "CP");
        return;
    }

    CreatePlayerOpts opts;
    opts.playerId = playerId;
    opts.team = team;
    opts.firstName = parts[3];
    opts.lastName = parts[4];
    opts.isDead = isDead;

    char jsonLog[JSON_LOG_SIZE];

    if (Event_CreatePlayer(opts, jsonLog, sizeof(jsonLog))) {
        logStatus("OK", "CP");
        writeJsonLog(jsonLog);
    } else {
        logStatus("FAIL", "CP");
    }
}

static void handleCreateMine(char** parts, int count)
{
    if (count != 7) {
        logStatus("ERR", "CM");
        return;
    }

    if (!tokenOk(parts[6])) {
        logStatus("DENY", "CM");
        return;
    }

    uint16_t mineId = 0;
    bool isActive = false;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    if (!parseU16(parts[1], &mineId)) {
        logStatus("ERR", "CM");
        return;
    }

    if (!parseBool01(parts[2], &isActive)) {
        logStatus("ERR", "CM");
        return;
    }

    if (!parseFloat32(parts[3], &x)) {
        logStatus("ERR", "CM");
        return;
    }

    if (!parseFloat32(parts[4], &y)) {
        logStatus("ERR", "CM");
        return;
    }

    if (!parseFloat32(parts[5], &z)) {
        logStatus("ERR", "CM");
        return;
    }

    CreateMineOpts opts;
    opts.mineId = mineId;
    opts.isActive = isActive;
    opts.pos.x = x;
    opts.pos.y = y;
    opts.pos.z = z;

    char jsonLog[JSON_LOG_SIZE];

    if (Event_CreateMine(opts, jsonLog, sizeof(jsonLog))) {
        logStatus("OK", "CM");
        writeJsonLog(jsonLog);
    } else {
        logStatus("FAIL", "CM");
    }
}

static void handleExplosion(char** parts, int count)
{
    if (count != 6) {
        logStatus("ERR", "EX");
        return;
    }

    if (!tokenOk(parts[5])) {
        logStatus("DENY", "EX");
        return;
    }

    uint16_t playerId = 0;
    uint16_t mineId = 0;
    bool isDead = false;
    bool isActive = false;

    if (!parseU16(parts[1], &playerId)) {
        logStatus("ERR", "EX");
        return;
    }

    if (!parseBool01(parts[2], &isDead)) {
        logStatus("ERR", "EX");
        return;
    }

    if (!parseU16(parts[3], &mineId)) {
        logStatus("ERR", "EX");
        return;
    }

    if (!parseBool01(parts[4], &isActive)) {
        logStatus("ERR", "EX");
        return;
    }

    char jsonLog[JSON_LOG_SIZE];

    if (Event_Explosion(playerId, isDead, mineId, isActive, jsonLog, sizeof(jsonLog))) {
        logStatus("OK", "EX");
        writeJsonLog(jsonLog);
    } else {
        logStatus("FAIL", "EX");
    }
}

static void handleFind(char** parts, int count)
{
    if (count != 5) {
        logStatus("ERR", "FD");
        return;
    }

    if (!tokenOk(parts[4])) {
        logStatus("DENY", "FD");
        return;
    }

    uint16_t playerId = 0;
    uint16_t mineId = 0;
    bool isFind = false;

    if (!parseU16(parts[1], &playerId)) {
        logStatus("ERR", "FD");
        return;
    }

    if (!parseU16(parts[2], &mineId)) {
        logStatus("ERR", "FD");
        return;
    }

    if (!parseBool01(parts[3], &isFind)) {
        logStatus("ERR", "FD");
        return;
    }

    char jsonLog[JSON_LOG_SIZE];

    if (Event_Find(playerId, mineId, isFind, jsonLog, sizeof(jsonLog))) {
        logStatus("OK", "FD");
        writeJsonLog(jsonLog);
    } else {
        logStatus("FAIL", "FD");
    }
}

static void processPayload(char* payload)
{
    char* parts[10];
    int count = splitLine(payload, parts, 10);

    if (count <= 0) {
        return;
    }

    if (strcmp(parts[0], "CP") == 0) {
        handleCreatePlayer(parts, count);
        return;
    }

    if (strcmp(parts[0], "CM") == 0) {
        handleCreateMine(parts, count);
        return;
    }

    if (strcmp(parts[0], "EX") == 0) {
        handleExplosion(parts, count);
        return;
    }

    if (strcmp(parts[0], "FD") == 0) {
        handleFind(parts, count);
        return;
    }

    logStatus("ERR", "UNKNOWN");
}

static bool radioConfig(void)
{
    writeCommand("sys reset");
    delay(500);

    char dump[320];

    while (readSerialLine(LORA_STREAM, dump, sizeof(dump), 100)) {
    }

    if (!pauseMac()) return false;
    if (!sendCommandExpectOk("radio set mod lora", 1000)) return false;

    char cmd[64];

    snprintf(cmd, sizeof(cmd), "radio set freq %s", RADIO_FREQ);
    if (!sendCommandExpectOk(cmd, 1000)) return false;

    snprintf(cmd, sizeof(cmd), "radio set sf %s", RADIO_SF);
    if (!sendCommandExpectOk(cmd, 1000)) return false;

    snprintf(cmd, sizeof(cmd), "radio set bw %s", RADIO_BW);
    if (!sendCommandExpectOk(cmd, 1000)) return false;

    snprintf(cmd, sizeof(cmd), "radio set cr %s", RADIO_CR);
    if (!sendCommandExpectOk(cmd, 1000)) return false;

    snprintf(cmd, sizeof(cmd), "radio set pwr %s", RADIO_PWR);
    if (!sendCommandExpectOk(cmd, 1000)) return false;

    snprintf(cmd, sizeof(cmd), "radio set prlen %s", RADIO_PRLEN);
    if (!sendCommandExpectOk(cmd, 1000)) return false;

    if (!sendCommandExpectOk("radio set crc on", 1000)) return false;
    if (!sendCommandExpectOk("radio set iqi off", 1000)) return false;

    snprintf(cmd, sizeof(cmd), "radio set wdt %s", RADIO_WDT);
    if (!sendCommandExpectOk(cmd, 1000)) return false;

    snprintf(cmd, sizeof(cmd), "radio set sync %s", RADIO_SYNC);
    if (!sendCommandExpectOk(cmd, 1000)) return false;

    return true;
}

static bool startReceive(void)
{
    if (!pauseMac()) {
        return false;
    }

    writeCommand("radio rx 0");
    return waitLineEquals("ok", 1000);
}

static void pollLoRa(void)
{
    while (LORA_STREAM.available() > 0) {
        char c = (char)LORA_STREAM.read();

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            loraLine[loraLineLen] = '\0';

            if (strncmp(loraLine, "radio_rx ", 9) == 0) {
                uint8_t raw[128];
                size_t rawLen = 0;
                char payload[128];

                if (hexToBytes(loraLine + 9, raw, sizeof(raw), &rawLen)) {
                    bytesToText(raw, rawLen, payload, sizeof(payload));
                    processPayload(payload);
                } else {
                    logStatus("ERR", "HEX");
                }

                startReceive();
            } else if (strcmp(loraLine, "radio_err") == 0) {
                startReceive();
            }

            loraLineLen = 0;
            continue;
        }

        if (loraLineLen < sizeof(loraLine) - 1) {
            loraLine[loraLineLen++] = c;
        } else {
            loraLineLen = 0;
        }
    }
}

void setup()
{
    USB_STREAM.begin(USB_BAUD);
    LORA_STREAM.begin(LORA_BAUD);

    delay(1500);

    EventState_Init();

    if (!radioConfig()) {
        USB_STREAM.println("LORA_CONFIG_FAIL");
        return;
    }

    if (!startReceive()) {
        USB_STREAM.println("LORA_RX_FAIL");
        return;
    }

    USB_STREAM.println("READY");
}

void loop()
{
    pollLoRa();
}