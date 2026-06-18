# ese pre-processor project
# (C) 2026 ESE Project Contributors
# Author: Ryan Pratt

import re

from .model import Action, Conditional, Machine, StateBlock, Transition


IDENT = r"[A-Za-z_][A-Za-z0-9_]*"


class ParseError(Exception):
    pass


def parse_ese_block(source: str) -> Machine:
    machine_blocks = _find_named_blocks(source, "machine")
    if len(machine_blocks) != 1:
        raise ParseError("exactly one machine block is required per ESE block")

    machine_name, machine_body, machine_start, machine_end = machine_blocks[0]
    initial = _parse_initial(machine_body, machine_name)
    states = _parse_states(machine_body, machine_name)
    _reject_unparsed_machine_text(machine_body, machine_name)

    conditionals_name = None
    conditionals: list[Conditional] = []
    conditionals_blocks = _find_named_blocks(source, "conditionals")
    if len(conditionals_blocks) > 1:
        raise ParseError(f"Machine {machine_name}: at most one conditionals block is supported")
    if conditionals_blocks:
        conditionals_name, body, start, end = conditionals_blocks[0]
        _ensure_not_inside_machine(start, end, machine_start, machine_end, "conditionals", machine_name)
        conditionals = _parse_conditionals(body, machine_name)

    actions_name = None
    actions: list[Action] = []
    actions_blocks = _find_named_blocks(source, "actions")
    if len(actions_blocks) > 1:
        raise ParseError(f"Machine {machine_name}: at most one actions block is supported")
    if actions_blocks:
        actions_name, body, start, end = actions_blocks[0]
        _ensure_not_inside_machine(start, end, machine_start, machine_end, "actions", machine_name)
        actions = _parse_actions(body, machine_name)

    return Machine(
        name=machine_name,
        initial=initial,
        states=states,
        conditionals=conditionals,
        actions=actions,
        conditionals_machine_name=conditionals_name,
        actions_machine_name=actions_name,
    )


def _find_named_blocks(source: str, keyword: str) -> list[tuple[str, str, int, int]]:
    matches: list[tuple[str, str, int, int]] = []
    pattern = re.compile(rf"\b{keyword}\s+({IDENT})\s*\{{")
    for match in pattern.finditer(source):
        open_brace = source.rfind("{", match.start(), match.end())
        close_brace = _find_matching_brace(source, open_brace)
        name = match.group(1)
        matches.append((name, source[open_brace + 1 : close_brace], match.start(), close_brace + 1))
    return matches


def _find_matching_brace(source: str, open_brace: int) -> int:
    depth = 0
    i = open_brace
    while i < len(source):
        ch = source[i]
        next_ch = source[i + 1] if i + 1 < len(source) else ""
        if ch in ("'", '"'):
            i = _skip_quoted(source, i)
            continue
        if ch == "/" and next_ch == "/":
            i = _skip_line_comment(source, i)
            continue
        if ch == "/" and next_ch == "*":
            i = _skip_block_comment(source, i)
            continue
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ParseError("unclosed block")


def _skip_quoted(source: str, start: int) -> int:
    quote = source[start]
    i = start + 1
    while i < len(source):
        if source[i] == "\\":
            i += 2
            continue
        if source[i] == quote:
            return i + 1
        i += 1
    raise ParseError("unterminated string or character literal")


def _skip_line_comment(source: str, start: int) -> int:
    newline = source.find("\n", start + 2)
    return len(source) if newline == -1 else newline + 1


def _skip_block_comment(source: str, start: int) -> int:
    end = source.find("*/", start + 2)
    if end == -1:
        raise ParseError("unterminated block comment")
    return end + 2


def _parse_initial(machine_body: str, machine_name: str) -> str:
    matches = re.findall(rf"\binitial\s+({IDENT})\s*;", machine_body)
    if len(matches) != 1:
        raise ParseError(f"Machine {machine_name}: exactly one initial state is required")
    return matches[0]


def _parse_states(machine_body: str, machine_name: str) -> list[StateBlock]:
    states: list[StateBlock] = []
    for state_name, body, _, _ in _find_named_blocks(machine_body, "state"):
        transitions = [
            Transition(condition=match.group(1), target=match.group(2))
            for match in re.finditer(rf"\bon\s+({IDENT})\s*->\s*({IDENT})\s*;", body)
        ]
        leftover = re.sub(rf"\bon\s+{IDENT}\s*->\s*{IDENT}\s*;", "", body).strip()
        if leftover:
            raise ParseError(
                f"Machine {machine_name}: state {state_name} contains unsupported syntax"
            )
        states.append(StateBlock(name=state_name, transitions=transitions))
    if not states:
        raise ParseError(f"Machine {machine_name}: at least one state block is required")
    return states


def _reject_unparsed_machine_text(machine_body: str, machine_name: str) -> None:
    cleaned = machine_body
    for _, _, start, end in reversed(_find_named_blocks(machine_body, "state")):
        cleaned = cleaned[:start] + cleaned[end:]
    cleaned = re.sub(rf"\binitial\s+{IDENT}\s*;", "", cleaned)
    if cleaned.strip():
        raise ParseError(f"Machine {machine_name}: machine body contains unsupported syntax")


def _parse_conditionals(body: str, machine_name: str) -> list[Conditional]:
    conditionals: list[Conditional] = []
    i = 0
    while True:
        i = _skip_space(body, i)
        if i >= len(body):
            return conditionals
        match = re.match(rf"({IDENT})\s*=", body[i:])
        if not match:
            raise ParseError(f"Machine {machine_name}: invalid conditional entry")
        name = match.group(1)
        expr_start = i + match.end()
        expr_end = _find_statement_semicolon(body, expr_start)
        expression = body[expr_start:expr_end].strip()
        if not expression:
            raise ParseError(f"Machine {machine_name}: conditional {name} has an empty expression")
        conditionals.append(Conditional(name=name, expression=expression))
        i = expr_end + 1


def _find_statement_semicolon(source: str, start: int) -> int:
    i = start
    while i < len(source):
        ch = source[i]
        next_ch = source[i + 1] if i + 1 < len(source) else ""
        if ch in ("'", '"'):
            i = _skip_quoted(source, i)
            continue
        if ch == "/" and next_ch == "/":
            i = _skip_line_comment(source, i)
            continue
        if ch == "/" and next_ch == "*":
            i = _skip_block_comment(source, i)
            continue
        if ch == ";":
            return i
        i += 1
    raise ParseError("conditional expression is missing a terminating semicolon")


def _parse_actions(body: str, machine_name: str) -> list[Action]:
    actions: list[Action] = []
    i = 0
    while True:
        i = _skip_space(body, i)
        if i >= len(body):
            return actions
        match = re.match(rf"(enter|exit)\s+({IDENT})\s*\{{", body[i:])
        if not match:
            raise ParseError(f"Machine {machine_name}: invalid action entry")
        kind = match.group(1)
        state = match.group(2)
        open_brace = i + match.end() - 1
        close_brace = _find_matching_brace(body, open_brace)
        actions.append(Action(kind=kind, state=state, body=body[open_brace + 1 : close_brace].strip()))
        i = close_brace + 1


def _skip_space(source: str, start: int) -> int:
    i = start
    while i < len(source) and source[i].isspace():
        i += 1
    return i


def _ensure_not_inside_machine(
    start: int,
    end: int,
    machine_start: int,
    machine_end: int,
    keyword: str,
    machine_name: str,
) -> None:
    if machine_start <= start < machine_end or machine_start < end <= machine_end:
        raise ParseError(f"Machine {machine_name}: {keyword} block cannot appear inside machine")
