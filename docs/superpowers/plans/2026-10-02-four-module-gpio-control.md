# Four-Module GPIO Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expand host control to four flags and make the feed, descaling, gutting, and discharge firmware obey persistent GPIO levels with safe low-level shutdown.

**Architecture:** The Python host writes four Modbus coils to the F407, which exposes them as PE4, PE5, PE6, and PC2. Each original-machine board polls PA6 and applies start/stop only on state changes; the discharge board gates its pressure workflow with Flag4 and sends `MOTOR_START`/`MOTOR_STOP` to the gutting board over USART2.

**Tech Stack:** Python 3 standard library, STM32F407 HAL/Keil C, STM32F103 Standard Peripheral Library/Keil C, PowerShell contract tests.

**Spec:** `docs/superpowers/specs/2026-10-02-four-module-gpio-control-design.md`

## Global Constraints

- Flag order is exactly feed, descaling, gutting, discharge.
- F407 GPIO mapping is exactly PE4, PE5, PE6, PC2; PE7 must remain assigned to FMC.
- Every original-machine control input is PA6 and requires common ground with the F407.
- GPIO low is the safe state; startup, explicit zero, and F407 command timeout must produce low outputs.
- Preserve all existing motor speeds, directions, acceleration values, pressure threshold `0.035 V`, position `25000` pulses, and command ordering.
- Stop Emm/X motors with immediate-stop commands while keeping drivers enabled; stop DC motors with zero PWM.
- Flag4 low overrides pressure and must cancel any pending discharge start.
- Do not change sensor telemetry formats or the five newly arranged sensor-receiver projects under `xiaweiji`.

## Review Focus

- A stale Modbus command must clear all four outputs, including PC2; covered in Task 2.
- A repeated unchanged GPIO level must not resend motor commands; covered in Tasks 3–5.
- PA6 dropping during a multi-motor start sequence must abort remaining starts and stop the whole module; covered in Tasks 3 and 4.
- Flag4 dropping during the two-second pressure delay must prevent `MOTOR_START` and send `MOTOR_STOP`; covered in Task 5.
- `MOTOR_START` and `MOTOR_STOP` prefix matching must not misclassify malformed USART2 frames; covered in Task 5.

---

### Task 1: Expand the Python host to four flags

**Files:**
- Create: `WiFi/tests/test_four_flag_control.py`
- Modify: `WiFi/mcu_wifi_listener.py`

**Interfaces:**
- Consumes: four host-owned boolean constants `FLAG1` through `FLAG4`.
- Produces: `build_cmd_packet(f1, f2, f3, f4) -> bytes`, four-coil Modbus writes, and four-coil readback.

- [ ] **Step 1: Write failing Python tests**

Add tests named `test_build_cmd_packet_contains_four_ordered_flags`, `test_default_coil_read_quantity_is_four`, and `test_four_coils_pack_into_low_nibble`. Assert the legacy body is exactly `CMD,1,0,1,0`, the XOR is correct, and the Modbus request quantity is four.

- [ ] **Step 2: Run tests and verify RED**

Run: `python -m unittest discover -s WiFi/tests -p "test_four_flag_control.py" -v`

Expected: failures because `build_cmd_packet` accepts three arguments and `read_coils` defaults to three.

- [ ] **Step 3: Implement the minimal four-flag host change**

Add `FLAG4`, change every production write/read/diagnostic path from three flags to four, and update the legacy packet builder signature and text. Validate configured flags as booleans rather than silently accepting arbitrary integers.

- [ ] **Step 4: Run the focused and full Python suites**

Run: `python -m unittest discover -s WiFi/tests -v` and `python -m py_compile WiFi/mcu_wifi_listener.py`.

Expected: all tests pass and syntax compilation exits zero.

- [ ] **Step 5: Commit**

```powershell
git add WiFi/mcu_wifi_listener.py WiFi/tests/test_four_flag_control.py
git commit -m "feat: send four module control flags"
```

### Task 2: Expand F407 Modbus, compatibility parser, and GPIO outputs

**Files:**
- Modify: `WiFi/F407_04/User/Modbus/modbus_tcp_server.h`
- Modify: `WiFi/F407_04/User/ESP8266/wifi_cmd.h`
- Modify: `WiFi/F407_04/User/ESP8266/wifi_cmd.c`
- Modify: `WiFi/F407_04/tests/test_modbus_tcp_server.c`
- Create: `WiFi/F407_04/tests/test_four_flag_gpio_contract.ps1`

**Interfaces:**
- Consumes: Modbus coil indices `0..3` or legacy `$CMD,f1,f2,f3,f4*CS`.
- Produces: PE4/PE5/PE6/PC2 output levels and Watch variables `g_cmd_flag1..g_cmd_flag4`.

- [ ] **Step 1: Extend portable Modbus tests and verify RED**

Test four-coil `0x0F` write/readback, coil 3 single-write, address 4 rejection, and timeout clearing all four callback values. Run the existing portable C test command used by the project and confirm failures while `MODBUS_COIL_COUNT` remains three.

- [ ] **Step 2: Add a failing GPIO/parser contract test**

The PowerShell test must require PC2/Flag4 definitions, four-field parsing with exact `0|1` validation, startup clearing of four outputs, and no use of PE7. Run it and confirm RED.

- [ ] **Step 3: Implement four Modbus coils**

Set `MODBUS_COIL_COUNT` to `4U`; preserve existing bounds, function codes, callback behavior, and timeout logic.

- [ ] **Step 4: Implement the fourth GPIO and strict legacy parsing**

Keep flags 1–3 on GPIOE and initialize PC2 separately on GPIOC. Extend apply/set APIs, Watch variables, comments, and `$CMD` parsing to four fields. Reject extra fields and values other than zero or one without changing outputs.

- [ ] **Step 5: Run focused and regression tests**

Run the portable Modbus test, `WiFi/F407_04/tests/test_four_flag_gpio_contract.ps1`, and all existing tests under `WiFi/F407_04/tests`.

Expected: all pass with no warnings from the portable C compile.

- [ ] **Step 6: Commit**

```powershell
git add WiFi/F407_04/User/Modbus WiFi/F407_04/User/ESP8266/wifi_cmd.* WiFi/F407_04/tests
git commit -m "feat: expose fourth module control output"
```

### Task 3: Convert the feed module from rising-edge touch to level control

**Files:**
- Modify: `原机代码/1_进料/Hardware/TTP223.c`
- Modify: `原机代码/1_进料/Hardware/inc/TTP223.h`
- Modify: `原机代码/1_进料/APP/main.c`
- Create: `tests/test_original_machine_gpio_control.ps1`

**Interfaces:**
- Consumes: `TTP223_ReadState()` as the current PA6 level.
- Produces: feed start/stop functions and a last-applied state that prevents repeated commands.

- [ ] **Step 1: Write the failing feed contract tests**

Require PA6 pulldown input without EXTI/NVIC setup, removal of touch-flag gating, exact original three start commands, exact three immediate-stop commands, and level-change guarding.

- [ ] **Step 2: Run the contract test and verify RED**

Run: `powershell -ExecutionPolicy Bypass -File tests/test_original_machine_gpio_control.ps1`

Expected: failure because feed still uses `TTP223_GetTouchFlag()` and has no stop path.

- [ ] **Step 3: Simplify PA6 initialization and implement feed level handling**

Keep the public read API, remove EXTI state/handler dependencies, extract focused module start/stop functions, and initialize the last-applied state so a boot-high input starts once while boot-low stays stopped.

- [ ] **Step 4: Guard the multi-command start sequence**

Re-read PA6 between commands. If it becomes low, stop all feed motors and return without issuing remaining start commands.

- [ ] **Step 5: Run the contract test and inspect the Keil project build**

Expected: contract passes; the `1_进料` build completes with zero errors if the local Keil command-line compiler is available.

- [ ] **Step 6: Commit**

```powershell
git add 原机代码/1_进料 tests/test_original_machine_gpio_control.ps1
git commit -m "feat: control feed module by GPIO level"
```

### Task 4: Add level-controlled start and stop to descaling and gutting

**Files:**
- Modify: `原机代码/2_去鳞/Hardware/TTP223.c`
- Modify: `原机代码/2_去鳞/Hardware/inc/TTP223.h`
- Modify: `原机代码/2_去鳞/APP/main.c`
- Modify: `原机代码/4_开膛去内脏/Hardware/TTP223.c`
- Modify: `原机代码/4_开膛去内脏/Hardware/inc/TTP223.h`
- Modify: `原机代码/4_开膛去内脏/APP/main.c`
- Modify: `tests/test_original_machine_gpio_control.ps1`

**Interfaces:**
- Consumes: persistent PA6 level on each board.
- Produces: one-shot state transitions for five X plus two DC motors in descaling, and three X plus one DC motor in gutting.

- [ ] **Step 1: Extend contract tests and verify RED**

Require exact original start parameters, X immediate-stop calls for every active address, zero PWM for every active DC channel, no touch-event API calls, and PA6 checks between every delayed start command.

- [ ] **Step 2: Implement descaling level control**

Convert PA6 to ordinary pulldown input, add start/stop functions, preserve the original command order, and abort safely if PA6 drops during startup.

- [ ] **Step 3: Run the contract test and verify the descaling section is GREEN**

- [ ] **Step 4: Implement gutting level control**

Convert PA6 to ordinary pulldown input and add independent X/DC start/stop state handling. Do not couple Flag3 to the USART2-controlled Emm address 1.

- [ ] **Step 5: Run contract tests and build both Keil projects**

Expected: contract passes; `2_去鳞` and `4_开膛去内脏` build with zero errors when Keil is available.

- [ ] **Step 6: Commit**

```powershell
git add 原机代码/2_去鳞 原机代码/4_开膛去内脏 tests/test_original_machine_gpio_control.ps1
git commit -m "feat: add GPIO level control to processing modules"
```

### Task 5: Gate discharge pressure control and add the stop command

**Files:**
- Modify: `原机代码/5_出料/Hardware/TTP223.c`
- Modify: `原机代码/5_出料/Hardware/inc/TTP223.h`
- Modify: `原机代码/5_出料/APP/main.c`
- Modify: `原机代码/4_开膛去内脏/APP/main.c`
- Modify: `tests/test_original_machine_gpio_control.ps1`

**Interfaces:**
- Consumes: discharge PA6 level, pressure voltage, and USART2 command frames.
- Produces: cancellable 20-by-100-ms pressure countdown plus exact `MOTOR_START\r\n` and `MOTOR_STOP\r\n` messages; gutting Emm start/stop actions.

- [ ] **Step 1: Add failing discharge and USART command tests**

Require removal of `delay_ms(2000)`, a 20-step cooperative countdown, cancellation on PA6 low within one loop, one stop message per high-to-low transition, start only while PA6 remains high, pressure re-arm at or above 0.035 V, and exact full command matching on the receiver.

- [ ] **Step 2: Run the contract test and verify RED**

Expected: failures because discharge uses blocking `delay_ms(2000)` and gutting only handles `MOTOR_START`.

- [ ] **Step 3: Implement discharge PA6 initialization and state machine**

Represent idle, waiting, and started states with a 20-iteration countdown driven by the existing 100 ms main-loop delay. On low, cancel waiting and send one stop command; on high plus pressure-low, set the countdown to 20; when it reaches zero, re-check high before sending start.

- [ ] **Step 4: Implement strict start/stop reception on the gutting board**

Accept only complete `MOTOR_START` or `MOTOR_STOP` frames after trimming CR/LF according to the existing receive length. Start with the existing position command and stop with `Emm_V5_Stop_Now(1, 0)`.

- [ ] **Step 5: Run contract tests and build both Keil projects**

Expected: all contracts pass; `5_出料` and `4_开膛去内脏` build with zero errors when Keil is available.

- [ ] **Step 6: Commit**

```powershell
git add 原机代码/5_出料 原机代码/4_开膛去内脏 tests/test_original_machine_gpio_control.ps1
git commit -m "feat: gate discharge conveyor with fourth flag"
```

### Task 6: Update wiring/protocol documentation and run full verification

**Files:**
- Modify: `上位机与5个下位机引脚清单.md`
- Modify: `原机代码/原机代码功能与外设引脚总结.md`
- Modify: `docs/superpowers/specs/2026-10-02-four-module-gpio-control-design.md` only if implementation evidence reveals a necessary correction.

**Interfaces:**
- Consumes: final verified pin assignments and behavior.
- Produces: one consistent operator-facing wiring and control description.

- [ ] **Step 1: Update documentation**

Document the four-bit order, PE4/PE5/PE6/PC2 outputs, PA6 inputs, common-ground requirement, high-run/low-stop behavior, Flag4 pressure gate, and `MOTOR_STOP` link.

- [ ] **Step 2: Search for stale three-flag claims**

Run targeted `rg` searches for `f1,f2,f3`, `0..2`, `3 个 GPIO`, `MODBUS_COIL_COUNT 3`, and old TTP223 edge-trigger descriptions; resolve every production/documentation hit or mark intentional legacy history.

- [ ] **Step 3: Run every automated suite**

Run the full Python suite, all portable F407 C tests, all PowerShell contract tests in `tests`, and Python syntax compilation. Record exact pass counts.

- [ ] **Step 4: Build all affected embedded projects**

Build `WiFi/F407_04`, `原机代码/1_进料`, `原机代码/2_去鳞`, `原机代码/4_开膛去内脏`, and `原机代码/5_出料`. If Keil is unavailable, record that limitation and retain the static contract evidence.

- [ ] **Step 5: Review the final diff against the approved spec**

Check pin safety, command bounds, startup states, timeout behavior, motor address coverage, and all exact retained mechanical parameters.

- [ ] **Step 6: Commit**

```powershell
git add 上位机与5个下位机引脚清单.md 原机代码/原机代码功能与外设引脚总结.md
git commit -m "docs: document four-module GPIO control"
```
