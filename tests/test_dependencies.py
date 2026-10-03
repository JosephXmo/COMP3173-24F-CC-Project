from pathlib import Path
import sys
import unittest

from scripts.audit_imports import inventory


class DependencyTests(unittest.TestCase):
    def test_standard_library_only_without_regex(self):
        imports = inventory(Path(__file__).resolve().parents[1])
        self.assertIn("concurrent.futures", imports["uslc"])
        self.assertIn("json", imports["uslc"])
        self.assertNotIn("re", {name for group in imports.values() for name in group})
        self.assertFalse(sys._is_gil_enabled())
