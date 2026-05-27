# Motor Mapping

Fill this table after `test_single_motor` validation.

| CAN Feedback ID | Motor ID | Physical Wheel | Positive Current Direction | Need Invert |
| --- | --- | --- | --- | --- |
| 0x201 | 1 | TBD | TBD | TBD |
| 0x202 | 2 | TBD | TBD | TBD |
| 0x203 | 3 | TBD | TBD | TBD |
| 0x204 | 4 | TBD | TBD | TBD |

Original STM32 chassis assumption:

```text
2\   /4
1/   \3
```

- 1 = rear-left
- 2 = front-left
- 3 = rear-right
- 4 = front-right
