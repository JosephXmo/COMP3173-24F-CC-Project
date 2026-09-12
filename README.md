# uslc Python Implementation

This is the standard-library-only Python port of `StandardImplementation`
(C reference commit `b416467`). It implements the complete core Set Algebra
language: lexical analysis, runtime SLR table generation, parsing, AST lowering,
static type checking, and symbolic evaluation.

The required interpreter is **CPython 3.14.5+freethreaded with the GIL disabled**.
The CLI and development commands verify this explicitly. No third-party Python
packages are required. Project code does not import or call regular-expression
APIs; the lexer scans raw bytes using explicit ASCII comparisons.

## Quick Start

From this directory, using the already installed `uv`:

```powershell
uv venv --python 3.14.5+freethreaded
uv run --frozen python -m uslc examples/membership.set
uv run --frozen python -m scripts.test --jobs 4
```

The reference setup in this checkout has a local `.venv` and an ignored Python
runtime under `.python-runtimes/`. Once the interpreter exists, all commands
work offline and need no package downloads:

```powershell
uv --cache-dir .uv-cache run --frozen --offline python -m uslc examples/arithmetic.set
uv --cache-dir .uv-cache run --frozen --offline python -m scripts.test --jobs 4
.venv\Scripts\python.exe -m uslc examples/sets.set
```

On Linux, the corresponding direct interpreter path is `.venv/bin/python`.
This is a source project (`tool.uv.package = false`); use `python -m uslc` rather
than installing a console entry point. `.python-version`, `pyproject.toml`, and
`uv.lock` pin the requested interpreter release and record zero external
package dependencies.

## Language

Programs contain zero or more declarations followed by exactly one `show`:

```text
let int lower be 3 .
let set positive be { x : x > lower } .
show 5 @ positive .
```

The result is `boolean: true`. Types are `integer`, `boolean`, and `set`.
Identifiers consist only of lowercase ASCII letters. Integers are `0` or a
nonzero digit followed by digits. Leading zeros and literals outside signed
64-bit range are lexical errors. Minus is a binary operator, so write `0-1`
for a negative value. Comments and Boolean literals are not part of the core
language; `true` and `false` are ordinary identifiers in source text.

| Operators | Operands | Result |
| --- | --- | --- |
| `+`, `-`, `*` | integer, integer | integer |
| `U`, `I` | set, set | set |
| `<`, `>`, `=` | integer, integer | boolean |
| `@` | integer, set | boolean |
| `&`, `\|` | boolean, boolean | boolean |
| `!` | boolean relation | boolean |

Arithmetic, union, and intersection associate to the left. Multiplication and
intersection bind more tightly than addition/subtraction and union. Relations
bind more tightly than `&`, which binds more tightly than `|`. The exact grammar
below takes precedence over informal precedence descriptions. In particular,
rule 26 is `! R`: `!1=2` is valid, but `!(1=2)` and `!!1=2` are not.

Declarations are immutable, cannot be repeated globally, and become visible
only after their initializer has been checked and evaluated. Set representatives
are local integers and may shadow outer names. Sets capture immutable values;
nested representatives have distinct binding IDs. Union/intersection rename
representatives before combining predicates. Membership substitutes the queried
integer and evaluates the predicate with normal Boolean short circuiting.

Python's arbitrary-precision integers are deliberately restricted to the same
`int64_t` domain as C. Every arithmetic result is range-checked. Predicates stay
symbolic until membership evaluation, including potentially overflowing branches
that short circuiting may never visit. Ordinary set display preserves logical
structure; the optional `simplify` extension is not implemented in either core.

## Stable Grammar

Production numbers and symbol order match the C implementation. The generator
computes nullable/FIRST/FOLLOW, LR(0) closure/goto and canonical states, then
detects ACTION conflicts while constructing the SLR table. It never loads the
course CSV at runtime. Generic grammar tests include epsilon productions and
both shift/reduce and reduce/reduce conflicts.

```text
 0. S'  -> S
 1. S   -> D' C .
 2. S   -> C .
 3. D'  -> D D'
 4. D'  -> D
 5. D   -> let T id be E .
 6. T   -> int
 7. T   -> set
 8. E   -> E'
 9. E   -> E U E'
10. E   -> E + E'
11. E   -> E - E'
12. E'  -> E''
13. E'  -> E' I E''
14. E'  -> E' * E''
15. E'' -> num
16. E'' -> id
17. E'' -> ( E )
18. E'' -> { Z P }
19. Z   -> id :
20. P   -> P | P'
21. P   -> P'
22. P'  -> P' & P''
23. P'  -> P''
24. P'' -> R
25. P'' -> ( P )
26. P'' -> ! R
27. R   -> E < E
28. R   -> E > E
29. R   -> E = E
30. R   -> E @ E
31. C   -> show A
32. A   -> E
33. A   -> P
```

The resulting 66-state table is identical to the C-generated table, including
state numbering, dumps, and parser trace order. A separate test compares every
ACTION/GOTO cell to the retained course CSV under a bijective renumbering.

## CLI and Artifacts

```text
python -m uslc [options] <source-file|->

--phase lex|parse|check|run   stop after a phase (default: run)
--emit-dir <path>            write completed-stage JSON to an existing directory
--trace-parser              emit shift/reduce trace to stderr
--dump-grammar              emit productions, FIRST and FOLLOW to stderr
--dump-slr                  emit LR(0) states and ACTION/GOTO to stderr
--compact-json              omit JSON indentation
--jobs <1..32>              number of parallel SLR workers (default: 1)
--help
--version
--                         end option parsing
```

`-` reads stdin as raw bytes. Dump commands may be used without a source file.
Success reports go to stdout; diagnostics, dumps and trace go to stderr.

| Exit | Meaning |
| --- | --- |
| 0 | Success |
| 1 | Lexical error |
| 2 | Syntax error |
| 3 | Type error |
| 4 | Evaluation error |
| 64 | CLI usage or interpreter configuration error |
| 66 | Input error |
| 70 | Internal or resource-limit error |
| 74 | Output error |

Create the artifact directory first:

```powershell
New-Item -ItemType Directory -Force build/artifacts
uv run --frozen python -m uslc --emit-dir build/artifacts examples/sets.set
```

The six filenames and schemas match C: `tokens.json`, `parser_out.json`,
`ast.json`, `typing_out.json`, `evaluation_out.json`, and `diagnostics.json`.
Files use UTF-8 without BOM, two-space indentation by default, stable key order,
and one final LF. Failed stages emit no partial artifacts. Reusing an output
directory removes the six known filenames before the run and retains unrelated
files. `--phase parse` includes structural AST lowering. `ast.json` snapshots
the untyped AST; `typing_out.json` includes types and bindings.

Compatibility details:

- Human reports, source-file byte positions, diagnostic codes, successful-stage
  artifacts, set displays, grammar dumps, and parser traces match C.
- Python help/version text identifies the port, and `--jobs` is an additional
  option. A nonmatching interpreter or an enabled GIL is rejected explicitly.
- Python reads stdin through `sys.stdin.buffer`, preserving CRLF byte offsets.
  Windows C text-mode stdin may translate CRLF; file input preserves bytes in
  both implementations.
- Storage and lifetime are managed by Python. Frozen records replace C arena
  allocation for tokens, parse trees, SLR tables, and symbolic values. Each
  compilation exclusively owns its mutable AST and symbol environment.
- The C grammar bounds (64 symbols, 32 terminals, 64 productions, RHS length 8)
  and depth bound of 255 are retained. Depth/resource failures are structured
  diagnostics rather than a user-facing Python traceback.

## Parallel Work

```powershell
uv run --frozen python -m scripts.batch --jobs 4 examples/arithmetic.set examples/membership.set examples/sets.set
uv run --frozen python -m uslc --jobs 4 --dump-slr examples/membership.set
```

`compile_many` shares one immutable SLR table across independent compilations.
Each worker owns its source session, AST, symbols, bindings, values, diagnostics,
and optional artifact buffers. Results are returned in input order; workers do
not write streams or files. The batch command prints the reports in input order
and returns the first nonzero result status in that order. Batch compilation
and the test runner default to at most four workers on a GIL-disabled runtime.

SLR construction optionally expands independent frontier states in parallel.
Only the coordinator interns states, traversing the original state and symbol
order. This preserves byte-level determinism. The 66-state core grammar is too
small to amortize thread startup, so single-program compilation defaults to
one SLR worker. Declaration order, type checking within one program, and
short-circuit branches remain sequential to preserve error and evaluation order.

Library example:

```python
from uslc.compiler import compile_many
from uslc.source import Source

results = compile_many([
    Source.from_text("show 1+2.", "first.set"),
    Source.from_text("show 3@{x:x>0}.", "second.set"),
], workers=4)
for result in results:
    print(result.report(), end="")
```

For a single source use `compile_source`; an optional immutable `table` can be
reused across calls. `artifacts=True` returns encoded stage snapshots in the
result's `artifacts` dictionary without writing files.

## Tests and Development Commands

```powershell
uv run --frozen python -m scripts.test --jobs 4
uv run --frozen python -m scripts.test --jobs 1
uv run --frozen python -m unittest discover -v
uv run --frozen python -m scripts.audit_imports
uv run --frozen python -m scripts.benchmark
uv run --frozen python -m scripts.differential ../StandardImplementation/build/release/uslc.exe
```

The differential command takes any already-built C reference executable; on
Linux use its `uslc` path. Normal tests are self-contained and do not require C.
The retained fixtures were copied without altering their expected output from
C reference commit `b416467`. Tests cover all 10 C fixtures and 35 JSON files,
the complete CSV table, boundary semantics, and simultaneous compilations.

`scripts/test.ps1 -Jobs 4` and `sh scripts/test.sh 4` locate the project root and
run the suite offline. They assume the pinned interpreter has already been
installed. Test cases never mutate shared working directories or monkeypatch
process-global functions, so the same cases can run concurrently.

See `VALIDATION.md` for actual results and performance measurements, and
`DEPENDENCIES.md` for the complete direct standard-library import inventory.

## Layout

```text
uslc/          compiler library and python -m uslc entry point
scripts/       parallel tests, batch runner, parity checks, import audit, benchmark
tests/         unittest modules and unchanged C golden fixtures
examples/      arithmetic, membership, sets, and error examples
build/         ignored local benchmark and validation output
.venv/         ignored uv virtual environment
```
