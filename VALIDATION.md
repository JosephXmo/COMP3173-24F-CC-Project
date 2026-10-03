# Validation Record

Validated on 2026-09-12. This record covers the core language (productions 0-33),
not the optional M7 `simplify` extension.

## Toolchain Matrix

| Platform | Compiler and mode | Debug | Release | Validation |
| --- | --- | --- | --- | --- |
| Linux, Ubuntu 24.04 under WSL 2 | Clang 18.1.3, `-std=c23` | Pass | Pass | 32 unit tests, 10 integration fixtures, CLI checks |
| Windows x64 | Clang 22.1.3, `-std=c23` | Pass | Pass | 32 unit tests, 10 integration fixtures, CLI checks |
| Windows x64 | MSVC 19.51.36257, `/std:clatest` | Pass | Pass | Same suite; compatibility build only |
| Linux | GCC 13.3.0, `-std=c2x -O2` | Not a strict gate | Compatibility experiment | Compiler executable and 32 unit tests pass |

Both Clang builds pass the `__STDC_VERSION__ >= 202311L` compile probe. Linux
Clang and the GCC compatibility experiment also compile with `-Werror`. The
final Windows Clang and MSVC build logs contain no project warnings.

GCC 13 is correctly rejected by the normal strict build script because it does
not accept `-std=c23`. The recommended GCC 14+ strict matrix was not available
for this validation; GCC 13's draft mode is not counted as C23 conformance.

The Windows Clang reference compiler is the x64 compiler supplied with Visual
Studio. Run from Developer PowerShell or provide a configured compiler path so
that the compiler can find the platform headers and linker.

## Semantic and Structural Checks

- Every token kind, adjacency, longest match, CRLF/LF spans, embedded NUL,
  non-ASCII input, leading zeros, and integer literal bounds.
- FIRST/FOLLOW with nullable recursive grammars, LR closure/goto, canonical
  state uniqueness, shift/reduce and reduce/reduce conflict detection.
- All 66 generated core states and every ACTION/GOTO cell match the retained
  course CSV after a bijective state renumbering. The compiler never loads it.
- Parse child order, precedence, associativity, epsilon reductions, missing and
  extra tokens, malformed internal trees, and bounded nesting errors.
- Declaration matching, undefined/duplicate/self references, operand types,
  representative shadowing, and unique lexical bindings.
- Checked arithmetic including `INT64_MIN`, overflow diagnostics, Boolean
  short circuiting, captured declarations, nested membership, set union and
  intersection, alpha-renaming, and deterministic symbolic display.
- JSON escaping, embedded controls, UTF-8 passthrough, compact and pretty
  formatting, writer state errors, and write-failure propagation.

The 10 integration fixtures compare stdout, stderr, exit status, and requested
artifacts. They cover every stopping phase and each user-error phase. There
are 35 golden JSON files, including a structured symbolic-set result. JSON
files also pass independent parsing and UTF-8/BOM/newline checks using
PowerShell/.NET. Both platforms produce byte-identical JSON.

CLI checks cover stdin, help, version, grammar and SLR dumps, parser trace,
invalid arguments, missing input, unavailable output directories, and reuse of
an artifact directory after failure. Stale stage files are removed while an
unrelated file is retained. Test output directories are separate for Linux and
Windows so the two hosts can share the checkout.

## Memory and Failure Paths

The complete Linux suite passes with:

```sh
CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -Werror' \
  ./scripts/build.sh debug
./scripts/test.sh debug --no-build
```

AddressSanitizer and UndefinedBehaviorSanitizer report no leaks or undefined
behavior in these runs. Arena allocation failure injection covers parser,
AST, and evaluator paths. Tests also exercise allocation-size overflow, node
alignment, invalid evaluator bindings, read-only JSON streams, and the tree
depth limit. These checks do not constitute exhaustive proof for every
possible allocation failure or input.

After sanitizer testing, ordinary Debug and Release binaries were rebuilt.
Release executables have no sanitizer dependency.

## Reproduce

From any directory, invoke the scripts by their path. They locate the project
root themselves; Linux tests were also run with `/tmp` as the caller's working
directory.

```sh
./scripts/build.sh debug
./scripts/test.sh debug --no-build
./scripts/build.sh release
./scripts/test.sh release --no-build
```

```powershell
# In a configured Developer PowerShell:
pwsh -NoProfile -File .\scripts\build.ps1 -Configuration Debug -Strict
pwsh -NoProfile -File .\scripts\test.ps1 -Configuration Debug -NoBuild
pwsh -NoProfile -File .\scripts\build.ps1 -Configuration Release -Strict
pwsh -NoProfile -File .\scripts\test.ps1 -Configuration Release -NoBuild

# Explicit compatibility build:
pwsh -NoProfile -File .\scripts\build.ps1 -Configuration Debug -Compiler cl.exe
pwsh -NoProfile -File .\scripts\test.ps1 -Configuration Debug -NoBuild
```

Build products, temporary validation helpers, and logs reside under ignored
`build/`. The retained course CSV and golden fixtures reside under `tests/`.
The original course documents and startup implementations are unchanged.

## Defined Limits

- `--emit-dir` requires an existing directory; ISO C has no directory creation
  API. Known artifact filenames in that directory are replaced on each run.
- Tree heights and recursive processing depths are bounded at 255 and report
  `I9004` rather than exhausting the host stack.
- File input preserves original bytes. Stdin offsets refer to the buffer after
  any host C runtime text-stream translation, as documented in the README.
- Set predicates remain symbolic and are evaluated for membership; ordinary
  `show` does not perform complete logical simplification.
