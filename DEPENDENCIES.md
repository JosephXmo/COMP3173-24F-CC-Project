# Dependency Inventory

No third-party Python packages are used or installed in the project environment.
The lockfile contains only the virtual `uslc-python` project. The interpreter is
CPython **3.14.5+freethreaded**; **uv 0.11.16** is an external environment manager,
not an imported Python package or runtime dependency of the compiler.

This inventory lists every standard-library module directly imported by project
code. Standard-library modules' internal implementation dependencies are not
additional project package requirements.

## Compiler and CLI

| Module | Use |
| --- | --- |
| `concurrent.futures` | Thread pools for independent compilations and SLR frontiers |
| `dataclasses` | Records, immutable nodes, per-session results, symbolic substitution |
| `enum` | Stable token IDs, node kinds, and type names |
| `json` | Deterministic JSON artifact encoding |
| `os` | Available CPU count for bounded worker defaults |
| `pathlib` | Binary source and artifact file I/O |
| `sys` | CLI arguments, streams, version and GIL state checks |
| `sysconfig` | Verification of the free-threaded build flag |

## Additional Test and Development Modules

| Module | Use |
| --- | --- |
| `ast` | Static import inventory and forbidden-import checks |
| `csv` | Read the retained course parsing table in tests only |
| `io` | Per-test in-memory output buffers |
| `random` | Seeded arithmetic and set property/differential cases |
| `subprocess` | Isolated CLI tests and execution of the C reference |
| `tempfile` | Isolated filesystem test and parity output directories |
| `threading` | Barrier-based simultaneous-compilation tests |
| `time` | Wall-clock benchmark measurements |
| `unittest` | Standard-library test framework |

Development commands also reuse the runtime modules listed above where needed.
The complete union is **17 directly imported standard-library modules**.

## Regular Expressions

Project code does not import `re` or `regex`, does not call a regular-expression
API, and does not use `eval` to parse source expressions. Tokenization is an
explicit byte scanner; grammar construction and parsing are direct algorithms.
General-purpose standard-library services, such as JSON and unittest, retain
their own internal implementations.

Reproduce the direct-import check:

```powershell
uv run --frozen python -m scripts.audit_imports
```

The audit parses every Python source in `uslc/`, `scripts/`, and `tests/`, lists
absolute standard-library imports by group, and rejects direct regex imports
or third-party packages. It also runs as a normal test.
