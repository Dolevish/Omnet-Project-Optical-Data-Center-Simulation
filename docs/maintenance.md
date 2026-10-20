# Maintenance and correctness review

The repository was reviewed from original commit `fc49605`. All 60 tracked paths were inventoried: source, NED/INI definitions, project/build metadata, scripts, documentation, and generated binaries/object/dependency files. Generated artifacts were classified by path/type and their generating build rules; they were not treated as source.

## Repository cleanup

| Files | Action and reason |
|---|---|
| `out/` | Remove tracked debug/release executables, objects, dependency files, and compiler caches; regenerate locally |
| `src/*.exe` | Remove platform-specific executables |
| `src/Makefile` and timestamped backups | Remove generated build rules; regenerate with `make makefiles` |
| `simulations/.qtenvrc` | Remove local last-run/UI state |
| `.settings/language.settings.xml` | Remove generated compiler-discovery state |
| `.project`, `.cproject`, `.nedfolders`, `.nedexclusions`, `.oppbuildspec` | Retain portable IDE import/build metadata; make target name explicit |
| `.settings/org.eclipse.core.resources.prefs` | Retain project-wide UTF-8 encoding |
| `.gitignore` | Replace UTF-16 contents with UTF-8 and cover generated outputs |
| `SwitchingFabric.*`, `TrafficMonitor.*` | Remove unused modules absent from the network; buffering and live queued demand now belong to `OpticalSwitch` |
| `simulations/run`, root `Makefile` | Repair case-sensitive executable naming, argument quoting, working-directory handling, and test targets |
| NED package files | Retain existing package names and license annotations; no new repository-wide license was inferred |

These deletions affect the working tree on the maintenance branch. They do not rewrite Git history; the original source and binaries remain available in earlier commits.

## Correctness changes

| Original problem | Updated behavior |
|---|---|
| The simulation constructed a scheduler but never called it | Invoke the standalone scheduler with actual queue occupancy |
| Traffic pattern was read from server parameters to create a hand-written schedule | Demand-aware scheduling has no knowledge of the traffic generator configuration |
| Graph completion balanced rows only; greedy fallback could create conflicting/nonexistent edges | Balance rows and columns, then use augmenting paths to extract genuine perfect matchings |
| Round-Robin included an all-self slot | Cycle only nonzero offsets |
| One FIFO per input caused head-of-line blocking | FIFO per input/destination, with shared finite input capacity |
| Queue service sent at most one packet per trigger and relied on catching channel errors | Resume on actual channel completion and respect slot boundaries |
| Flow IDs from different sources collided at receivers | Use `(source, flowId)` keys |
| FCT started on first-packet arrival | Carry source creation time on every packet |
| Later packets could not establish receive state after first-packet loss | Every packet includes complete flow metadata |
| `websearch` destination selection recursed indefinitely; hotspot traffic could target itself | Explicit skewed selection excludes the source, including two-server cases |
| Offered-load timing used the wrong mean flow size | Derive arrival rate from the configured size distribution |
| Sender packet duplication and retained finished flow entries added overhead | Create packets lazily and discard source descriptors after their final packet |
| Unused metric declarations and cumulative counter emissions were misleading | Record defined signals plus explicit conservation, goodput, and residual-work diagnostics |
| Cleanup occurred only in `finish()` | Cancel owned events and release queued packets in destructors |

## Verification

`make test` runs deterministic standalone scheduling tests. `make smoke` builds and invokes the real simulator for the scenario and accounting checks described in the README. The GitHub Actions workflow installs and builds a pinned OMNeT++ 6.1 source revision, then runs both suites.

The checks validate execution and model invariants, not the scientific fidelity of the abstraction or superiority of one scheduler. No fabricated performance results or benchmark figures are included.

### Verified on 2026-10-20

- Headless OMNeT++ 6.1 (`f7568267cd58d461319965b3cd59722338779edd`) with GCC 13.3.0 on Linux.
- All simulation sources compiled; a separate strict `-Wall -Wextra -Wpedantic -Werror` syntax check passed against the real OMNeT++ headers.
- All 960 randomized scheduling cases and the boundary/invalid-input cases passed.
- All 16 successful simulation scenarios and six expected configuration failures in `tests/smoke.py` passed.
- The README's default `Vermilion_Uniform` configuration completed all three repetitions with its normal warm-up and buffer settings.
- GUI behavior and Windows/macOS execution were not exercised.
