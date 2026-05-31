# ese pre-processor project
# (C) 2026 ESE Project Contributors
# Author: Ryan Pratt

from dataclasses import dataclass, field


@dataclass(frozen=True)
class Transition:
    condition: str
    target: str


@dataclass(frozen=True)
class StateBlock:
    name: str
    transitions: list[Transition] = field(default_factory=list)


@dataclass(frozen=True)
class Conditional:
    name: str
    expression: str


@dataclass(frozen=True)
class Action:
    kind: str
    state: str
    body: str


@dataclass(frozen=True)
class Machine:
    name: str
    initial: str
    states: list[StateBlock]
    conditionals: list[Conditional] = field(default_factory=list)
    actions: list[Action] = field(default_factory=list)
    conditionals_machine_name: str | None = None
    actions_machine_name: str | None = None
