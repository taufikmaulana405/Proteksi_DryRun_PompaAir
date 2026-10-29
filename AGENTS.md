# Pump Controller — Agent Guide

Treat this as an owner-review draft based on confirmed project decisions. Keep it short; do not invent requirements, file paths, or test results.

## Scope and priorities

Maintain an Arduino Uno controller for a SAN-EI 3SEP2.5/11 pump filling a 1,500 L tank from a 16 m borehole with a 4-inch casing. Use the existing relay and FS300A. The tank float operates outside Arduino; its exact electrical/mechanical arrangement is unverified. No well-level sensor is integrated. A contactor purchase was declined for budget reasons.

Apply project priorities in this order: (1) reliable stop commands and truthful safety claims, (2) owner-approved behavior and calibration, (3) correct timing and measurements, (4) readable code, (5) optimization. Follow explicit current user instructions over this draft; apply nested guidance only within its scope. Ask about unresolved conflicts affecting pump operation; proceed with unrelated work.

## ALWAYS DO — five conventions

1. Read the current sketch before editing. Locate capabilities through symbols such as `startPump`, `stopPump`, `readAndClearPulses`, and `updateStatusLed`; do not assume a sketch filename. Keep changes focused on the request.
2. Use nonblocking `millis()` timing with unsigned subtraction: `now - startedAt >= duration`. Keep LED updates reachable before early returns.
3. Keep the interrupt handler limited to counting pulses. Read and clear the shared counter atomically. Calculate flow using the actual sampling interval.
4. Use named constants and ordinary `if`/`else`. Follow `remainingTime()` as the readability example. Preserve Indonesian comments and Serial messages unless asked otherwise.
5. Verify changed behavior and report exactly what was inspected, compiled, simulated, or physically tested. State missing tools or hardware explicitly.

## Confirmed behavior — preserve unless explicitly changed

| Capability | Baseline |
| --- | --- |
| Pins and polarity | FS300A signal D2, relay D8; HIGH = ON, LOW = OFF |
| Calibration | `FLOW_FACTOR = 5.5f`, confirmed by the owner; 330 pulses/L |
| Flow threshold | `MIN_FLOW_LPM = 7.0f`; equality qualifies as sufficient flow |
| Startup/reset | Set output OFF during initialization, then call `startPump()` immediately |
| STARTING | Allow 15,000 ms to detect sufficient flow, then stop if still insufficient |
| RUNNING | Stop after approximately 3,000 ms of consecutive low-flow samples; clear accumulated low time on a good sample |
| Cycle limit | 30 minutes from relay activation, including STARTING |
| RESTING | Relay OFF for 30 minutes, then retry indefinitely; no fault-count lockout |
| Sampling/reporting | Every 1,000 ms; Serial at 9600 baud |
| Debit display | `ALWAYS_SHOW_FLOW = false`: hide debit in RESTING only; continue sampling |
| Countdown/volume | Show every second in all states; retain volume during RESTING |
| Volume calculation | Add `pulses / (FLOW_FACTOR * 60.0f)` while STARTING or RUNNING, before processing a stop transition |
| Volume reset | Reset only when `startPump()` begins a new cycle; no persistence across power loss |
| Built-in LED | STARTING: 250 ms ON/OFF; RUNNING: steady ON; RESTING: 1,000 ms ON/OFF |

## ASK FIRST

Ask only when the current request does not already authorize the change:

- Change calibration, pins, polarity, thresholds, timing, startup behavior, or retry policy.
- Add hardware, dependencies, EEPROM persistence, or a new build framework.
- Upload firmware or perform an action that could energize the real pump.

## NEVER DO — five anti-patterns

1. Do not introduce `delay()`, blocking waits, or Serial output inside an interrupt handler.
2. Do not use ternary expressions for control logic; write explicit `if`/`else` branches.
3. Do not restore earlier defaults such as a 2 L/min threshold, 8-second startup, delayed initial start, or a three-fault lockout. Do not call the calibrated factor an unverified assumption.
4. Do not describe relay state, LED state, or zero flow as proof that the motor is electrically OFF. Do not claim flow-only protection prevents all dry running or that a generic relay is motor-rated.
5. Do not bypass protection, deliberately run the pump dry, or treat compilation as hardware validation. Reset currently causes an immediate restart; disclose this when relevant.

## Verification

Discover the actual sketch and installed toolchain first:

```sh
rg --files -g '*.ino' -g 'AGENTS.md' -g 'platformio.ini' -g 'arduino-cli.yaml'
arduino-cli version
arduino-cli core list
```

After locating the sketch, replace the placeholder with its existing directory:

```sh
arduino-cli compile --fqbn arduino:avr:uno --warnings all "<actual-sketch-directory>"
```

Do not invent a successful compile if the sketch, CLI, or AVR core is missing. Do not install dependencies without authorization. Compile firmware changes; documentation-only edits need a consistency review, not a firmware build.

For control-logic changes, check affected cases with simulated time/pulses or an isolated bench setup: startup with no pulses; sufficient flow; consecutive low samples; recovery clearing the low timer; 30-minute stop/retry; volume reset/retention; LED/reporting during RESTING; and `millis()` rollover. At this calibration, 330 pulses represent 1 L. Keep real pump power disconnected for bench checks; never shorten production timers merely to speed testing.

## Maintain this guide

Have the owner review this draft before adopting it. Add or revise a rule only after an observed mistake or a confirmed decision. Keep detailed procedures in separate documents only when needed, and link only to files that exist. Use nested AGENTS.md files for distinct subprojects without duplicating these rules. Do not generate a repository map or add unrelated application conventions.
