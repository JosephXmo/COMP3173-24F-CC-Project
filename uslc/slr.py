"""Deterministic LR(0)/SLR construction, with optional parallel frontier expansion."""

from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass

from .diagnostic import internal
from .grammar import Analysis, Grammar, analyze

type Item = tuple[int, int]
type ItemSet = frozenset[Item]


@dataclass(frozen=True, slots=True)
class Action:
    kind: str = "error"
    target: int = 0


@dataclass(frozen=True, slots=True)
class State:
    items: ItemSet
    transitions: tuple[int | None, ...]
    actions: tuple[Action, ...]


@dataclass(frozen=True, slots=True)
class SlrTable:
    grammar: Grammar
    analysis: Analysis
    states: tuple[State, ...]


def closure(grammar: Grammar, items: ItemSet) -> ItemSet:
    result = set(items)
    pending = list(items)
    while pending:
        production_id, dot = pending.pop()
        production = grammar.productions[production_id]
        if dot == len(production.rhs):
            continue
        symbol = production.rhs[dot]
        if symbol < grammar.terminals:
            continue
        for index, candidate in enumerate(grammar.productions):
            item = (index, 0)
            if candidate.lhs == symbol and item not in result:
                result.add(item)
                pending.append(item)
    return frozenset(result)


def goto(grammar: Grammar, items: ItemSet, symbol: int) -> ItemSet:
    shifted = frozenset((p, dot + 1) for p, dot in items
                        if dot < len(grammar.productions[p].rhs)
                        and grammar.productions[p].rhs[dot] == symbol)
    return closure(grammar, shifted)


def build_slr(grammar: Grammar, workers: int = 1) -> SlrTable:
    if workers < 1:
        raise ValueError("workers must be positive")
    analysis = analyze(grammar)
    initial = closure(grammar, frozenset({(0, 0)}))
    item_sets = [initial]
    indices = {initial: 0}
    transitions = []

    def expand(items: ItemSet) -> tuple[tuple[int, ItemSet], ...]:
        symbols = sorted({grammar.productions[p].rhs[dot] for p, dot in items
                          if dot < len(grammar.productions[p].rhs)})
        return tuple((symbol, goto(grammar, items, symbol)) for symbol in symbols)

    pool = ThreadPoolExecutor(max_workers=workers) if workers > 1 else None
    try:
        cursor = 0
        while cursor < len(item_sets):
            frontier_end = len(item_sets)
            frontier = item_sets[cursor:frontier_end]
            rows = pool.map(expand, frontier) if pool and len(frontier) > 1 else map(expand, frontier)
            # Only this coordinator assigns state IDs, in the C implementation's BFS order.
            for edges in rows:
                row = [None] * len(grammar.names)
                for symbol, items in edges:
                    if items not in indices:
                        indices[items] = len(item_sets)
                        item_sets.append(items)
                    row[symbol] = indices[items]
                transitions.append(tuple(row))
            cursor = frontier_end
    finally:
        if pool:
            pool.shutdown(wait=True, cancel_futures=True)

    states = []
    for state_id, items in enumerate(item_sets):
        actions = [Action()] * grammar.terminals

        def insert(terminal: int, action: Action) -> None:
            previous = actions[terminal]
            if previous.kind != "error" and previous != action:
                raise internal("P2090", f"grammar conflict in state {state_id} on "
                               f"'{grammar.names[terminal]}': {previous.kind}/{action.kind}")
            actions[terminal] = action

        for terminal in range(grammar.terminals):
            target = transitions[state_id][terminal]
            if target is not None:
                insert(terminal, Action("shift", target))
        for production_id, dot in sorted(items):
            production = grammar.productions[production_id]
            if dot != len(production.rhs):
                continue
            if production_id == 0:
                insert(grammar.eof, Action("accept"))
            else:
                for terminal in sorted(analysis.follow[production.lhs]):
                    insert(terminal, Action("reduce", production_id))
        states.append(State(items, transitions[state_id], tuple(actions)))
    return SlrTable(grammar, analysis, tuple(states))
