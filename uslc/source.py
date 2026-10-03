"""Immutable raw source bytes shared by tokens, trees, and diagnostics."""

from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True, slots=True)
class Source:
    name: str
    data: bytes

    @classmethod
    def from_text(cls, text: str, name: str = "<memory>") -> Source:
        return cls(name, text.encode("utf-8"))

    @classmethod
    def from_file(cls, path: str) -> Source:
        return cls(path, Path(path).read_bytes())
