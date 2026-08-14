import csv
import sys
import time

from bb_machine import TransitionMachine


def run_search(
    num_states: int,
    max_steps_limit: int = 1000,
    max_candidates: int | None = None,
    use_unreachable_pruning: bool = True,
) -> tuple[int, list[list[list[str | None]]]]:
    state_letters = [chr(ord("A") + i) for i in range(num_states)]

    initial_head_position = num_states * 25

    instruction_list = [
        f"{v}{d}{s}" for v in [0, 1] for d in ["L", "R"] for s in state_letters
    ]
    instruction_list = ["0RH"] + instruction_list

    max_steps = 0
    max_patterns: list[list[list[str | None]]] = []

    writer = csv.writer(sys.stdout, lineterminator="\n")
    writer.writerow(["pattern", "steps", "elapsed_ms", "growth_pattern"])

    candidates_tried = 0

    instruction_stack: list[tuple[int, int, str]] = [(0, 0, "0RH")]

    while True:
        if max_candidates is not None and candidates_tried >= max_candidates:
            break

        last_instruction = instruction_stack[-1][2]
        last_instruction_index = instruction_list.index(last_instruction)
        if last_instruction_index == len(instruction_list) - 1:
            instruction_stack.pop()
            if len(instruction_stack) == 0:
                break
            continue
        if len(instruction_stack) == num_states * 2:
            # Last slot is always "0RH" (no other option) → backtrack
            instruction_stack.pop()
            if len(instruction_stack) == 0:
                break
            continue

        next_instruction = instruction_list[last_instruction_index + 1]

        instruction_stack[-1] = (
            instruction_stack[-1][0],
            instruction_stack[-1][1],
            next_instruction,
        )
        if len(instruction_stack) == 1:
            if instruction_stack[0][2][1] == "L":
                continue  # The first instruction must be "0RH" (halt) or "*RB" (right)

        state_list = {instruction[2] for _, _, instruction in instruction_stack} - {"A"}
        original_state_list = {chr(ord("B") + i) for i in range(len(state_list))}
        if state_list != original_state_list:
            # "A","B","E"のように、飛びがあるパターンは不要なのでスキップ
            continue

        t0 = time.perf_counter()

        tm = TransitionMachine(num_states, initial_head_position)
        pattern: list[list[str | None]] = [[None, None] for _ in range(num_states)]
        for state_index, tape_value, instruction in instruction_stack:
            pattern[state_index][tape_value] = instruction
        tm.init_instructions(pattern)
        if use_unreachable_pruning and tm.check_unreachable_halt_or_none():
            continue
        steps, result_instruction = tm.run(max_steps=max_steps_limit)
        growth_pattern = steps == 0 and tm.machine.check_sweep_growth_pattern()
        if steps > 0 and result_instruction is None:
            # 未定義の場合、そこをHALTにする
            instruction_stack.append((
                tm.machine.state,
                tm.machine.tape[tm.machine.head_position],
                instruction_list[0],
            ))

        elapsed_ms = (time.perf_counter() - t0) * 1000

        writer.writerow([str(pattern), steps, f"{elapsed_ms:.3f}", growth_pattern])

        candidates_tried += 1

        if steps > 0 and steps > max_steps:
            max_steps = steps
            max_patterns = [pattern]
        elif steps > 0 and steps == max_steps:
            max_patterns.append(pattern)

    print(f"# Candidates tried: {candidates_tried}", file=sys.stderr)
    print(f"# Max steps: {max_steps}", file=sys.stderr)
    print(f"# Patterns with max steps: {max_patterns}", file=sys.stderr)

    return max_steps, max_patterns


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser(description="Search for Busy Beaver machines")
    parser.add_argument("num_states", type=int, help="Number of states")
    parser.add_argument("--max-steps", type=int, default=1000)
    parser.add_argument("--max-candidates", type=int, default=None)
    parser.add_argument("--no-unreachable-pruning", action="store_true")
    args = parser.parse_args()

    run_search(
        args.num_states,
        max_steps_limit=args.max_steps,
        max_candidates=args.max_candidates,
        use_unreachable_pruning=not args.no_unreachable_pruning,
    )


if __name__ == "__main__":
    main()
