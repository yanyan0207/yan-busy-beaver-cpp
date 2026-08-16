import sys


class MachineState:
    def __init__(self, initial_head_position: int = 100) -> None:
        self.head_position = initial_head_position
        self.tape = [0] * (
            self.head_position * 2 + 1
        )  # Initialize a tape with cells, all set to 0
        self.max_position = self.head_position
        self.min_position = self.head_position
        self.state = 0
        self.step = 0

        self.history: list[MachineState] = []
        self.instruction_history: list[tuple[int, int, int]] = []

    def copy_without_history(self) -> "MachineState":
        new_state = MachineState(self.head_position)
        new_state.tape = self.tape.copy()
        new_state.max_position = self.max_position
        new_state.min_position = self.min_position
        new_state.state = self.state
        new_state.step = self.step
        return new_state

    def write_and_move(self, value: int, direction: int, state: int) -> None:
        self.history.append(self.copy_without_history())
        self.tape[self.head_position] = value
        self.head_position += direction
        self.max_position = max(self.max_position, self.head_position)
        self.min_position = min(self.min_position, self.head_position)
        self.state = state
        self.step += 1
        self.instruction_history.append((value, direction, state))

    def read(self) -> list[int]:
        return self.tape[self.min_position : self.max_position + 1]

    def check_loop(self) -> int | None:
        """ループを検出し、ヘッドの平行移動量を返す。ループなしは None。0 は定位置ループ。"""
        old_min_position = len(self.tape)
        old_max_position = 0
        for prev_state in self.history[::-1]:
            old_min_position = min(old_min_position, prev_state.head_position)
            old_max_position = max(old_max_position, prev_state.head_position)

            if prev_state.state != self.state:
                continue

            if prev_state.head_position == self.head_position:
                old_tape_slice = prev_state.tape[
                    old_min_position : old_max_position + 1
                ]
                current_tape_slice = self.tape[old_min_position : old_max_position + 1]
                if old_tape_slice == current_tape_slice:
                    return 0
            elif prev_state.head_position < self.head_position:
                diff = self.head_position - prev_state.head_position
                old_tape_slice = prev_state.tape[
                    old_min_position : prev_state.max_position + 1
                ]
                current_tape_slice = self.tape[
                    old_min_position + diff : self.max_position + 1
                ]
                if old_tape_slice == current_tape_slice:
                    return diff
            else:
                diff = prev_state.head_position - self.head_position
                old_tape_slice = prev_state.tape[
                    prev_state.min_position : old_max_position + 1
                ]
                current_tape_slice = self.tape[
                    self.min_position : old_max_position - diff + 1
                ]
                if old_tape_slice == current_tape_slice:
                    return -diff

        return None

    def check_sweep_growth_pattern(self) -> bool:
        if len(self.history) < 100:
            return False  # Not enough history to check for growth pattern
        if self.max_position - self.min_position < 30:
            return False  # Not enough tape growth to consider

        # find become left edge
        left_edge_index = None
        for i in range(len(self.history) - 1, 50, -1):
            search_state = self.history[i]
            search_prev_state = self.history[i - 1]
            if (
                search_state.head_position != search_state.min_position
                and search_prev_state.head_position == search_prev_state.min_position
            ):
                left_edge_index = i
                break

        if left_edge_index is None:
            return False  # No left edge found in history

        # find become riget edge
        right_edge_index = None
        for i in range(left_edge_index - 1, 50, -1):
            search_state = self.history[i]
            search_prev_state = self.history[i - 1]
            if (
                search_state.head_position != search_state.max_position
                and search_prev_state.head_position == search_prev_state.max_position
            ):
                right_edge_index = i
                break

        if right_edge_index is None:
            return False  # No right edge found in history

        # serch for prev left edge
        prev_left_edge_index = None
        for i in range(right_edge_index - 1, 20, -1):
            search_state = self.history[i]
            search_prev_state = self.history[i - 1]
            if (
                search_state.head_position != search_state.min_position
                and search_prev_state.head_position == search_prev_state.min_position
            ):
                prev_left_edge_index = i
                break
        if prev_left_edge_index is None:
            return False  # No previous left edge found in history

        # find become riget edge
        prev_right_edge_index = None
        for i in range(prev_left_edge_index - 1, 10, -1):
            search_state = self.history[i]
            search_prev_state = self.history[i - 1]
            if (
                search_state.head_position != search_state.max_position
                and search_prev_state.head_position == search_prev_state.max_position
            ):
                prev_right_edge_index = i
                break
        if prev_right_edge_index is None:
            return False  # No previous right edge found in history

        # Compare history
        current_instruction_history = self.instruction_history[
            right_edge_index:left_edge_index
        ]
        prev_instruction_history = self.instruction_history[
            prev_right_edge_index:prev_left_edge_index
        ]

        # find same instruction
        if not (len(current_instruction_history) > len(prev_instruction_history) > 10):
            return False  # Not enough history to compare and not growth pattern
        if current_instruction_history[:10] != prev_instruction_history[:10]:
            return False  # Not the same instruction history, so not a growth pattern

        start_same_count = 10
        for i in range(start_same_count, len(prev_instruction_history)):
            if current_instruction_history[i] != prev_instruction_history[i]:
                start_same_count = i
                break

        # check end is same
        if (
            prev_instruction_history[start_same_count:]
            != current_instruction_history[
                start_same_count - len(prev_instruction_history) :
            ]
        ):
            return False  # Not the same instruction history, so not a growth pattern

        # exclude repeat pattern
        repeat_pattern = current_instruction_history[
            start_same_count : start_same_count - len(prev_instruction_history)
        ]
        # print("repeat_pattern:", repeat_pattern)
        if (
            len(repeat_pattern) > 0
            and repeat_pattern * 2
            == current_instruction_history[
                start_same_count - len(repeat_pattern) * 2 : start_same_count
            ]
        ):
            return True  # Repeat pattern, so growth pattern
        return False


class TransitionMachine:
    def __init__(self, state_num: int, initial_head_position: int = 100):
        self.state_num = state_num
        self.table: list[list[tuple[int, int, int] | None]] = [
            [(0, 0, -1) for _ in range(2)] for _ in range(state_num)
        ]
        self.machine = MachineState(initial_head_position)
        self.loop_shift: int | None = None

    def _get_instruction(self, instruction_code: str) -> tuple[int, int, int]:
        assert len(instruction_code) == 3, "Instruction code must be 3 characters long"
        value = int(instruction_code[0])
        direction = 1 if instruction_code[1] == "R" else -1
        state = ["H", "A", "B", "C", "D", "E", "F", "G", "H"].index(
            instruction_code[2]
        ) - 1

        assert value in [0, 1], "Value must be 0 or 1"
        assert direction in [-1, 1], "Direction must be 'L' or 'R'"
        assert -1 <= state < self.state_num, (
            "State must be between 0 and number of states - 1"
        )

        return (value, direction, state)

    def init_instructions(self, instructions: list[list[str | None]]) -> None:
        assert len(instructions) == self.state_num, (
            "Number of instructions must match the number of states"
        )
        for i, instruction_pair in enumerate(instructions):
            assert len(instruction_pair) == 2, "Each instruction must have two elements"

            for j in range(2):
                instruction = instruction_pair[j]
                if instruction is not None:
                    value, direction, state = self._get_instruction(instruction)
                    self.table[i][j] = (value, direction, state)
                else:
                    self.table[i][j] = None

    def check_unreachable_halt_or_none(self) -> bool:
        halt_or_none_reachable_state: set[int] = {
            s
            for s in range(self.state_num)
            if any(instr is None or instr[2] == -1 for instr in self.table[s])
        }

        if not halt_or_none_reachable_state:
            return True

        # Follow the actual initial execution path (tape starts all-zero)
        instr0 = self.table[0][0]
        if instr0 is None or instr0[2] == -1:
            return False
        instr1 = self.table[instr0[2]][0]
        if instr1 is None or instr1[2] == -1:
            return False

        searched: set[int] = set()

        def _find_reachable(state: int) -> bool:
            if state in halt_or_none_reachable_state:
                return True
            if state in searched:
                return False
            searched.add(state)
            for instr in self.table[state]:
                assert instr is not None and instr[2] != -1
                if _find_reachable(instr[2]):
                    return True
            return False

        return not _find_reachable(instr1[2])

    def move(self) -> tuple[int, int, int] | None:
        machine = self.machine
        state = machine.state
        value = machine.tape[machine.head_position]
        instruction = self.table[state][value]
        if instruction is not None:
            machine.write_and_move(*instruction)
        return instruction

    def run(
        self, max_steps: int = 1000, debug: bool = False
    ) -> tuple[int, tuple[int, int, int] | None]:
        instruction = None
        for i in range(max_steps):
            if debug:
                m = self.machine
                tape_val = m.tape[m.head_position]
                slot = f"{chr(ord('A') + m.state)}{tape_val}"
                next_instr = self.table[m.state][tape_val]
                lo = min(m.min_position, m.head_position)
                hi = max(m.max_position, m.head_position)
                tape_str = "".join(
                    f"[{m.tape[p]}]" if p == m.head_position else str(m.tape[p])
                    for p in range(lo, hi + 1)
                )
                if next_instr is None:
                    instr_name = "???"
                else:
                    v, d, s = next_instr
                    instr_name = f"{v}{'R' if d == 1 else 'L'}{'H' if s == -1 else chr(ord('A') + s)}"
                print(f"Step {i:3d}: {slot}  {tape_str}  → {instr_name}")

            instruction = self.move()
            if instruction is None:
                return (i + 1, None)

            if instruction[2] == -1:
                return i + 1, instruction  # halted

            loop_shift = self.machine.check_loop()
            if loop_shift is not None:
                self.loop_shift = loop_shift
                return -(i + 1), instruction  # loop detected

        return 0, instruction  # timeout


def main() -> None:
    import argparse
    import ast

    parser = argparse.ArgumentParser(
        description="Run a specific Busy Beaver machine",
        epilog=(
            "Examples:\n"
            "  bb_machine 1RB 1LB 1LA 0RH\n"
            "  bb_machine \"[['1RB', '1LB'], ['1LA', '0RH']]\""
        ),
    )
    parser.add_argument(
        "instructions",
        nargs="+",
        help="Flat: A0 A1 B0 B1 ... (use '_' for None)  OR  Python list string from CSV",
    )
    parser.add_argument("--max-steps", type=int, default=1000)
    parser.add_argument("--debug", action="store_true")
    args = parser.parse_args()

    raw: list[str] = args.instructions
    if len(raw) == 1 and raw[0].lstrip().startswith("["):
        parsed = ast.literal_eval(raw[0].strip())
        pattern: list[list[str | None]] = parsed
    else:
        if len(raw) % 2 != 0:
            print("Error: number of instructions must be even", file=sys.stderr)
            sys.exit(1)
        num_states_raw = len(raw) // 2
        pattern = [
            [
                None if raw[i * 2] == "_" else raw[i * 2],
                None if raw[i * 2 + 1] == "_" else raw[i * 2 + 1],
            ]
            for i in range(num_states_raw)
        ]

    num_states = len(pattern)
    m = TransitionMachine(num_states, initial_head_position=num_states * 25)
    m.init_instructions(pattern)
    unreachable = m.check_unreachable_halt_or_none()
    steps, instruction = m.run(max_steps=args.max_steps, debug=args.debug)

    tape = m.machine.tape
    lo, hi = m.machine.min_position, m.machine.max_position
    tape_str = "".join(str(x) for x in tape[lo : hi + 1])
    ones = tape_str.count("1")

    if unreachable:
        print("Halt unreachable (static)")
    if steps > 0 and instruction is not None and instruction[2] == -1:
        print(f"Halted: {steps} steps, {ones} ones written")
    elif steps < 0:
        print(f"Loop detected: {-steps} steps")
    elif steps > 0 and instruction is None:
        print(f"Undefined instruction: {steps} steps")
    else:
        print(f"Timeout: {args.max_steps} steps")
        if m.machine.check_sweep_growth_pattern():
            print("Growth pattern detected")
    print(f"Tape: {tape_str}")


if __name__ == "__main__":
    main()
