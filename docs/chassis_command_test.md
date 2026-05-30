# Chassis Command Test

Use this tool when the keyboard test does not receive keys through a remote
desktop, input method, or terminal focus issue.

Build first:

```bash
cd ~/projects/rk3588-chassis-control/build
cmake --build .
```

Run one movement at a time:

```bash
./test_chassis_command can0 forward 0.3 2000 500 400
./test_chassis_command can0 back 0.3 2000 500 400
./test_chassis_command can0 left 0.3 2000 500 400
./test_chassis_command can0 right 0.3 2000 500 400
./test_chassis_command can0 rotate-left 0.3 2000 500 400
./test_chassis_command can0 rotate-right 0.3 2000 500 400
```

Arguments:

```text
can0              SocketCAN interface
forward          direction
0.3              command scale, range 0.0 to 1.0
2000             duration in ms
500              max translation wheel rpm
400              max rotation wheel rpm
```

Supported directions:

```text
forward
back
left
right
rotate-left
rotate-right
stop
```

Safety checklist:

- Lift the chassis for the first test.
- Start with scale `0.2` or `0.3`.
- Keep one hand ready to press `Ctrl+C`.
- The program sends zero current when it exits.
- If the displayed `online` value becomes `0`, the program sends zero current.

Expected output:

```text
remain=1900ms online=1 cmd=[0.30 0.00 0.00] M1 tgt= -150 rpm= ...
```

If all target rpm values stay zero, check that the direction and scale arguments
are correct.
