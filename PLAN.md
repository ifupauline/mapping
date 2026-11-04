# mapping – C port and methodology upgrade

**Goal.** A system-level methodology that estimates the computational
resources an automotive application needs on a zonal E/E architecture
(cores per controller, network load, timing margins), implemented in C11
and evaluated by fast, reproducible experiments; then the thesis
(M. Gauthier, ISAE-SUPAERO, 2024) rewritten to match.

Repository: https://github.com/ifupauline/mapping (private).
This file is the source of truth; a copy lives in the claude.ai project
(`claude/mapping-c/PLAN.md`).

## Decisions (Nov 4, 2026)

| Topic | Decision |
|---|---|
| Language | C11, no C++ left after Step 9 |
| Style | Linux kernel coding style, adapted to userspace (STYLE.md) |
| Symbol prefix | `mp_` (functions, types), `MP_` (macros, enum constants) |
| Build | CMake; GCC and Clang on Linux |
| Dependencies | `libm` and OpenMP only |
| Repository | same repository; the C code replaces the C++ code |
| Workflow | one branch and one pull request per step; the owner reviews and merges; nothing pushed directly to `main` |
| Strictness | the C port reproduces the C++ results **exactly** for the same seed (Steps 4–9 are checked against golden results) |
| Order | port first, then improve: methodology changes are made in C only after the port is proven equivalent |
| Commit author | ifupauline <ifupauline21@gmail.com>, no co-author lines |

## Principles

1. **Equivalent before better.** Until Step 9 the C code must give the
   same results as the C++ code; any difference is a bug of the port.
2. **Verified.** Every step ends with its tests passing: golden results,
   invariant checker, sanitizers (and the network simulator from Step 8).
3. **Reproducible.** One seed gives one result, whatever the number of
   threads.
4. **Fast.** Every experiment runs in under 10 minutes on a workstation
   with its default settings.
5. **Traceable to the thesis.** Each experiment states which claim of the
   thesis it supports.

## Phase 0 – Foundations

### Step 1. Conventions and plan
PLAN.md, STYLE.md, CLAUDE.md.

### Step 2. Deterministic C++ and golden results
- One seeded random generator (splitmix64 seeding xoshiro256**) with an
  own bounded-integer function, written so that C++ and C produce the
  same sequence (`std::uniform_int_distribution` is
  implementation-defined, so it is not used).
- Every sort gets a total order (full tie-break): `qsort` and `std::sort`
  order equal elements differently.
- `std::map` iterations replaced by sorted keys.
- `-ffp-contract=off` so floating-point results do not depend on fused
  multiply-add.
- `tools/golden`: for a fixed set of seeds × sigma × algorithm, dump
  partitions, assignment, priorities and response times, placement,
  paths and network bounds into text files committed under
  `tests/golden/`.
- The invariant checker and the network simulator move into `tests/`.

### Step 3. C building blocks and test pipeline
- Growable arrays for tasks, task-chains and cores with explicit
  `_copy()` and `_free()` (the C++ code copies whole cores implicitly).
- `struct mp_ctx` holding all state that is global today (`STATE`, PA
  counters, generator counter) plus the per-instance random generator.
- Comparators with a total order; error handling with negative error
  codes and `goto` cleanup.
- CMake skeleton, `ctest`, AddressSanitizer + UBSan and Valgrind targets,
  `.clang-format` (kernel), GitHub Actions on pull requests (GCC, Clang,
  sanitizers).

## Phase 1 – Port, module by module

Each step ports one part, links it with the remaining C++ through a thin
`extern "C"` boundary, and must reproduce the golden results.

### Step 4. Model, generator, partitioning
### Step 5. LET and assignment (BFDU, WFDU, FFDU)
### Step 6. Scheduling (WCRT, priority assignment)
### Step 7. Placement (swapping, displacement)
### Step 8. Communication and TSN network
### Step 9. Drivers, statistics, output; C++ removed
All experiment programs in C; golden results, invariants and sanitizers
green; the C++ sources deleted.

## Phase 2 – Methodology improvements (in C)

### Step 10. Parallel experiment engine
OpenMP over independent design points; per-thread CPU timers
(`CLOCK_THREAD_CPUTIME_ID`; `clock()` sums all threads); command line
`--seed --instances --jobs --out --verbose`; quiet library output;
progress line with ETA; one raw CSV row per instance and algorithm.
Results identical for any number of threads.

### Step 11. LET-aware end-to-end latency
Add the LET sampling delays on both sides of every hop (write at the
producer core's LET instant, read at the consumer core's), next to the
network bound; reaction time and data age.

### Step 12. Timing as design constraints
End-to-end deadline per task-chain and bandwidth limit per link,
enforced during assignment and placement (not only reported); chains
violating their deadline count as unschedulable.

### Step 13. Sensitivity and per-controller sizing
Critical WCET scaling factor of each mapping (how much the WCETs can grow
before one more core is needed); cores, load and network load reported
per controller (PCU, ZCU1..5).

### Step 14. Experiment redesign
| Experiment | Content | Thesis claim |
|---|---|---|
| sweep | sigma × alpha, N instances (replaces Exp 1 and 2); feasibility rate | allocation, FR, M, SR, CC |
| scaling | run time vs number of chains up to 5000, fitted exponent per phase (replaces Exp 3 and 4) | efficiency, complexity |
| pa | PA1 vs PA2 vs optimal (Audsley with precedence) | priority assignment |
| waters | WATERS 2019 at phi = 1 and 0.8 (Tables 4.6, 4.7) | real use case |
| network | inter-ECU traffic, delay bounds, 100 Mbit/s vs 1 Gbit/s, message sizes | communication |
| let | LET cost 35 us vs 200 us | LET overhead |
| margins | deadlines and WCET growth margins per controller | resource estimation |
Each with a run-time budget of under 10 minutes by default.

### Step 15. Terminal display and plots
Aligned summary tables with units; `plots.py <experiment> [--save]
[--pdf]`; multi-panel figures, 95 % confidence bands, colour-blind-safe
colours and markers, vector output for LaTeX.

## Phase 3 – Wrap-up

### Step 16. Documentation and final review
README, model description (MODEL.md), experiment guide, full test pass
under sanitizers and Valgrind, reproducibility check (two runs, two
thread counts, identical CSV).

## Phase 4 – Thesis rewrite

Needs the original LaTeX sources (text, figures, bibliography).

### Step 17. Thesis setup
LaTeX build in the workspace; figures regenerated from `plots.py` (PDF);
list of every claim and number that changes.
### Step 18. Chapter 3 – Methodology
Exact WCRT, reassignment, communication, TSN network, LET-aware latency,
timing constraints, sensitivity.
### Step 19. Chapter 4 – Experiments
New experiments, results and conclusions.
### Step 20. Chapters 1, 2, 5, abstract and French summary
Consistency with the new chapters 3 and 4; Appendix A updated.

## Progress

| # | Phase | Step | Deliverable | Size | Status |
|---|---|---|---|---|---|
| 0 | – | Bug fixes, communication, TSN network (C++) | merged in `main` (Nov 4, 2026) | – | done |
| 1 | 0 – Foundations | Conventions and plan | PLAN.md, STYLE.md, CLAUDE.md | S | in review |
| 2 | 0 – Foundations | Deterministic C++, golden results | seeded generator, total-order sorts, `tests/golden/`, tests moved | M | to do |
| 3 | 0 – Foundations | C building blocks, pipeline | arrays, context, CMake, sanitizers, CI | M | to do |
| 4 | 1 – Port | Model, generator, partitioning | C code = golden | M | to do |
| 5 | 1 – Port | LET, assignment | C code = golden | S | to do |
| 6 | 1 – Port | Scheduling | C code = golden | M | to do |
| 7 | 1 – Port | Placement | C code = golden | M | to do |
| 8 | 1 – Port | Communication, TSN network | C code = golden, simulator | M | to do |
| 9 | 1 – Port | Drivers, stats; C++ removed | full C, all tests green | M | to do |
| 10 | 2 – Improvements | Parallel experiment engine | OpenMP, CLI, CSV, timers | M | to do |
| 11 | 2 – Improvements | LET-aware latency | sampling delays, data age | S | to do |
| 12 | 2 – Improvements | Timing as constraints | e2e deadlines, link bandwidth | L | to do |
| 13 | 2 – Improvements | Sensitivity, per-controller sizing | scaling factor, per-ECU report | M | to do |
| 14 | 2 – Improvements | Experiment redesign | 7 experiments, budgets | L | to do |
| 15 | 2 – Improvements | Terminal display, plots | tables, `plots.py` | M | to do |
| 16 | 3 – Wrap-up | Documentation, final review | README, MODEL.md, test pass | S | to do |
| 17 | 4 – Thesis | Setup | LaTeX build, figures, change list | S | to do (needs LaTeX sources) |
| 18 | 4 – Thesis | Chapter 3 | rewritten methodology | L | to do |
| 19 | 4 – Thesis | Chapter 4 | rewritten experiments | L | to do |
| 20 | 4 – Thesis | Chapters 1, 2, 5, abstract, résumé | consistent thesis | M | to do |
