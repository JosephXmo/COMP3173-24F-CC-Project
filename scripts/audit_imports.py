"""List direct standard-library imports and reject third-party or regex imports."""

import ast
from pathlib import Path
import sys


def inventory(root: Path) -> dict[str, list[str]]:
    result = {}
    for group in ("uslc", "scripts", "tests"):
        imports = set()
        for path in sorted((root / group).rglob("*.py")):
            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
            for node in ast.walk(tree):
                modules = []
                if isinstance(node, ast.Import):
                    modules = [alias.name for alias in node.names]
                elif isinstance(node, ast.ImportFrom) and not node.level and node.module:
                    modules = [node.module]
                for module in modules:
                    top = module.split(".")[0]
                    if top in ("uslc", "scripts", "tests"):
                        continue
                    if top in ("re", "regex"):
                        raise ValueError(f"regular-expression import in {path}: {module}")
                    if top not in sys.stdlib_module_names:
                        raise ValueError(f"non-standard-library import in {path}: {module}")
                    imports.add(module)
        result[group] = sorted(imports)
    return result


def main() -> int:
    result = inventory(Path(__file__).resolve().parents[1])
    for group, imports in result.items():
        print(f"{group}: {', '.join(imports)}")
    print("Third-party Python packages: none")
    print("Direct regular-expression imports: none")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
