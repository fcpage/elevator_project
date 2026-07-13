# ese pre-processor project
# (C) 2026 ESE Project Contributors
# Author: Ryan Pratt

from .model import Machine


class ValidationError(Exception):
    pass


def validate_machine(machine: Machine) -> None:
    if machine.conditionals_machine_name and machine.conditionals_machine_name != machine.name:
        raise ValidationError(
            f"Machine {machine.name}: conditionals block is for "
            f"'{machine.conditionals_machine_name}', expected '{machine.name}'."
        )
    if machine.actions_machine_name and machine.actions_machine_name != machine.name:
        raise ValidationError(
            f"Machine {machine.name}: actions block is for "
            f"'{machine.actions_machine_name}', expected '{machine.name}'."
        )

    state_names: list[str] = []
    for state in machine.states:
        if state.name in state_names:
            raise ValidationError(f"Machine {machine.name}: duplicate state '{state.name}'.")
        state_names.append(state.name)

    known_states = set(state_names)
    if machine.initial not in known_states:
        raise ValidationError(
            f"Machine {machine.name}: initial state '{machine.initial}' "
            "does not match any declared state.\n"
            f"Known states: {', '.join(state_names)}"
        )

    conditional_names: list[str] = []
    for conditional in machine.conditionals:
        if conditional.name in conditional_names:
            raise ValidationError(
                f"Machine {machine.name}: duplicate conditional '{conditional.name}'."
            )
        conditional_names.append(conditional.name)

    known_conditions = set(conditional_names)
    for state in machine.states:
        for transition in state.transitions:
            if transition.target not in known_states:
                raise ValidationError(
                    f"Machine {machine.name}: transition target '{transition.target}' "
                    "does not match any declared state.\n"
                    f"Known states: {', '.join(state_names)}"
                )
            if transition.condition not in known_conditions:
                raise ValidationError(
                    f"Machine {machine.name}: transition condition '{transition.condition}' "
                    "does not have a matching conditional."
                )

    seen_actions: set[tuple[str, str]] = set()
    for action in machine.actions:
        if action.state not in known_states:
            raise ValidationError(
                f"Machine {machine.name}: {action.kind} action refers to unknown "
                f"state '{action.state}'.\nKnown states: {', '.join(state_names)}"
            )
        key = (action.kind, action.state)
        if key in seen_actions:
            raise ValidationError(
                f"Machine {machine.name}: duplicate {action.kind} action for state '{action.state}'."
            )
        seen_actions.add(key)
