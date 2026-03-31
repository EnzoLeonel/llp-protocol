# LLP Protocol - Lightweight Link Protocol

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Arduino](https://img.shields.io/badge/Arduino-Compatible-brightgreen)](https://www.arduino.cc/)
[![C Standard](https://img.shields.io/badge/C-99%20%2F%20ISO-blue)](https://en.wikipedia.org/wiki/C99)

Un protocolo de comunicación **liviano, robusto y extensible** para microcontroladores. 
Ideal para UART, RF (433MHz, LoRa), RS485, CAN y otros medios de comunicación con ruido o baja velocidad.

**Características:**
- 🎯 Ultra-liviano: ~500B de código, sin dependencias
- 🛡️ Robusto: CRC16-CCITT, sincronización anti-ruido, timeouts automáticos
- 🔧 Extensible: 256 tipos de mensaje customizables
- ⚡ Bidireccional: Soporta request-response y eventos asincrónicos
- 📦 Payload variable: 0-512 bytes (configurable)
- 🌐 Agnóstico del medio: UART, RF, RS485, Bluetooth, CAN, etc.
- 💾 Compatible: Arduino, ESP8266, STM32, AVR, PIC, C puro

---

## 📑 Tabla de Contenidos

- [Descripción General](#descripción-general)
- [Estructura del Frame](#estructura-del-frame)
- [Instalación](#instalación)
- [Uso Rápido](#uso-rápido)
- [API Completa](#api-completa)
- [Tipos de Mensaje](#tipos-de-mensaje)
- [Ejemplos](#ejemplos)
- [Configuración Avanzada](#configuración-avanzada)
- [Performance](#performance)
- [Preguntas Frecuentes](#preguntas-frecuentes)
- [Contribuciones](#contribuciones)
- [Licencia](#licencia)

---

## 📐 Estructura del Frame

```
Offset  │  Bytes  │  Campo      │  Descripción
─────────┼─────────┼─────────────┼──────────────────────────────────
0       │  2      │  MAGIC      │  0xAA 0x55 (sincronización)
2       │  1      │  TYPE       │  Tipo de mensaje (0x00-0xFF)
3       │  2      │  ID         │  ID de transacción (LE)
5       │  2      │  LENGTH     │  Longitud payload (LE)
7       │  N      │  PAYLOAD    │  Datos (0-512B)
7+N     │  2      │  CRC16      │  CRC16-CCITT (LE)
```

**Total:** 9 bytes overhead + payload

---

## 🚀 Instalación

### Para Arduino IDE:

1. **Opción A: Via ZIP**
   - Descarga [llp-protocol.zip](https://github.com/EnzoLeonel/llp-protocol/archive/main.zip)
   - Sketch → Incluir librería → Agregar librería desde ZIP
   - Selecciona el ZIP descargado

2. **Opción B: Vía Git**
   ```bash
   cd ~/Documents/Arduino/libraries
   git clone https://github.com/EnzoLeonel/llp-protocol.git
   ```

3. **Opción C: Manual**
   - Copia `include/llp_protocol.h` a tu proyecto

### Para C puro / Embedded:

```bash
git clone https://github.com/EnzoLeonel/llp-protocol.git
#include "llp_protocol.h"
```

---

## ⚡ Uso Rápido

### Inicializar Parser

```cpp
#include "llp_protocol.h"

llp_parser_t parser;
llp_parser_init(&parser);
```

### Procesar Bytes Entrantes

```cpp
void setup() {
  Serial.begin(9600);
  llp_parser_init(&parser);
}

void loop() {
  while (Serial.available()) {
    uint8_t byte = Serial.read();
    
    int result = llp_parser_process_byte(&parser, byte, millis());
    
    if (result == 1) {
      // ✅ Frame completo recibido
      handleFrame(&parser.frame);
      
    } else if (result == -1) {
      // ❌ Error (checksum, timeout, etc)
      Serial.print("Error: 0x");
      Serial.println(parser.error_code, HEX);
    }
  }
}

void handleFrame(llp_frame_t *frame) {
  Serial.print("Type: 0x");
  Serial.print(frame->type, HEX);
  Serial.print(" | ID: ");
  Serial.print(frame->id);
  Serial.print(" | Payload: ");
  
  for (int i = 0; i < frame->payload_len; i++) {
    Serial.write(frame->payload[i]);
  }
  Serial.println();
}
```

### Construir y Enviar Frame

```cpp
void sendData() {
  uint8_t buffer[520];
  uint8_t myData[] = {0xDE, 0xAD, 0xBE, 0xEF};
  
  // Construir frame
  size_t frameLen = llp_build_frame(
    buffer,                    // Buffer destino
    sizeof(buffer),            // Tamaño disponible
    LLP_DATA,                  // Tipo: datos
    123,                       // ID de transacción
    myData,                    // Payload
    4                          // Tamaño payload
  );

  // Enviar por UART
  Serial.write(buffer, frameLen);
}
```

---

## 🔌 API Completa

### Inicialización

```cpp
void llp_parser_init(llp_parser_t* parser);
```
Inicializa el parser a estado inicial. Debe llamarse una vez en `setup()`.

---

### Procesamiento de Bytes

```cpp
int llp_parser_process_byte(llp_parser_t* parser, 
                             uint8_t byte, 
                             unsigned long current_ms);
```

**Parámetros:**
- `parser`: Puntero al parser
- `byte`: Byte recibido
- `current_ms`: Timestamp actual (`millis()`)

**Retorna:**
- `1`: Frame completo recibido (leer en `parser.frame`)
- `0`: Frame incompleto, sigue esperando
- `-1`: Error en frame (leer `parser.error_code`)

---

### Construcción de Frame

```cpp
size_t llp_build_frame(uint8_t* out_buffer,
                       size_t out_buffer_size,
                       uint8_t type,
                       uint16_t id,
                       const uint8_t* payload,
                       uint16_t payload_len);
```

**Parámetros:**
- `out_buffer`: Buffer donde escribir el frame
- `out_buffer_size`: Tamaño disponible en buffer
- `type`: Tipo de mensaje (0x00-0xFF)
- `id`: ID de transacción (opcional)
- `payload`: Datos a enviar (puede ser NULL si len=0)
- `payload_len`: Longitud del payload

**Retorna:**
- Tamaño total del frame construido
- `0` si hay error (payload muy largo, buffer insuficiente, etc)

---

### Estadísticas

```cpp
void llp_get_stats(llp_parser_t* parser, 
                   uint32_t* frames_ok,
                   uint32_t* frames_error,
                   uint32_t* timeouts);
```

Obtiene contadores de frames recibidos, errores y timeouts.

```cpp
void llp_reset_stats(llp_parser_t* parser);
```

Reinicia los contadores a cero.

---

## 🏷️ Tipos de Mensaje

| Constante      | Valor | Propósito                                  |
|----------------|-------|------------------------------------------|
| `LLP_PING`     | 0x01  | Prueba de enlace (keep-alive)            |
| `LLP_ACK`      | 0x02  | Confirmación positiva (respuesta OK)     |
| `LLP_NACK`     | 0x03  | Confirmación negativa (respuesta error)  |
| `LLP_DATA`     | 0x10  | Datos genéricos (sensores, valores)      |
| `LLP_CONFIG`   | 0x11  | Parámetros y configuración remota        |
| `LLP_STATUS`   | 0x12  | Consulta/Reporte de estado del dispositivo|
| `LLP_COMMAND`  | 0x13  | Comando a ejecutar en dispositivo remoto |
| `LLP_EVENT`    | 0x14  | Evento asíncrono o externo               |
| `LLP_ERROR`    | 0x15  | Reporte de error específico              |

**Custom Types:** Usa valores 0x16-0xFF para tipos específicos de tu aplicación.

---

## 📚 Ejemplos

### Ejemplo 1: UART Simple (Arduino)

Ver [`examples/minimal_uart/minimal_uart.ino`](examples/minimal_uart/)

Envía un PING cada 5 segundos y procesa respuestas.

### Ejemplo 2: Request-Response

Ver [`examples/request_response/request_response.ino`](examples/request_response/)

Implementa patrón request-response con timeout y reintentos.

### Ejemplo 3: Multi-node RS485 (próximamente)

---

## ⚙️ Configuración Avanzada

### Ajustar Payload Máximo

Para **Arduino UNO/Nano (2KB RAM)**:
```cpp
#define LLP_MAX_PAYLOAD 64  // En lugar de 512
```

Para **ESP8266/STM32 (> 32KB RAM)**:
```cpp
#define LLP_MAX_PAYLOAD 1024  // Más payload
```

### Modificar Timeout

```cpp
#define LLP_FRAME_TIMEOUT_MS 5000  // Para RF lento o latente
```

### Magic Bytes Personalizados

```cpp
#define LLP_MAGIC_1 0xCC
#define LLP_MAGIC_2 0xDD
```

---

## 📊 Performance

### Consumo de Memoria

| Componente | Bytes |
|------------|-------|
| `llp_parser_t` | ~600-650 |
| `llp_frame_t` (con payload 512B) | ~520 |
| Código ejecutable | ~500 |
| **Total (mínimo)** | ~650 |

Para Arduino UNO (2KB RAM): ~30% de memoria disponible.

### Velocidad

- **Parsing:** ~50 µs por byte (Arduino UNO, 16 MHz)
- **CRC16:** ~15 µs por byte
- **Frame builder:** ~100 µs (típico)

### Throughput (UART 9600 baud)

```
8 bits/byte + 1 start + 1 stop = 10 bits/byte
9600 baud / 10 = 960 bytes/seg

Con 20B overhead + 100B payload = 120B/frame
960 / 120 ≈ 8 frames/segundo máximo
```

---

## 🛡️ Seguridad y Robustez

| Característica | Descripción |
|---|---|
| **CRC16-CCITT** | Detecta bit-flips, inversiones, ráfagas de error comunes en RF |
| **Sincronización RF** | Maneja magic bytes consecutivos para recuperación automática |
| **Timeout** | Descarta frames incompletos tras 2 segundos (configurable) |
| **Validación de tamaño** | Rechaza payloads > LLP_MAX_PAYLOAD |
| **Defensiva con NULL** | Valida punteros en llp_build_frame y llp_get_stats |

---

## ❓ Preguntas Frecuentes

**P: ¿Puedo usar esto en RF 433MHz?**  
R: Sí. El protocolo está diseñado para RF. Recomendamos implementar retransmisión automática con ACK para mayor confiabilidad.

**P: ¿Funciona en UART, I2C, SPI?**  
R: Funciona nativamente en UART. Para I2C/SPI, es viable pero menos natural (requiere polling).

**P: ¿Hay encriptación?**  
R: No incluida en el core. Puedes encriptar el payload antes de llamar `llp_build_frame()`.

**P: ¿Cómo detectar comandos perdidos?**  
R: Usa el campo `ID` para correlacionar. Si no recibes ACK en N milisegundos, retransmite.

**P: ¿Soporta fragmentación?**  
R: No automática. Para payloads > 512B, implementa en tu aplicación (e.g., ID + fragment counter en payload).

**P: ¿Consume mucho flash en Arduino?**  
R: ~500-700 bytes. Compatible con Arduino UNO/Nano.

---

## 🤝 Contribuciones

Las **contribuciones son bienvenidas**:

- 🐛 Reporta bugs en [Issues](https://github.com/EnzoLeonel/llp-protocol/issues)
- 💡 Propón mejoras
- 📝 Agrega ejemplos (RF, RS485, etc)
- ✏️ Mejora documentación

### Cómo contribuir:

1. Fork el repo
2. Crea una rama: `git checkout -b feature/mi-mejora`
3. Commit: `git commit -am 'Agrego soporte para XYZ'`
4. Push: `git push origin feature/mi-mejora`
5. Abre un Pull Request

---

## 📜 Licencia

MIT License - Ver [`LICENSE`](LICENSE)

Uso, modificación y distribución libres sin restricciones.

---

## ✍️ Autor

Creado por **EnzoLeonel** con la colaboración de amigos revisores.

---

## 📞 Contacto / Soporte

- 📧 [Crear Issue](https://github.com/EnzoLeonel/llp-protocol/issues)
- 💬 [Discussions](https://github.com/EnzoLeonel/llp-protocol/discussions)

---

**Last updated:** 2026-03-30  
**Version:** 1.0.0
