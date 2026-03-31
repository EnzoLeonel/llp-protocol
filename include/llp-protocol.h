#ifndef LLP_PROTOCOL_H
#define LLP_PROTOCOL_H

#include <stdint.h>
#include <string.h>

// ================= CONFIG =================
#define LLP_MAGIC_1 0xAA
#define LLP_MAGIC_2 0x55

#define LLP_HEADER_SIZE 7           // Magic(2) + Type(1) + ID(2) + Len(2)
#define LLP_MAX_PAYLOAD 512         // Ajustable según RAM disponible. Bajar a 64/128 para AVR (Arduino Uno/Nano)
#define LLP_FRAME_TIMEOUT_MS 2000   // Timeout para sincronismo

// ================= MESSAGE TYPES =================
// Base types. El usuario puede usar tipos hasta 0xFF en su aplicación.
typedef enum {
  LLP_PING    = 0x01,    // Prueba de enlace
  LLP_ACK     = 0x02,    // Confirmación positiva
  LLP_NACK    = 0x03,    // Confirmación negativa (error)
  LLP_DATA    = 0x10,    // Datos genéricos (sensores, control)
  LLP_CONFIG  = 0x11,    // Configuración
  LLP_STATUS  = 0x12,    // Estado del dispositivo
  LLP_COMMAND = 0x13,    // Comando a ejecutar
  LLP_EVENT   = 0x14,    // Evento reportado
  LLP_ERROR   = 0x15     // Reporte de error
} llp_msg_type_t;

// ================= ERROR CODES =================
typedef enum {
  LLP_ERR_OK = 0x00,
  LLP_ERR_CHECKSUM = 0x01,
  LLP_ERR_TYPE = 0x02,
  LLP_ERR_PAYLOAD_LEN = 0x03,
  LLP_ERR_TIMEOUT = 0x04,
  LLP_ERR_SYNC = 0x05,
  LLP_ERR_BUFFER_FULL = 0x06
} llp_error_t;

// ================= CRC16-CCITT =================
static inline uint16_t llp_crc16_update(uint16_t crc, uint8_t data) {
  crc ^= (uint16_t)data << 8;
  for (int i = 0; i < 8; i++) {
    if (crc & 0x8000)
      crc = (crc << 1) ^ 0x1021;
    else
      crc <<= 1;
  }
  return crc;
}

static inline uint16_t llp_crc16_buffer(const uint8_t* buf, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc = llp_crc16_update(crc, buf[i]);
  }
  return crc;
}

// ================= FRAME STRUCTURE =================
typedef struct {
  uint8_t type;                      // Tipo de mensaje
  uint16_t id;                       // ID de transacción (opcional)
  uint16_t payload_len;
  uint8_t payload[LLP_MAX_PAYLOAD];
  uint16_t crc;                      // CRC16 calculado
} llp_frame_t;

// ================= PARSER =================
typedef struct {
  // Estado interno
  enum {
    LLP_STATE_WAIT_MAGIC1,
    LLP_STATE_WAIT_MAGIC2,
    LLP_STATE_READ_TYPE,
    LLP_STATE_READ_ID_L,
    LLP_STATE_READ_ID_H,
    LLP_STATE_READ_LEN_L,
    LLP_STATE_READ_LEN_H,
    LLP_STATE_READ_PAYLOAD,
    LLP_STATE_READ_CRC_L,
    LLP_STATE_READ_CRC_H
  } state;

  uint8_t header_buf[LLP_HEADER_SIZE];
  
  uint16_t payload_idx;
  
  uint16_t crc_received;
  uint16_t crc_calculated;
  
  unsigned long last_byte_time;
  
  // Frame completado (Único buffer para ahorrar RAM)
  llp_frame_t frame;
  uint8_t error_code;
  
  // Estadísticas
  uint32_t frames_ok;
  uint32_t frames_error;
  uint32_t timeouts;
  
} llp_parser_t;

// ================= FUNCIONES PÚBLICAS =================

/**
 * Inicializa el parser
 */
static inline void llp_parser_init(llp_parser_t* parser) {
  parser->state = LLP_STATE_WAIT_MAGIC1;
  parser->payload_idx = 0;
  parser->error_code = LLP_ERR_OK;
  parser->frames_ok = 0;
  parser->frames_error = 0;
  parser->timeouts = 0;
}

/**
 * Procesa un byte recibido
 * Retorna: 1 si frame completo, 0 si sigue recibiendo, -1 si error
 */
static inline int llp_parser_process_byte(llp_parser_t* parser, uint8_t byte, 
                                          unsigned long current_ms) {
  
  // Timeout protection
  if (parser->state != LLP_STATE_WAIT_MAGIC1) {
    if (current_ms - parser->last_byte_time > LLP_FRAME_TIMEOUT_MS) {
      parser->error_code = LLP_ERR_TIMEOUT;
      parser->timeouts++;
      parser->state = LLP_STATE_WAIT_MAGIC1;
      return -1;
    }
  }
  parser->last_byte_time = current_ms;

  switch (parser->state) {
    
    case LLP_STATE_WAIT_MAGIC1:
      if (byte == LLP_MAGIC_1) {
        parser->header_buf[0] = byte;
        parser->state = LLP_STATE_WAIT_MAGIC2;
      }
      break;

    case LLP_STATE_WAIT_MAGIC2:
      if (byte == LLP_MAGIC_2) {
        parser->header_buf[1] = byte;
        parser->crc_calculated = 0xFFFF;
        parser->crc_calculated = llp_crc16_update(parser->crc_calculated, LLP_MAGIC_1);
        parser->crc_calculated = llp_crc16_update(parser->crc_calculated, LLP_MAGIC_2);
        parser->state = LLP_STATE_READ_TYPE;
      } else if (byte == LLP_MAGIC_1) {
        // CORRECCIÓN RF: Si recibimos otro MAGIC_1, nos quedamos en MAGIC_2
        // porque podríamos estar en medio de ruido seguido del preámbulo real.
        parser->state = LLP_STATE_WAIT_MAGIC2; 
      } else {
        parser->state = LLP_STATE_WAIT_MAGIC1;
      }
      break;

    case LLP_STATE_READ_TYPE:
      parser->header_buf[2] = byte;
      parser->crc_calculated = llp_crc16_update(parser->crc_calculated, byte);
      
      // NOTA: Eliminamos la restricción de validación de tipo para permitir
      // mensajes customizados > 0x15 en aplicaciones genéricas.
      parser->frame.type = byte;
      parser->state = LLP_STATE_READ_ID_L;
      break;

    case LLP_STATE_READ_ID_L:
      parser->header_buf[3] = byte;
      parser->crc_calculated = llp_crc16_update(parser->crc_calculated, byte);
      parser->state = LLP_STATE_READ_ID_H;
      break;

    case LLP_STATE_READ_ID_H:
      parser->header_buf[4] = byte;
      parser->crc_calculated = llp_crc16_update(parser->crc_calculated, byte);
      parser->frame.id = parser->header_buf[3] | (parser->header_buf[4] << 8);
      parser->state = LLP_STATE_READ_LEN_L;
      break;

    case LLP_STATE_READ_LEN_L:
      parser->header_buf[5] = byte;
      parser->crc_calculated = llp_crc16_update(parser->crc_calculated, byte);
      parser->state = LLP_STATE_READ_LEN_H;
      break;

    case LLP_STATE_READ_LEN_H:
      parser->header_buf[6] = byte;
      parser->crc_calculated = llp_crc16_update(parser->crc_calculated, byte);
      parser->frame.payload_len = parser->header_buf[5] | (parser->header_buf[6] << 8);
      
      // Validar tamaño
      if (parser->frame.payload_len > LLP_MAX_PAYLOAD) {
        parser->error_code = LLP_ERR_PAYLOAD_LEN;
        parser->frames_error++;
        parser->state = LLP_STATE_WAIT_MAGIC1;
        return -1;
      }
      
      parser->payload_idx = 0;
      
      // Si payload es 0, pasar a lectura de CRC
      if (parser->frame.payload_len == 0) {
        parser->state = LLP_STATE_READ_CRC_L;
      } else {
        parser->state = LLP_STATE_READ_PAYLOAD;
      }
      break;

    case LLP_STATE_READ_PAYLOAD:
      // Guardamos directo en el frame final, evitando copias.
      parser->frame.payload[parser->payload_idx] = byte;
      parser->crc_calculated = llp_crc16_update(parser->crc_calculated, byte);
      parser->payload_idx++;
      
      if (parser->payload_idx == parser->frame.payload_len) {
        parser->state = LLP_STATE_READ_CRC_L;
      }
      break;

    case LLP_STATE_READ_CRC_L:
      parser->crc_received = byte;
      parser->state = LLP_STATE_READ_CRC_H;
      break;

    case LLP_STATE_READ_CRC_H:
      parser->crc_received |= (byte << 8);
      
      // Validar CRC
      if (parser->crc_received != parser->crc_calculated) {
        parser->error_code = LLP_ERR_CHECKSUM;
        parser->frames_error++;
        parser->state = LLP_STATE_WAIT_MAGIC1;
        return -1;
      }
      
      parser->frame.crc = parser->crc_calculated;
      parser->frames_ok++;
      
      parser->state = LLP_STATE_WAIT_MAGIC1; 
      
      return 1; // Frame validado

    default:
      parser->state = LLP_STATE_WAIT_MAGIC1;
      break;
  }

  return 0;  // Sigue recibiendo
}

// ================= FRAME BUILDER =================

/**
 * Construye un frame en un buffer
 * Retorna: tamaño del frame construido, 0 si error
 */
static inline size_t llp_build_frame(
  uint8_t* out_buffer,
  size_t out_buffer_size,
  uint8_t type,
  uint16_t id,
  const uint8_t* payload,
  uint16_t payload_len
) {
  // Validaciones
  if (payload_len > LLP_MAX_PAYLOAD) return 0;
  if (out_buffer_size < LLP_HEADER_SIZE + payload_len + 2) return 0;

  size_t idx = 0;

  // Magic
  out_buffer[idx++] = LLP_MAGIC_1;
  out_buffer[idx++] = LLP_MAGIC_2;

  // Type
  out_buffer[idx++] = type;

  // ID (Little Endian)
  out_buffer[idx++] = (id & 0xFF);
  out_buffer[idx++] = ((id >> 8) & 0xFF);

  // Length (Little Endian)
  out_buffer[idx++] = (payload_len & 0xFF);
  out_buffer[idx++] = ((payload_len >> 8) & 0xFF);

  // Payload
  if (payload_len > 0 && payload != NULL) {
    memcpy(&out_buffer[idx], payload, payload_len);
    idx += payload_len;
  }

  // Calcular CRC16 (sobre todo menos el CRC mismo)
  uint16_t crc = llp_crc16_buffer(out_buffer, idx);

  // CRC (Little Endian)
  out_buffer[idx++] = (crc & 0xFF);
  out_buffer[idx++] = ((crc >> 8) & 0xFF);

  return idx;
}

// ================= HELPERS =================

/**
 * Obtiene estadísticas del parser
 */
static inline void llp_get_stats(llp_parser_t* parser, 
                                  uint32_t* frames_ok,
                                  uint32_t* frames_error,
                                  uint32_t* timeouts) {
  if(frames_ok) *frames_ok = parser->frames_ok;
  if(frames_error) *frames_error = parser->frames_error;
  if(timeouts) *timeouts = parser->timeouts;
}

/**
 * Reinicia las estadísticas
 */
static inline void llp_reset_stats(llp_parser_t* parser) {
  parser->frames_ok = 0;
  parser->frames_error = 0;
  parser->timeouts = 0;
}

#endif
