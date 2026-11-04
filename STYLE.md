# Coding style

The C code follows the Linux kernel coding style
(Documentation/process/coding-style.rst), adapted to a userspace project.
`clang-format` with the kernel's `.clang-format` (added in Step 3) is the
reference for layout.

## Layout
- Tabs for indentation (8 columns); lines up to 80 columns, longer only
  when breaking them hurts readability (long format strings, tables).
- Braces: opening brace on the same line, except for functions (next
  line); `else` on the closing-brace line.
- One statement per line; no braces around single statements unless the
  other branch has them.
- Spaces after keywords (`if (`, `for (`), none after function names.

## Names
- Public symbols: `mp_` prefix for functions and types
  (`mp_core_add_tc()`, `struct mp_core`), `MP_` for macros and enum
  constants (`MP_SCHED_OK`).
- `static` functions inside a file: short names without prefix
  (`resp_task()`), no leading underscore.
- Lower case with underscores; no typedefs for structs (`struct mp_task`,
  not `mp_task_t`), except opaque handles if any are introduced.
- Units in names or comments when not obvious: `t_us`, `bits`,
  `u` is a utilization in permill.

## Types
- `int` for counts, indices and times in microseconds (as today),
  `int64_t` when a product can exceed 2^31 (WCRT iteration, hyperperiods).
- `double` for analysis values (network bounds); `float` only where the
  C++ code uses it, so results stay identical (Steps 4–9).
- `bool` from `<stdbool.h>` for new flags; the existing `YES`/`NO`
  values are kept until Step 9, then replaced.

## Memory and errors
- Every dynamic array has `_init()`, `_push()`, `_copy()` and `_free()`;
  a structure that owns arrays has `_copy()` and `_free()` too.
- Functions that can fail return `0` or a negative error code
  (`-ENOMEM`, `-EINVAL`) and clean up with `goto` labels:
  `err_free_tcs:`.
- No `exit()` in library code: errors propagate to the caller (today's
  `exit(0)` on internal errors become error returns).
- No global mutable state in the library: everything lives in
  `struct mp_ctx` (needed for OpenMP, Step 10).

## Comments
- `/* */` only; multi-line comments in kernel form.
- kernel-doc on every public function: what it does, parameters, return
  value, ownership of returned memory.
- Each formula from the thesis cites it: `/* Eq. 3.3 */`, `/* Rule 3.7 */`.

## Floating point
- Built with `-ffp-contract=off`, never `-ffast-math`.

## Files
- SPDX header on the first line: `/* SPDX-License-Identifier: GPL-3.0-or-later */`
  (the repository licence).
- One module per `.c`/`.h` pair: `model`, `generator`, `partition`,
  `let`, `assign`, `sched`, `placement`, `comm`, `network`, `stats`,
  `output`, `rng`, `array`.
