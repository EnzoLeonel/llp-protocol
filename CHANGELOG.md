# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-03-30

### Added
- Initial release of LLP Protocol
- Core parser with state machine
- CRC16-CCITT checksum validation
- Support for 0-512 byte payload
- Examples for Arduino UART
- Complete documentation and README

### Features
- RF-robust synchronization (consecutive magic bytes handling)
- Timeout protection (2 seconds configurable)
- Defensive NULL pointer validation
- Direct payload storage (optimized for small RAM)
- Statistics tracking (frames_ok, frames_error, timeouts)

---

## [Unreleased]

### Planned
- RS485 multi-node example
- LoRa integration example
- Advanced retransmission patterns
- Fragmentation support for large payloads
- Hardware CRC acceleration (STM32, etc)
