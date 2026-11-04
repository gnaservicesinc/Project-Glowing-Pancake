# M1-01 — Deterministic state primitives

Completed 2026-11-04. All six native compiler jobs pass across all five targets,
including exact 1,000-day canonical fixture parity.

## Modules and bounds

The Qt-free core owns `units.hpp`, `checked_arithmetic.hpp`, `calendar.hpp`,
`random.hpp` and `integer_codec.hpp` under `src/core/include/pgp/core/`.

- Explicit integral-only types distinguish constant-2014 `Cents`, consumed
  `MilliCU`, `MilliCUPerDay`, `BasisPoints`, `PartsPerMillion`, `OrdinalDay`
  and nonnegative `Days`. Rates/probabilities have bounded signed delta types.
  Units reject floating/bool construction, invalid ranges and narrowing.
  `EntityId<Tag>` has a nonzero unsigned 64-bit value and distinct domain tags.
  Named checked operations preserve units; `integrate(rate, days)` yields work.
- Products use portable base-2^32 multiplication and unsigned long division,
  with the same path on Clang, GCC and MSVC/ARM64. Intermediates above 2^128−1
  and final destination overflow raise errors. Zero factors are recognized
  before multiplication; empty factors mean product 1. Denominators must be
  positive. Half-up rounding occurs once after full multiplication. Signed
  postings round the magnitude and restore the sign, including INT64_MIN.
- Gregorian dates support years 1–9999; 2014-01-01 is ordinal zero. Invalid
  dates/origins and overflowing steps are rejected. The 2014–2114 interval has
  36,524 days; 2100 is not a leap year. Formatting is fixed ASCII.
- SplitMix64 uses increment `0x9e3779b97f4a7c15`, shifts 30/27/31 and
  multipliers `0xbf58476d1ce4e5b9` / `0x94d049bb133111eb`, with deliberate
  uint64 modular PRNG arithmetic. State is the accumulator before the next
  increment/draw. Initial state is `mix(seed + increment*(ordinal+1))`.
  Saved order: research, rivals, markets, geography, politics, hazards,
  narrative. Bounded draws reject the short prefix before modulo. Copies
  support previews without consuming authoritative draws.
- Integer-token readers accept complete JSON integer tokens within signed or
  unsigned 64-bit range, rejecting decimal/exponent notation, bool/null,
  whitespace, quoted/coerced values and leading zeros. Full document/schema
  validation belongs to M1-02; no floating conversion precedes these readers.

These are primitive contracts. Domain commands, ledgers, content loading,
campaign persistence and market allocation remain later tasks.

## Frozen primitive fixture

`tests/primitive_fixture.cpp` runs 1,000 scripted days, seed 123456789, with all
seven streams, signed postings, multi-factor rounding, integrated capacity and
a total sort key ending in typed entity ID. This test-only executable emits
1,000 daily FNV-1a64 checksums of canonical ASCII JSON state and exact final
state. FNV is a test checksum, never a security/content hash. Key order is
explicit; integer `to_chars` and fixed ASCII dates avoid locale dependence.
Dates, quantities, all RNG accumulators and entity order are included.

At day 500, a comparison run round-trips each primitive integer token and
restores seven RNG accumulators, then matches uninterrupted state each day.
A preview draw leaves authoritative RNG unchanged. This proves the primitive
token/snapshot boundary, not a complete save loader or migration.

`tools/tests/primitive_fixture_oracle.py` independently uses Python arbitrary
precision integers and `datetime` to derive the checked-in golden
`tests/fixtures/primitive-1000-days.json`. Tests never regenerate the golden.
Its whole normalized output SHA-256 is
`fa9f8690ae06f649aa0f7f6332c28b46ccd5af7b0819744d84528a62089a0868`.
The verifier rejects floats, nonfinite constants and duplicate keys, then
compares exact bytes, normalizing CRLF only.

The constrained-residue test reproduces the planning example: 100-cent budget,
two five-task tentative quantities at 10/20 cents, ratio 100/150, floors 3/3,
and ID-tied final allocation 4/3. Capacity and demand restrictions stop
otherwise eligible residues. This fixed local example is not M2-03's optimizer.

## Verification

| Check | Observed result |
| --- | --- |
| Core Release and Debug | 13/13 CTests each, including all 1,000 oracle checksums and six verifier unit checks |
| Desktop Release | 19/19 CTests; existing resource/audio/graphics tests pass |
| Build wrapper | Existing 11/11 cleanup/configuration/failure checks pass |
| Independent arithmetic checks | Strict LLVM warnings, ASan/UBSan and 200,000 two/three-factor comparisons against native unsigned-128 scratch oracle pass |
| Permanent boundary tests | Exhaustive small arithmetic, intermediate/final overflow, signed minima, invalid denominators, single-round formulas, Gregorian endpoints/centuries and all 36,524 campaign days, typed ranges, RNG vectors/rejection/stream independence pass |
| Headless runtime linkage | Only system libc++/libSystem; no Qt or new dependency |
| Independent integration review | No blocking finding; evidence limits above confirmed |

No SDK installation, new GUI render, audio-device exercise or gameplay run.
The existing optional Qt TaskTree warning remains recorded in M0-03 evidence.

## Native acceptance

`.github/workflows/core-primitives.yml` has six native Release jobs: Mac arm64
Apple Clang; Linux amd64 GCC/Clang; Linux arm64 GCC; Windows amd64/arm64 MSVC.
It asserts runner architecture, uses installed tools, runs CTest and exact
golden verification, and retains small compiler/test records for seven days.
Actions use pinned commits. Labels were verified against
[GitHub's runner reference](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
Windows uses the Visual Studio generator with explicit x64/ARM64 target; the
[ARM inventory](https://github.com/actions/partner-runner-images/blob/main/images/arm-windows-11-image.md)
records available tools. These jobs neither package nor qualify Qt graphics,
audio or clean devices; M0-03/04 remain separate gates.

The first run found hosted Windows images had moved to Visual Studio 2026.
The workflow now discovers the installed instance with bundled `vswhere`,
matches its major version to CMake's advertised generators and selects that
exact instance. No compiler installation or downgrade is hidden in the job.

[Native run 41863103816](https://github.com/gnaservicesinc/Project-Glowing-Pancake/actions/runs/41863103816)
tests implementation `56bf6db` plus workflow correction `8dcf6e5`. Decoded job
logs establish the results below; the list-jobs API returned stale queued
snapshots during this observation and was not treated as completion evidence.

| Native target/compiler | Result |
| --- | --- |
| Linux amd64 GCC 13.3.0 | 13/13 CTests and exact golden match |
| Linux arm64 GCC 13.3.0 | 13/13 CTests and exact golden match |
| Linux amd64 Clang 18.1.3 | 13/13 CTests and exact golden match |
| Windows amd64 MSVC 19.51.36260.0 / VS 18 2026 | 13/13 CTests and exact golden match |
| Windows arm64 MSVC 19.51.36260.0 / VS 18 2026 | 13/13 CTests and exact golden match |
| Mac arm64 Apple Clang 17.0.0 (clang-1700.0.13.5) | 13/13 CTests and exact golden match; local Apple Clang 21.0.0 also passes |

Every native job records normalized fixture SHA-256
`fa9f8690ae06f649aa0f7f6332c28b46ccd5af7b0819744d84528a62089a0868`.
Windows logs directly identify `Hostarm64/arm64/cl.exe` and
`Hostx64/x64/cl.exe`, confirming native compiler hosts as well as targets.
All six jobs successfully finalized/uploaded evidence and completed
post-job cleanup without logged errors.

The [Mac job](https://github.com/gnaservicesinc/Project-Glowing-Pancake/actions/runs/41863103816/job/126690116324)
waited for runner assignment, then completed on `macos-15-arm64`, including
13/13 CTests in 1.18 seconds and evidence artifact `13342016386`. Windows ARM64/
amd64 artifacts are `13339748796` / `13340817152`; Linux GCC amd64/arm64 and
Clang amd64 artifacts are `13341531320`, `13341860128`, `13340437822`. Artifact
retention is seven days; the checked-in golden/fingerprint and run links preserve
the durable test contract and observed result. This closes M1-01 primitive
parity, while M0 graphics/audio/deployment acceptance stays separate.

```sh
./make.sh --core-only --test --jobs 2
python3 tools/check_primitive_fixture.py --executable build/core-release/bin/pgp-primitive-fixture
```
