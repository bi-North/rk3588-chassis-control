# CAN Wiring Checklist

## Basic Wiring

```text
USB-CAN CANH -> motor controller CANH
USB-CAN CANL -> motor controller CANL
USB-CAN GND  -> motor controller GND / battery negative
```

## Power

- The 3508 motor controller must have main power.
- The motor phase wires must be connected firmly.
- Lift the chassis before sending non-zero current.

## Termination

Measure CANH to CANL while the bus is powered off:

- About 60 ohm: ideal, two 120 ohm terminators are present.
- About 120 ohm: one terminator is present, often okay for short bench tests.
- Open circuit: no terminator.
- Very low resistance: likely short circuit.

## Expected Linux State

```bash
ip -details link show can0
```

Expected:

```text
state UP
can state ERROR-ACTIVE
bitrate 1000000
```
