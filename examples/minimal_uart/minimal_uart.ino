/**
 * LLP Protocol - Minimal UART Example (Arduino)
 * 
 * Este ejemplo muestra la forma más simple de usar LLP:
 * - Recibir frames por Serial (UART)
 * - Enviar frames de respuesta
 * - Procesar diferentes tipos de mensaje
 * 
 * Hardware: Arduino UNO + Serial Monitor
 * NOTA: Ajustar LLP_MAX_PAYLOAD del protocolo según microcontrolador
 * Velocidad: 9600 baud
 */

#include "llp-protocol.h"

// ============= GLOBALS =============
llp_parser_t parser;
uint8_t tx_buffer[LLP_HEADER_SIZE + LLP_MAX_PAYLOAD + 2];

static uint16_t next_cmd_id = 1;

// ============= SETUP =============
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  llp_parser_init(&parser);
  
  Serial.println(F("\n=== LLP Protocol - Minimal UART Example ==="));
  Serial.println(F("Esperando frames...\n"));
  
  // Debug: enviar un PING cada 10s
  Serial.println(F("Tip: Abre Serial Monitor (9600 baud) para enviar frames\n"));
}

// ============= MAIN LOOP =============
void loop() {
  // Procesar bytes entrantes
  while (Serial.available()) {
    uint8_t byte = Serial.read();
    int result = llp_parser_process_byte(&parser, byte, millis());
    
    if (result == 1) {
      // ✅ Frame válido recibido
      handleReceivedFrame(&parser.frame);
      
    } else if (result == -1) {
      // ❌ Error en frame
      printError(parser.error_code);
    }
  }

  // Enviar PING cada 10 segundos como demostración
  static unsigned long lastPing = 0;
  if (millis() - lastPing > 10000) {
    lastPing = millis();
    sendPing();
  }

  // Mostrar estadísticas cada 30s
  static unsigned long lastStats = 0;
  if (millis() - lastStats > 30000) {
    lastStats = millis();
    printStats();
  }
}

// ============= FRAME HANDLERS =============

void handleReceivedFrame(llp_frame_t *frame) {
  Serial.print(F("[RX] Type: 0x"));
  Serial.print(frame->type, HEX);
  Serial.print(F(" | ID: "));
  Serial.print(frame->id);
  Serial.print(F(" | Len: "));
  Serial.print(frame->payload_len);
  Serial.print(F(" | Data: "));

  // Mostrar payload
  for (uint16_t i = 0; i < frame->payload_len; i++) {
    if (frame->payload[i] >= 0x20 && frame->payload[i] < 0x7F) {
      Serial.write(frame->payload[i]);
    } else {
      Serial.print(F("[0x"));
      Serial.print(frame->payload[i], HEX);
      Serial.print(F("]"));
    }
  }
  Serial.println();

  // Procesar según tipo
  switch (frame->type) {
    case LLP_PING:
      Serial.println(F("  → Recibido PING, enviando ACK..."));
      sendAck(frame->id, LLP_ERR_OK);
      break;

    case LLP_DATA:
      Serial.println(F("  → Recibido DATA"));
      sendAck(frame->id, LLP_ERR_OK);
      break;

    case LLP_CONFIG:
      Serial.println(F("  → Recibido CONFIG"));
      sendAck(frame->id, LLP_ERR_OK);
      break;

    case LLP_COMMAND:
      Serial.println(F("  → Recibido COMMAND"));
      handleCommand(frame);
      break;

    case LLP_ACK:
      Serial.println(F("  → Recibido ACK"));
      break;

    case LLP_NACK:
      Serial.println(F("  → Recibido NACK (error)"));
      break;

    default:
      Serial.print(F("  → Tipo desconocido (0x"));
      Serial.print(frame->type, HEX);
      Serial.println(F(")"));
      break;
  }
}

void handleCommand(llp_frame_t *frame) {
  // Ejemplo: si el payload es "LED_ON", activar LED
  if (frame->payload_len == 6 && 
      strncmp((char*)frame->payload, "LED_ON", 6) == 0) {
    Serial.println(F("    → Comando: LED_ON (dummy, no hay LED físico)"));
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
  }
}

// ============= TX FUNCTIONS =============

void sendPing() {
  size_t len = llp_build_frame(
    tx_buffer, sizeof(tx_buffer),
    LLP_PING,
    next_cmd_id++,
    NULL, 0
  );

  Serial.print(F("[TX] Enviando PING (ID "));
  Serial.print(next_cmd_id - 1);
  Serial.println(F(")..."));
  
  if (len > 0) {
    Serial.write(tx_buffer, len);
  } else {
    Serial.println(F("[ERROR] Failed to build frame"));
  }
}

void sendAck(uint16_t id, uint8_t ack_code) {
  uint8_t payload[] = {ack_code};
  size_t len = llp_build_frame(
    tx_buffer, sizeof(tx_buffer),
    LLP_ACK,
    id,
    payload, 1
  );

  Serial.print(F("[TX] Enviando ACK (ID "));
  Serial.print(id);
  Serial.print(F(", code: 0x"));
  Serial.print(ack_code, HEX);
  Serial.println(F(")"));
  
  if (len > 0) {
    Serial.write(tx_buffer, len);
  } else {
    Serial.println(F("[ERROR] Failed to build frame"));
  }
}

void sendData(const uint8_t *data, uint16_t len) {
  size_t frame_len = llp_build_frame(
    tx_buffer, sizeof(tx_buffer),
    LLP_DATA,
    next_cmd_id++,
    data, len
  );

  Serial.print(F("[TX] Enviando DATA (ID "));
  Serial.print(next_cmd_id - 1);
  Serial.print(F(", "));
  Serial.print(len);
  Serial.println(F(" bytes)"));
  
  if (len > 0) {
    Serial.write(tx_buffer, len);
  } else {
    Serial.println(F("[ERROR] Failed to build frame"));
  }
}

// ============= UTILITIES =============

void printError(uint8_t error_code) {
  Serial.print(F("[ERROR] 0x"));
  Serial.print(error_code, HEX);
  Serial.print(" - ");

  switch (error_code) {
    case LLP_ERR_CHECKSUM:
      Serial.println(F("CRC Inválido"));
      break;
    case LLP_ERR_TIMEOUT:
      Serial.println(F("Timeout (frame incompleto)"));
      break;
    case LLP_ERR_PAYLOAD_LEN:
      Serial.println(F("Payload demasiado largo"));
      break;
    case LLP_ERR_SYNC:
      Serial.println(F("Problema de sincronismo"));
      break;
    default:
      Serial.println(F("Desconocido"));
      break;
  }
}

void printStats() {
  uint32_t ok, err, timeout;
  llp_get_stats(&parser, &ok, &err, &timeout);

  Serial.println(F("\n--- ESTADÍSTICAS ---"));
  Serial.print(F("Frames OK: "));
  Serial.println(ok);
  Serial.print(F("Frames Error: "));
  Serial.println(err);
  Serial.print(F("Timeouts: "));
  Serial.println(timeout);
  Serial.println(F("---\n"));
}
