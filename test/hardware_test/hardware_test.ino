/**
 * hardware_test.ino — LLP v3.1.0 Hardware Test Firmware
 *
 * Responds to LLP frames over Serial at 115200 baud.
 * Responses are plain-text, one per line:
 *
 *   OK <len> <hex>    — Valid frame received
 *   ERR <code>        — Parse error
 *   [HB]              — Heartbeat (every 2s)
 *   [BOOT]            — Boot message
 *
 * The echo feature is used for roundtrip validation:
 * every received payload (len > 0) is echoed back as an LLP frame.
 */

#include <Arduino.h>
#include "llp_protocol.h"

static llp_parser_t parser;
static unsigned long last_hb = 0;


static void send_llp(const uint8_t* data, uint16_t len) {
    uint8_t payload[LLP_MAX_PAYLOAD];
    size_t plen = llp_build_final_payload(payload, sizeof(payload), data, len);
    if (plen == 0) return;
    uint8_t frame[LLP_MAX_FRAME_SIZE(plen)];
    size_t flen = llp_build_frame(frame, sizeof(frame), payload, (uint16_t)plen);
    if (flen > 0) Serial.write(frame, flen);
}


void setup() {
    Serial.begin(115200);
    llp_parser_init(&parser);
    Serial.println("[BOOT] LLP Hardware Test v3.1.0");
}


void loop() {
    unsigned long now = millis();

    while (Serial.available() > 0) {
        uint8_t b = Serial.read();
        int r = llp_parser_process_byte(&parser, b, now);

        if (r == 1) {
            uint16_t raw_len;
            const uint8_t* raw = llp_get_final_payload_ptr(&parser.frame, &raw_len);

            Serial.print("OK ");
            Serial.print(raw_len);
            Serial.print(" ");
            for (uint16_t i = 0; i < raw_len && i < LLP_MAX_PAYLOAD; i++) {
                if (raw[i] < 0x10) Serial.print('0');
                Serial.print(raw[i], HEX);
            }
            Serial.println();

            if (raw_len > 0) send_llp(raw, raw_len);

        } else if (r == -1) {
            Serial.print("ERR ");
            Serial.println(parser.error_code);
        }
    }

    if (now - last_hb >= 2000) {
        last_hb = now;
        Serial.println("[HB]");
    }
}
