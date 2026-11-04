# mapping – notes for Claude

Mapping methodology of M. Gauthier's thesis (ISAE-SUPAERO, 2024): estimate
the computational resources of an automotive application on a zonal E/E
architecture. Being ported from C++ to C11, then improved. **PLAN.md is the
source of truth**: read its Progress table first and continue from the
first step not done.

## Conventions
- Linux kernel coding style, `mp_`/`MP_` prefixes: STYLE.md.
- One branch per step (`step-NN-short-name`), one pull request; the owner
  (ifupauline) reviews and merges. Never push to `main`.
- Commits authored `ifupauline <ifupauline21@gmail.com>`, no co-author or
  session lines (owner's decision). Set once per clone:
  `git config user.name ifupauline && git config user.email ifupauline21@gmail.com`
- Pull requests are opened with the REST API
  (`gh api repos/ifupauline/mapping/pulls -f title=... -f head=... -f base=main -f body=...`);
  GraphQL (`gh pr ...`) is not available.
- Until Step 9 the C port must reproduce the C++ results exactly
  (golden results from Step 2).

## Workspace
- Conversations start from an empty workspace: clone the repository,
  build in a separate directory (`cmake -S . -B build && cmake --build build`),
  never commit build output or `res_*.txt`.
- Long jobs: the workspace has 2 CPUs; run experiments with reduced
  instance counts here and full runs on the owner's machine.
- The owner may type "continue" when a conversation runs out of room:
  pick up from the Progress table and the open pull request.

## Model reminders
- Times in microseconds, utilizations in permill (PHI = 800 = 80 %).
- WCRT is the exact fixed point of Eq. 3.3 (`sched_analysis.cpp`).
- LET task: highest priority, period = GCD of the core's task periods,
  C = 200 us; zero on an empty core.
- Network: star, 1 Gbit/s, IEEE 802.1Q strict priority + 802.1Qav CBS
  (idle slope 75 %), Network Calculus bounds (`network.cpp`).

## Status
See the Progress table in PLAN.md.
