# Python Port Validation

## Environment

- Host: Windows x64.
- Interpreter: CPython 3.14.5 free-threading build, installed with uv.
- `sysconfig.get_config_var("Py_GIL_DISABLED")`: `1`.
- `sys._is_gil_enabled()`: `False`, including after importing the compiler and
  running the concurrent tests.
- Available process CPUs reported by Python: 24.
- Third-party Python distributions in `.venv`: none (`uv pip list` is empty).
- C comparison baseline: `StandardImplementation`, commit `b416467`, Windows
  Release executable previously validated with Clang 22.1.3.

All Python runtime testing reported here used the requested free-threaded
3.14.5 interpreter. Linux execution of the Python port was not performed in
this validation; the shell wrapper and standard-library implementation are
provided for that platform without claiming a Linux test result.

## Functional Results

The self-contained suite passes in both modes:

```text
48 tests, 0 failures (1 workers; GIL enabled: False)
48 tests, 0 failures (4 workers; GIL enabled: False)
```

This includes:

- Every terminal and every single-byte input, adjacent tokens, keyword maximal
  matching, raw CRLF offsets, embedded NUL, invalid UTF-8 token bytes, leading
  zeros, and a 10,000-digit overflow literal.
- Nullable/FIRST/FOLLOW for recursive epsilon grammars, epsilon reductions,
  malformed grammar rejection, closure/goto, canonical-state deduplication,
  shift/reduce and reduce/reduce conflict detection.
- All 66 LR states and every ACTION/GOTO cell compared to the retained course
  CSV under a bijective state mapping.
- Identical sequential and 2/4/8-worker generated SLR tables, including stable
  conflict messages and state numbering.
- Parse-node ordering, precedence and associativity, early EOF, extra tokens,
  malformed parse trees, and the inherited depth limit.
- All combinations of integer/set operands for the nine binary arithmetic,
  set, comparison and membership operators; declaration visibility, shadowing,
  duplicate/undefined/self references, and static checking of unexecuted branches.
- Checked signed-64-bit arithmetic, the minimum representable integer, short
  circuiting, immutable captures, nested membership, alpha-renaming and set
  display, plus seeded arithmetic and set property checks.
- All 10 original C fixtures, unchanged expected reports and exit codes, and
  all 35 original golden JSON files. The optional C reference executable is
  not needed to run these tests.
- JSON escaping, UTF-8, indentation, compact encoding, untyped/typed AST stage
  snapshots, structured predicates, unavailable output paths, and artifact
  directory reuse without removing unrelated files.
- CLI stdin, phase stopping, help/version, malformed options, file errors,
  parallel dump/trace stability, and rejection when the GIL is explicitly
  reenabled with `-X gil=1`.
- Ordered batch results, isolated errors, and barrier-synchronized concurrent
  compilations sharing the immutable table while using separate mutable state.
- Static verification that all direct external imports belong to the standard
  library and that no regular-expression module is directly imported.

The PowerShell test wrapper also passes when invoked from `FOCProject`, outside
the Python repository. `compileall` succeeds for `uslc`, `scripts`, and `tests`.

## Live C/Python Differential Results

```text
C/Python parity: 10 full-artifact fixtures, grammar/SLR dumps and traces,
240 generated/boundary programs passed
```

The differential command executes the actual C binary and compares status,
stdout/stderr, the entire grammar/SLR dump, parser traces, and all completed
stage artifacts. The additional cases consist of 100 arithmetic programs,
100 set-algebra programs, 30 nested capture/membership programs, and 10 boundary
or invalid programs, generated with a fixed seed. Human stream line endings
are normalized for host text-mode comparison; JSON is compared byte-for-byte.

The original C repository remains unchanged. Intentional interface differences
(Python help/version, `--jobs`, and byte-preserving stdin) are documented in the
README rather than silently treated as byte-equivalent behavior.

## Parallel Measurements

Workload: 160 independent source programs, each adding 160 integer terms.
All produce the independently checked result `6720`. Each measurement is the
minimum of three runs using `time.perf_counter`. JSON artifact generation is
disabled for this workload; the complete parse/type/evaluation pipeline runs.

| Workers | Core SLR construction | Batch compilation |
| ---: | ---: | ---: |
| 1 | 0.002049 s | 0.566564 s |
| 2 | 0.002699 s | 0.370784 s |
| 4 | 0.004260 s | 0.287606 s |

On this workload, four-worker batch compilation is approximately **1.97x** as
fast as one worker. The core SLR grammar is small, so thread coordination makes
its parallel construction slower. Consequently, the ordinary single-source
CLI defaults to one SLR worker, while independent batch compilation and tests
use at most four workers by default. Users can choose one worker or an explicit
count. These are local workload measurements, not a universal speedup claim.

`scripts.benchmark` reproduces this experiment and writes ignored machine-local
details to `build/benchmark.json`.

## Reproduce

```powershell
uv --cache-dir .uv-cache run --frozen --offline python -m scripts.test --jobs 4
uv --cache-dir .uv-cache run --frozen --offline python -m scripts.test --jobs 1
uv --cache-dir .uv-cache run --frozen --offline python -m scripts.audit_imports
uv --cache-dir .uv-cache run --frozen --offline python -m scripts.differential ../StandardImplementation/build/release/uslc.exe
uv --cache-dir .uv-cache run --frozen --offline python -m scripts.benchmark
```

The local interpreter and virtual environment must already exist for the
offline commands. See `DEPENDENCIES.md` for all 17 directly imported
standard-library modules and their purposes.
