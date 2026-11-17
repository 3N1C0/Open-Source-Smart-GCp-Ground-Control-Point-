# Contributing to SmartGCP

Thanks for your interest in the project. SmartGCP is at an early stage, so
field reports and small fixes are as valuable as new features.

## Ways to help

- **Report a bug.** Open an issue with your board type, ESP32 Arduino core
  version, firmware version (`FW_VERSION` in `config.h`), and the serial output
  from power-up, including the `[TEST]` and `[BENCH]` lines.
- **Share test results.** Benchmark logs and PPK accuracy comparisons against
  known points are especially useful right now.
- **Suggest a change.** Open an issue to discuss larger ideas before writing
  code or changing hardware, so effort is not wasted.
- **Improve the docs.** Corrections and clearer explanations are always welcome.

## Pull requests

1. Fork the repository and create a branch from `main`.
2. Keep each pull request focused on one change.
3. For firmware changes, confirm the sketch compiles for at least one
   supported board and say which board you tested on.
4. Update the relevant README or doc when behaviour changes.
5. Describe what changed and why in the pull request.

## Firmware guidelines

- Settings a builder may need to change belong in `config.h`.
- Do not add external library dependencies without discussing it first. The
  firmware currently builds with the stock ESP32 Arduino core alone.
- Match the existing style: two-space indentation and short comments that
  explain why, not what.
- The GNSS logging path must never block. Keep slow work out of `gnssTask`.

## Hardware guidelines

- Commit editable source files, plus exported formats (STEP, STL, PDF
  schematic, Gerbers) so people without the original tool can use them.
- Note the tool and version used to create the files.

## Licensing of contributions

By contributing, you agree that your contributions are licensed under the same
terms as the part of the project you are changing: MIT for `firmware/`,
CERN-OHL-P-2.0 for `cad/` and `pcb/`, and CC BY 4.0 for documentation.
