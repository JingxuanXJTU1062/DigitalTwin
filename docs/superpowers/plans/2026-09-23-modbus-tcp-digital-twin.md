# F407—Python Modbus TCP Digital Twin Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the production `$FR/$CMD` TCP stream with standards-compliant Modbus TCP while preserving all J/C/P/D/I signal meanings, scaling, and legacy debug helpers.

**Architecture:** STM32F407 is a Modbus TCP server behind an ESP8266 AP/TCP byte gateway; the Python host is the Modbus TCP client. Sensor values are exposed through input registers and PE4/PE5/PE6 through coils. ESP8266 `+IPD` data is parsed by link and length, then reassembled by the MBAP length field.

**Tech Stack:** STM32F407 HAL/Keil ARMCC5 C, ESP8266 AT firmware, Python 3 standard library (`socket`, `struct`, `threading`, `unittest`).

**Spec:** `docs/superpowers/specs/2026-09-23-modbus-tcp-digital-twin-design.md`

## Global Constraints

- Production TCP endpoint is `192.168.4.1:502`, Unit ID `1`.
- Preserve the current J/C/P/D/I signal names, units, integer scaling, and legacy NMEA helper functions.
- Use Modbus TCP functions `0x01`, `0x04`, `0x05`, and `0x0F` with standard exception responses.
- UART sensor freshness timeout is 500 ms; coil command safety timeout is 1500 ms.
- No new Python package dependency is allowed.
- The workspace is not a Git repository; replace commit checkpoints with explicit file/diff and test checkpoints.

## Review Focus

- An MBAP frame containing zero bytes must survive USART/TCP forwarding without string truncation; covered in Tasks 2, 3, and 6.
- Fragmented and coalesced `+IPD`/Modbus data must be reassembled per link; covered in Task 3.
- Invalid Modbus address, quantity, byte count, and coil value must return the correct exception; covered in Task 2.
- Stale UART sensor values and stale actuator commands must transition to invalid/safe state; covered in Tasks 4 and 5.
- TCP reconnect must resume polling even after the link mask reached zero; covered in Tasks 5 and 6.

---

### Task 1: Establish protocol test harnesses

**Files:**
- Create: `WiFi/tests/test_modbus_client.py`
- Create: `WiFi/F407_04/tests/test_modbus_tcp_server.c`
- Create: `WiFi/F407_04/tests/test_esp8266_ipd.c`
- Create: `WiFi/F407_04/tests/test_protocol_freshness.c`

**Interfaces:**
- Consumes: register/coil layout from the approved design.
- Produces: executable expectations for Python framing and portable C protocol modules.

- [ ] **Step 1: Add Python tests that import the wished-for public API**

```python
from mcu_wifi_listener import (
    ModbusTcpClient, decode_signal_registers, build_mbap_request,
    INPUT_REGISTER_COUNT, UNIT_ID,
)

def test_decode_preserves_existing_scaling():
    regs = [0] * INPUT_REGISTER_COUNT
    regs[0], regs[2], regs[10], regs[13] = 0x0100, 0x1F, 1234, 567
    data = decode_signal_registers(regs)
    assert data["jinliao"]["omega1"] == 1.234
    assert data["chuliao"]["pressure_kpa"] == 5.67
```

- [ ] **Step 2: Run `python -m unittest discover -s WiFi/tests -v` and verify RED because the Modbus API does not exist**

- [ ] **Step 3: Add portable C tests for `ModbusTcp_ProcessAdu`, `Esp8266Ipd_PushByte`, and freshness APIs using fixed byte vectors**

- [ ] **Step 4: Compile the C tests and verify RED because the new headers/sources do not exist**

- [ ] **Step 5: Record the exact RED failures before writing production code**

### Task 2: Implement the portable F407 Modbus TCP server core

**Files:**
- Create: `WiFi/F407_04/User/Modbus/modbus_tcp_server.h`
- Create: `WiFi/F407_04/User/Modbus/modbus_tcp_server.c`
- Modify: `WiFi/F407_04/tests/test_modbus_tcp_server.c`

**Interfaces:**
- Consumes: a 30-register snapshot, three coil values, and a complete Modbus TCP ADU.
- Produces: `ModbusTcp_Init`, `ModbusTcp_SetInputRegisters`, `ModbusTcp_ProcessAdu`, `ModbusTcp_GetCoils`, `ModbusTcp_ApplySafetyTimeout`; response ADU length or zero for ignored Unit ID.

- [ ] **Step 1: Test a `0x04` request and exact MBAP/PDU response bytes, including Transaction ID echo**
- [ ] **Step 2: Test `0x01`, `0x05`, and `0x0F`, including PE coil callback invocation and readback**
- [ ] **Step 3: Test exceptions `0x01`, `0x02`, and `0x03`, malformed MBAP length, Protocol ID, Unit ID, and embedded zero bytes**
- [ ] **Step 4: Run the portable C test binary and verify the new tests are RED**
- [ ] **Step 5: Implement bounds-checked big-endian helpers and the four function codes without HAL dependencies**
- [ ] **Step 6: Run the C tests and verify GREEN with zero warnings**

### Task 3: Implement binary ESP8266 `+IPD` framing and reliable binary transmit

**Files:**
- Create: `WiFi/F407_04/User/ESP8266/esp8266_ipd.h`
- Create: `WiFi/F407_04/User/ESP8266/esp8266_ipd.c`
- Modify: `WiFi/F407_04/User/ESP8266/bsp_esp8266.h`
- Modify: `WiFi/F407_04/User/ESP8266/bsp_esp8266.c`
- Modify: `WiFi/F407_04/User/usart/bsp_debug_usart.c`
- Modify: `WiFi/F407_04/tests/test_esp8266_ipd.c`

**Interfaces:**
- Consumes: raw USART3 bytes containing AT responses and `+IPD,<id>,<len>:` chunks.
- Produces: `Esp8266Ipd_PushByte`, `Esp8266Ipd_PollAdu(link_id, adu, capacity)`, and `ESP8266_SendBytes(data, length, link_id)`.

- [ ] **Step 1: Test a header split across calls, payload split across `+IPD` chunks, two ADUs in one payload, and two interleaved link IDs**
- [ ] **Step 2: Test malformed/oversized headers and payloads containing `0x00`, `\r`, `\n`, and `$`**
- [ ] **Step 3: Run the IPD C test and verify RED**
- [ ] **Step 4: Implement a bounded byte state machine and one MBAP reassembly buffer per link**
- [ ] **Step 5: Add `ESP8266_SendBytes`; require a fresh `>` prompt, transmit exactly `length` bytes with HAL, and return false on timeout/error**
- [ ] **Step 6: NUL-terminate the AT text buffer after every receive update without passing binary IPD payload to string parsing**
- [ ] **Step 7: Run IPD and Modbus C tests and verify GREEN**

### Task 4: Add complete sensor freshness and register snapshot mapping

**Files:**
- Modify: `WiFi/F407_04/User/usart_protocol/usart_protocol.h`
- Modify: `WiFi/F407_04/User/usart_protocol/usart_protocol.c`
- Modify: `WiFi/F407_04/User/usart_protocol/sensor_frame.c`
- Modify: `WiFi/F407_04/tests/test_protocol_freshness.c`

**Interfaces:**
- Consumes: valid UART/I2C frames and current HAL tick.
- Produces: `Protocol_ExpireStaleData(now_ms, timeout_ms)` and `Protocol_BuildModbusInputRegisters(regs, count, sequence, command_fresh)`.

- [ ] **Step 1: Test each J/C/P/D/I valid bit and every register address/scaling value**
- [ ] **Step 2: Test that valid frames refresh timestamps and bad frames do not**
- [ ] **Step 3: Test 500 ms boundary behavior and data clearing for all five channels**
- [ ] **Step 4: Run tests and verify RED**
- [ ] **Step 5: Store `HAL_GetTick()` for J, C, P, and D and add the common expiry routine**
- [ ] **Step 6: Build the exact 30-register snapshot with saturating uint16 conversions and bitmaps**
- [ ] **Step 7: Run all portable C tests and verify GREEN**

### Task 5: Integrate Modbus request/response into F407 main and ESP8266 configuration

**Files:**
- Modify: `WiFi/F407_04/User/main.c`
- Modify: `WiFi/F407_04/User/ESP8266/bsp_esp8266_test.c`
- Modify: `WiFi/F407_04/User/ESP8266/bsp_esp8266_test.h`
- Modify: `WiFi/F407_04/Project/Fire_F407ZG.uvprojx`

**Interfaces:**
- Consumes: complete ADUs from the IPD module and 30-register snapshots.
- Produces: standards-compliant responses on the originating ESP8266 link and safe GPIO behavior.

- [ ] **Step 1: Add new Modbus/IPD sources and include paths to the Keil project**
- [ ] **Step 2: Configure TCP port 502 and execute `AT+CIPDINFO=0` after server setup**
- [ ] **Step 3: Initialize Modbus, process all pending ADUs in the main loop, update snapshots, and send binary responses**
- [ ] **Step 4: Map coil writes to PE4/PE5/PE6 and enforce the 1500 ms command watchdog**
- [ ] **Step 5: Remove production NMEA periodic sends while leaving NMEA functions compiled for legacy diagnostics**
- [ ] **Step 6: Move link scanning outside any zero-link early exit so reconnect remains possible**
- [ ] **Step 7: Build the Keil project and resolve every compiler/linker error and warning**

### Task 6: Convert the Python host runtime to a Modbus TCP client

**Files:**
- Modify: `WiFi/mcu_wifi_listener.py`
- Modify: `WiFi/tests/test_modbus_client.py`

**Interfaces:**
- Consumes: standard Modbus TCP responses from `192.168.4.1:502`.
- Produces: the same J/C/P/D/I dictionaries/display units, coil refresh/readback, statistics, and automatic reconnect.

- [ ] **Step 1: Test MBAP request construction and fragmented exact-length response reads**
- [ ] **Step 2: Test Transaction ID, Protocol ID, Unit ID, function, byte count, exception, and connection-close validation**
- [ ] **Step 3: Test register decoding produces the legacy signal names and scaling**
- [ ] **Step 4: Test `0x0F` coil refresh and `0x01` readback mismatch reporting**
- [ ] **Step 5: Run Python tests and verify RED**
- [ ] **Step 6: Implement the client using only `socket` and `struct`; retain legacy NMEA helper functions but remove them from the production loop**
- [ ] **Step 7: Poll input registers every 100 ms, refresh/read back coils every 500 ms, track sequence gaps, and reconnect on network/protocol failure**
- [ ] **Step 8: Run Python tests and syntax compilation and verify GREEN**

### Task 7: Update communication documentation

**Files:**
- Modify: `WiFi/README.md`
- Create: `WiFi/Modbus_TCP通信与信号映射报告.md`

**Interfaces:**
- Consumes: final constants and behavior from Tasks 2–6.
- Produces: competition-facing description of communication mode, protocol, register/coil map, timing, wiring, error behavior, and test evidence.

- [ ] **Step 1: Replace the production `$FR` topology with Modbus TCP Client/Server roles and port 502**
- [ ] **Step 2: Document every register and coil with address, type, scaling, direction, freshness, and physical endpoint**
- [ ] **Step 3: Mark `$FR/$CMD` explicitly as retained legacy diagnostic formats**
- [ ] **Step 4: Document software verification separately from required hardware acceptance tests**
- [ ] **Step 5: Search documentation for stale claims (`$FRAME`, port 8888, 250 ms, transparent mode) and correct them**

### Task 8: Full verification and review

**Files:**
- Verify all files named above.

**Interfaces:**
- Consumes: completed implementation.
- Produces: evidence-backed delivery report.

- [ ] **Step 1: Run the full Python unit-test suite and `python -m py_compile WiFi/mcu_wifi_listener.py`**
- [ ] **Step 2: Build and run every portable C test with warnings treated as errors**
- [ ] **Step 3: Perform a clean Keil rebuild and confirm zero errors and zero warnings**
- [ ] **Step 4: Inspect the final project file to confirm both new C sources are compiled**
- [ ] **Step 5: Review the implementation against every acceptance item in the design spec**
- [ ] **Step 6: Independently review high-risk framing, bounds, timeout, and GPIO code; fix any critical/important issue and rerun all verification**
- [ ] **Step 7: Report exact communication mode, format, mapping, automated evidence, and remaining hardware-only checks**
