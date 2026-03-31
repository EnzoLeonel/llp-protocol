# Ejemplos de LLP Protocol

## minimal_uart/

**Descripción:** Ejemplo básico que muestra:
- Recepción y parseo de frames
- Respuesta con ACK
- Diferentes tipos de mensaje
- Estadísticas

**Hardware:** Arduino UNO + Serial Monitor  
**Velocidad:** 9600 baud

**Instrucciones:**
1. Abre `minimal_uart.ino` en Arduino IDE
2. Selecciona tu placa (Arduino UNO)
3. Carga el sketch
4. Abre Serial Monitor (9600 baud)
5. El dispositivo enviará PING cada 10 segundos

**Prueba:**
- El sketch envía PING automáticamente
- Procesa frames recibidos
- Responde con ACK

---

## request_response/

**Descripción:** Patrón request-response robusto con:
- Timeout automático
- Reintentos hasta 3 veces
- Cola de solicitudes pendientes
- Verificación de ACK

**Hardware:** 2 Arduino o Arduino + PC (Python)  
**Velocidad:** 9600 baud

**Instrucciones:**
1. Carga el sketch en primer Arduino
2. Carga el sketch en segundo Arduino (o usa Python)
3. Primer Arduino envía "HELLO" cada 5 segundos
4. Segundo Arduino responde con ACK

---

Más ejemplos próximamente (RF433, RS485, LoRa...)
