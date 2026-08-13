#include "bb_machine.hpp"

#include <stdexcept>

BbMachine::Result BbMachine::run() {
    for (size_t i = count_; i < max_steps_; ++i) {
        count_++;
        const auto& instruction = table_.get_instruction(state_, tape_.read());

        if (instruction.next == -1) {
            return Result::HALT;  // halt
        }

        tape_.write_and_move(instruction.write, instruction.dir);
        state_ = instruction.next;
    }
    return Result::MAX_STEPS_EXCEEDED;  // max steps exceeded
}
