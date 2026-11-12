# Coding style

The C code follows the Linux kernel coding style
(Documentation/process/coding-style.rst), adapted to a userspace project.

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
  (`mp_core_add_chain()`, `struct mp_core`), `MP_` for macros and enum
  constants (`MP_FFDU`).
- `static` functions inside a file: short names without prefix
  (`choose_core()`), no leading underscore.
- Lower case with underscores; no typedefs for structs (`struct mp_task`,
  not `mp_task_t`).
- Descriptive names: `wcet`, `period`, `response_time`, `priority`,
  `fragment_index`, `params.sigma`, `params.fill_limit` (phi). Single
  letters only for loop indices (`i`, `j`, `k`) and for the symbols of a
  formula written next to it.

## Comments
- A comment above every function (what and why), above every structure
  field, and before every step that is not obvious from the code.
- Units in comments: microseconds, ppm of a core, bits, percent.
- `/* */` only; multi-line comments in kernel form.
- kernel-doc on every public function: what it does, parameters, return
  value, ownership of returned memory.

## Types
- `int` for counts, indices, times in microseconds and utilizations in
  ppm; `int64_t` when a product can exceed 2^31 (response-time
  iteration, utilization products).
- `double` for analysis values (network bounds, slacks); `float` for the
  reported metrics.
- `bool` from `<stdbool.h>` for flags.

## Memory and errors
- Every dynamic array has `_push()`, `_copy()` and `_free()`; a structure
  that owns arrays has `_copy()` and `_free()` too.
- Functions that can fail return `0` or a negative error code
  (`-ENOMEM`, `-EINVAL`) and clean up with `goto` labels
  (`err_free_chains:`).
- No `exit()` in library code: errors propagate to the caller.
- No global mutable state in the library: everything lives in
  `struct mp_ctx`, so design points can run in parallel (OpenMP).

## Floating point
- Built with `-ffp-contract=off`, never `-ffast-math`: the golden results
  are compared to the last digit.

## Files
- SPDX header on the first line: `/* SPDX-License-Identifier: Apache-2.0 */`
  (`//` form in `.c` files, as in the kernel).
- One module per `.c`/`.h` pair: `model`, `generator`, `partition`,
  `let`, `assign`, `sched`, `placement`, `repair`, `communication`,
  `network`, `stats`, `pipeline`, `experiment`, `table`, `context`,
  `rng`, `array`.
