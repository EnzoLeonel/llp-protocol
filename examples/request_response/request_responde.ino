/**
 * LLP Protocol - Request-Response Pattern
 * 
 * Ejemplo de patrón request-response con timeout y reintentos.
 * Útil para comunicación confiable donde necesitas garantizar
 * que el otro lado recibió y procesó tu comando.
 */

#include "llp_protocol.h"

#define ACK_TIMEOUT_MS 1000
#define MAX_RETRIES 3

// ============= STRUCTURES =============

struct PendingRequest {
  uint16_t id;
  unsigned long sent_time;
  uint8_t retry_count;
  uint8_t frame[520];
  size_t frame_len;
  bool in_use;
};

// ============= GLOBALS =============

llp_parser_t parser;
uint8_t tx_buffer[520];

#define MAX_PENDING_REQUESTS 5
struct PendingRequest pending[MAX_PENDING_REQUESTS];

static uint16_t next_cmd_id = 1;

// ============= SETUP =============

void setup() {
  Serial.begin(9600);
  llp_parser_init(&parser);

  memset(pending, 0, sizeof(pending));

  Serial.println("\n=== LLP Protocol - Request-Response ===");
  Serial.println("Enviando comando cada 5 segundos con reintentos\n");
}

// ============= MAIN LOOP =============

void loop() {
  // Procesar bytes entrantes
  while (Serial.available()) {
    uint8_t byte = Serial.read();
    int result = llp_parser_process_byte(&parser, byte, millis());

    if (result == 1) {
      handleReceivedFrame(&parser.frame);
    }
  }

  // Verificar reintentos de solicitudes pendientes
  checkTimeouts();

  // Enviar comando de ejemplo cada 5 segundos
  static unsigned long lastCmd = 0;
  if (millis() - lastCmd > 5000) {
    lastCmd = millis();
    sendCommandWithRetry("HELLO");
  }
}

// ============= HANDLERS =============

void handleReceivedFrame(llp_frame_t *frame) {
  Serial.print("[RX] Type: 0x");
  Serial.print(frame->type, HEX);
  Serial.print(" ID: ");
  Serial.println(frame->id);

  if (frame->type == LLP_ACK) {
    // Encontrar request pendiente
    for (int i = 0; i < MAX_PENDING_REQUESTS; i++) {
      if (pending[i].in_use && pending[i].id == frame->id) {
        Serial.print("  ✓ ACK recibido para comando ID ");
        Serial.print(frame->id);
        Serial.print(" después de ");
        Serial.print(millis() - pending[i].sent_time);
        Serial.println("ms");

        pending[i].in_use = false;
        return;
      }
    }
  }
}

void checkTimeouts() {
  unsigned long now = millis();

  for (int i = 0; i < MAX_PENDING_REQUESTS; i++) {
    if (!pending[i].in_use) continue;

    unsigned long elapsed = now - pending[i].sent_time;

    if (elapsed > ACK_TIMEOUT_MS) {
      if (pending[i].retry_count < MAX_RETRIES) {
        // Reintentar
        pending[i].retry_count++;
        pending[i].sent_time = now;

        Serial.print("  ⟲ Reintento ");
        Serial.print(pending[i].retry_count);
        Serial.print("/");
        Serial.print(MAX_RETRIES);
        Serial.print(" para comando ID ");
        Serial.println(pending[i].id);

        Serial.write(pending[i].frame, pending[i].frame_len);

      } else {
        // Falló permanentemente
        Serial.print("  ✗ Comando ID ");
        Serial.print(pending[i].id);
        Serial.println(" falló después de reintentos");

        pending[i].in_use = false;
      }
    }
  }
}

// ============= TX FUNCTIONS =============

void sendCommandWithRetry(const char *cmd) {
  // Buscar slot disponible
  int slot = -1;
  for (int i = 0; i < MAX_PENDING_REQUESTS; i++) {
    if (!pending[i].in_use) {
      slot = i;
      break;
    }
  }

  if (slot == -1) {
    Serial.println("ERROR: Cola de solicitudes llena");
    return;
  }

  uint16_t id = next_cmd_id++;
  size_t len = llp_build_frame(
    tx_buffer, sizeof(tx_buffer),
    LLP_COMMAND,
    id,
    (uint8_t*)cmd, strlen(cmd)
  );

  // Guardar en pending
  pending[slot].id = id;
  pending[slot].sent_time = millis();
  pending[slot].retry_count = 0;
  pending[slot].frame_len = len;
  pending[slot].in_use = true;
  memcpy(pending[slot].frame, tx_buffer, len);

  // Enviar inmediato
  Serial.print("[TX] Comando: \"");
  Serial.print(cmd);
  Serial.print("\" (ID ");
  Serial.print(id);
  Serial.println(")");

  Serial.write(tx_buffer, len);
}
