# Especificación LLP Protocol v1.0

## Resumen Ejecutivo

LLP (Lightweight Link Protocol) es un protocolo de nivel de enlace (Layer 2 OSI) 
diseñado para ser ultra-liviano, robusto y extensible en microcontroladores.

## Estructura del Frame

```
 Offset │ Bytes │ Nombre    │ Descripción
────────┼───────┼───────────┼─────────────────────────────────
 0      │ 2     │ MAGIC     │ 0xAA 0x55 (Sincronización)
 2      │ 1     │ TYPE      │ Tipo de mensaje (0x00-0xFF)
 3      │ 2     │ ID        │ ID de transacción (Little Endian)
 5      │ 2     │ LENGTH    │ Tamaño payload (Little Endian)
 7      │ N     │ PAYLOAD   │ Datos (0-512 bytes)
 7+N    │ 2     │ CRC16     │ CRC16-CCITT (Little Endian)
```

## Campos Detallados

### MAGIC (2 bytes)
- **Propósito:** Sincronización y delimitación de frame
- **Valor:** 0xAA seguido de 0x55
- **Recuperación:** Si llega 0xAA cuando se espera 0x55, pero llega otro 0xAA, 
  volvemos a esperar 0x55 (evita falsos positivos en RF ruidoso)

### TYPE (1 byte)
- **Rango:** 0x00-0xFF
- **Definición:** Tipos base 0x01-0x15 en `llp_msg_type_t`
- **Custom:** Aplicaciones pueden usar 0x16-0xFF

### ID (2 bytes, Little Endian)
- **Propósito:** Correlacionar request-response
- **Formato:** LSB first, MSB second
- **Ejemplo:** ID=0x1234 → bytes [0x34, 0x12]
- **Uso:** Detectar frames duplicados o perdidos

### LENGTH (2 bytes, Little Endian)
- **Rango:** 0-512 (configurable en `#define LLP_MAX_PAYLOAD`)
- **Formato:** LSB first
- **Validación:** Rechaza si > LLP_MAX_PAYLOAD

### PAYLOAD (Variable, 0-512 bytes)
- **Estructura:** Completamente aplicación-dependiente
- **Validación:** Ninguna (responsabilidad de la aplicación)
- **Ejemplo:** ASCII, binario, struct, JSON, etc.

### CRC16 (2 bytes, Little Endian)
- **Algoritmo:** CRC16-CCITT (polinomio 0x1021)
- **Inicial:** 0xFFFF
- **Cobertura:** Magic + Type + ID + Length + Payload
- **Excluye:** El mismo CRC16
- **Formato:** LSB first

## Máquina de Estados del Parser

```
WAIT_MAGIC1 ─┐
  0xAA recib.│
      ├─────→ WAIT_MAGIC2 ─┐
      │       0x55 recib.  │
      │           ├────────→ READ_TYPE
      │           │           └→ READ_ID_L → READ_ID_H
      │           │               └→ READ_LEN_L → READ_LEN_H
      │           │                   ├─ Si len=0 → READ_CRC_L
      │           │                   └─ Si len>0 → READ_PAYLOAD
      │           │                       ├─→ READ_CRC_L
      │           │
      │      0xAA recib. (otro)
      │           └─ Quedarse en WAIT_MAGIC2 (robusto RF)
      │
  Otro byte
      └─────────→ Volver a WAIT_MAGIC1

READ_CRC_L → READ_CRC_H ─┐
     Validar CRC         │
     ├─ OK ──────────────→ Return 1 (Frame completo)
     └─ Error ──────────→ Return -1 + Volver a WAIT_MAGIC1
```

## Códigos de Error

```c
LLP_ERR_OK            = 0x00  // Sin error
LLP_ERR_CHECKSUM      = 0x01  // CRC no coincide
LLP_ERR_TYPE          = 0x02  // Tipo inválido (obsoleto en v1.0)
LLP_ERR_PAYLOAD_LEN   = 0x03  // Payload > LLP_MAX_PAYLOAD
LLP_ERR_TIMEOUT       = 0x04  // Timeout sin completar frame
LLP_ERR_SYNC          = 0x05  // Problema de sincronismo
LLP_ERR_BUFFER_FULL   = 0x06  // Buffer desbordado
```

## Patrones de Comunicación

### 1. Unidireccional (Fire-and-Forget)

```
Device A ──DATA──→ Device B
  (sin esperar ACK)
```

Ideal para telemetría pasiva donde no importa si se pierde un frame.

### 2. Request-Response

```
Device A ──COMMAND──→ Device B
Device A ←───ACK──── Device B
```

Ideal para configuración remota, consultas de estado.

### 3. Publish-Subscribe (vía coordinador)

```
Device A ──EVENT──→ Coordinador
Device B ←──EVENT─ Coordinador
```

Ideal para redes multi-dispositivo.

## Recomendaciones

### Para Enlace Confiable
1. Usar ID en frames
2. Implementar timeout + reintentos
3. Usar LLP_ACK/LLP_NACK explícitamente

### Para RF Ruidoso
1. Usar LLP_MAX_PAYLOAD pequeño (64-128B)
2. Implementar Forward Error Correction (Hamming) en payload si es crítico
3. Multi-intentos antes de dar por perdido

### Para Payload Grande (> 512B)
1. Fragmentar en múltiples frames
2. Usar campo ID con contador de fragmento en payload
3. Ejemplo:
   ```
   Payload: [FRAG_ID (2B)] [FRAG_NUM (1B)] [FRAG_TOTAL (1B)] [DATA (N bytes)]
   ```

## Ejemplos

### Enviar Datos Simples

```c
uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
llp_build_frame(buf, 520, LLP_DATA, 1, data, 4);
// Result: [0xAA, 0x55, 0x10, 0x01, 0x00, 0x04, 0x00, 
//          0xDE, 0xAD, 0xBE, 0xEF, CRC_L, CRC_H]
```

### Recibir y Procesar

```c
while (serial.available()) {
  int ret = llp_parser_process_byte(&parser, byte, millis());
  if (ret == 1) {
    // parser.frame.type
    // parser.frame.id
    // parser.frame.payload_len
    // parser.frame.payload[]
  }
}
```

---

**Documento:** Especificación técnica v1.0  
**Fecha:** 2026-03-30  
**Autor:** EnzoLeonel
