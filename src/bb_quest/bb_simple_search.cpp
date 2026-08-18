#include <argparse/argparse.hpp>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <print>
#include <string>

#include "../bb_types.hpp"
#include "bb_simple_machine.hpp"

struct Config {
    int n_states;
    int64_t max_steps;
    int64_t max_candidates;
};
static std::vector<Instruction> instruction_candidates;

struct OutputFiles {
    std::ofstream skipped;
    std::ofstream success;
    std::ofstream failure;
};

struct InstructionStackMember {
    int instruction_index;
    State state;
    Symbol symbol;
    Instruction instr;
};

InstructionStackMember index_to_stack_member(int index, int n_states, State state, Symbol symbol) {
    Instruction instr = instruction_candidates[index];
    return {.instruction_index = index, .state = state, .symbol = symbol, .instr = instr};
}

using InstructionStack = std::vector<InstructionStackMember>;

// static void load_csv(std::string filename) {}

static void push_instruction_stack(InstructionStack* pcurrent_stack, int n_states, State next_state,
                                   Symbol next_symbol) {
    InstructionStack& current_stack = *pcurrent_stack;
    assert(current_stack.size() < n_states * 2 - 1);
    InstructionStackMember next = index_to_stack_member(0, n_states, next_state, next_symbol);
    current_stack.push_back(next);
}

static bool next_instruction_stack(InstructionStack* pcurrent_stack, int n_states,
                                   int finish_stack_size) {
    InstructionStack& current_stack = *pcurrent_stack;
    assert(current_stack.size() > 0 && current_stack.size() < n_states * 2);

    while (true) {
        int& last_instruction_index = current_stack[current_stack.size() - 1].instruction_index;
        if (last_instruction_index < instruction_candidates.size() - 1) {
            current_stack[current_stack.size() - 1].instr =
                instruction_candidates[++last_instruction_index];
            return true;
        } else if (last_instruction_index == instruction_candidates.size() - 1) {
            current_stack.pop_back();
            if (current_stack.size() == finish_stack_size)
                return false;
        } else {
            assert(false);
        }
    }
}

static TransitionTable create_table(int n_states, const InstructionStack& instruction_stack) {
    TransitionTable table(n_states);
    for (const InstructionStackMember& instr_member : instruction_stack) {
        table.set_instruction(instr_member.state, instr_member.symbol, instr_member.instr);
    }
    return table;
}

static bool to_test(const std::vector<Instruction>& instructions) {
    int n_states = static_cast<int>(instructions.size() / 2);
    // check A0 is "R"
    if (instructions[0].dir != Dir::R)
        return false;

    // AがA、BがB、CがC...にしか遷移しなかったら状態から脱出できないので不要
    for (int s = 0; s < n_states; ++s) {
        const Instruction& instr0 = instructions[s * 2 + 0];
        const Instruction& instr1 = instructions[s * 2 + 1];
        if (instr0.next == s && instr1.next == s)
            return false;
    }

    // BとCが全く同じなど
    // BとCのwrite/dirが同じで、遷移先が->C ->Bなど等価な場合
    for (int s1 = 1; s1 < n_states - 1; ++s1) {
        for (int s2 = s1 + 1; s2 < n_states; ++s2) {
            const Instruction& instr0_sym0 = instructions[s1 * 2 + 0];
            const Instruction& instr0_sym1 = instructions[s1 * 2 + 1];
            const Instruction& instr1_sym0 = instructions[s2 * 2 + 0];
            const Instruction& instr1_sym1 = instructions[s2 * 2 + 1];
            if (instr0_sym0.is_halt() || instr0_sym1.is_halt() || instr1_sym0.is_halt() ||
                instr1_sym1.is_halt()) {
            } else if (instr0_sym0.write != instr1_sym0.write ||
                       instr0_sym0.dir != instr1_sym0.dir ||
                       instr0_sym1.write != instr1_sym1.write ||
                       instr0_sym1.dir != instr1_sym1.dir) {
            } else {
                bool same0 = false;
                bool same1 = false;
                if (instr0_sym0.next == instr1_sym0.next)
                    same0 = true;
                else if ((instr0_sym0.next == s1 || instr0_sym0.next == s2) &&
                         (instr1_sym0.next == s1 || instr1_sym0.next == s2))
                    same0 = true;
                if (instr0_sym1.next == instr1_sym1.next)
                    same1 = true;
                else if ((instr0_sym1.next == s1 || instr0_sym1.next == s2) &&
                         (instr1_sym1.next == s1 || instr1_sym1.next == s2))
                    same1 = true;

                if (same0 && same1)
                    return false;
            }
        }
    }

    // check B,C,Dの状態が歯抜けでないかチェック
    std::vector<bool> state_used;
    state_used.resize(n_states);
    for (const auto& instr : instructions) {
        if (!instr.is_halt()) {
            state_used[instr.next] = true;
        }
    }

    // Bから最大の状態までがすべて使われているかチェック
    for (int s = 1; s < n_states - 1; ++s) {
        if (!state_used[s] && state_used[s + 1])
            return false;
    }

    // Haltが到達可能かチェック
    // HaltがAにあって、Aに遷移可能な場合はOK
    bool halt_reachable = false;
    if (instructions[1].is_halt() && state_used[0])
        halt_reachable = true;
    // Haltが到達可能な状態にあればOK
    else {
        for (int s = 1; s < n_states; ++s) {
            if (state_used[s] &&
                (instructions[s * 2].is_halt() || instructions[s * 2 + 1].is_halt())) {
                halt_reachable = true;
                break;
            }
        }
    }

    return halt_reachable;
}

class StackTransitionTableGenerator {
    InstructionStack current_stack;
    int root_stack_size;
    int n_states_;
    std::ofstream& skipped_file_;

public:
    StackTransitionTableGenerator(int n_states, const InstructionStack& root_stack,
                                  std::ofstream& skipped_file)
        : n_states_(n_states),
          current_stack(root_stack),
          root_stack_size(static_cast<int>(root_stack.size())),
          skipped_file_(skipped_file) {}

    const InstructionStack* next(State next_state, Symbol next_symbol) {
        if (current_stack.size() < n_states_ * 2 - 1) {
            push_instruction_stack(&current_stack, n_states_, next_state, next_symbol);
            if (to_test(create_table(n_states_, current_stack).get_instructions())) {
                return &current_stack;
            } else {
                skipped_file_ << table_to_notation(
                                     create_table(n_states_, current_stack).get_instructions())
                              << ",skipped\n";
            }
        }
        return next();
    }
    const InstructionStack* next() {
        while (true) {
            bool next = next_instruction_stack(&current_stack, n_states_, root_stack_size);
            if (!next)
                return nullptr;
            TransitionTable table = create_table(n_states_, current_stack);
            bool skipped = !to_test(table.get_instructions());
            if (skipped)
                skipped_file_ << table_to_notation(
                                     create_table(n_states_, current_stack).get_instructions())
                              << ",skipped\n";
            else
                return &current_stack;
        }
    }
};

static void quest_stack_search(int n_states, const InstructionStack& root_stack, int64_t max_steps,
                               int64_t max_candidates, OutputFiles* output_files) {
    int64_t candidates_tried = 0;

    // root_stackの実行
    State next_state = 0;
    Symbol next_symbol = Symbol::ZERO;
    if (root_stack.size()) {
        TransitionTable table = create_table(n_states, root_stack);
        if (!to_test(table.get_instructions())) {
            assert(false);
            return;
        }
        BbSimpleMachine m;
        m.init(max_steps * 2 + 1, &table);
        const Transition transition = m.run(max_steps);
        const Instruction& instr = transition.instr;
        candidates_tried++;
        if (instr.is_halt()) {
            output_files->success << table_to_notation(table.get_instructions()) << ",halt,"
                                  << m.count() << '\n';
            next_state = transition.state;
            next_symbol = transition.value;
        } else {
            output_files->failure << table_to_notation(table.get_instructions()) << ",unresolved,"
                                  << m.count() << '\n';
            return;
        }
    }

    StackTransitionTableGenerator table_generator(n_states, root_stack, output_files->skipped);
    while (true) {
        const InstructionStack* current_stack;
        if (next_state == -1) {
            current_stack = table_generator.next();
        } else {
            current_stack = table_generator.next(next_state, next_symbol);
        }
        if (!current_stack) {
            break;
        }

        TransitionTable table = create_table(n_states, *current_stack);
        std::string table_str = table_to_notation(table.get_instructions());
        BbSimpleMachine m;
        m.init(max_steps * 2 + 1, &table);
        const Transition transition = m.run(max_steps);
        const Instruction& instr = transition.instr;

        if (instr.is_halt()) {
            output_files->success << table_str << ",halt," << m.count() << '\n';
            next_state = transition.state;
            next_symbol = transition.value;
        } else {
            output_files->failure << table_str << ",unresolved," << m.count() << '\n';
            next_state = -1;
        }
        if (++candidates_tried >= max_candidates && max_candidates > 0) {
            std::println("max_candidates reached: {}", max_candidates);
            return;
        }
    }
}

static void quest_main(const Config& config) {
    const auto output_dir =
        std::filesystem::path{std::format("results/bb_quest_{}/01_simple_search", config.n_states)};

    std::filesystem::create_directories(output_dir);
    OutputFiles output_files{
        .skipped = std::ofstream(
            std::format("results/bb_quest_{}/01_simple_search/skipped.csv", config.n_states)),
        .success = std::ofstream(
            std::format("results/bb_quest_{}/01_simple_search/decided.csv", config.n_states)),
        .failure = std::ofstream(
            std::format("results/bb_quest_{}/01_simple_search/unresolved.csv", config.n_states)),
    };

    output_files.skipped << "pattern,result" << '\n';
    output_files.success << "pattern,result,steps" << '\n';
    output_files.failure << "pattern,result,steps" << '\n';
    if (!output_files.skipped || !output_files.success || !output_files.failure) {
        std::println(stderr, "failed to open output files");
        return;
    }

    InstructionStack stack;
    quest_stack_search(config.n_states, stack, config.max_steps, config.max_candidates,
                       &output_files);
}

int main(int argc, char* argv[]) {
    argparse::ArgumentParser app("bb_quest");

    app.add_argument("n_states").help("number of states").scan<'i', int>();
    app.add_argument("max_steps")
        .help("max steps")
        .default_value(static_cast<int64_t>(1000))
        .scan<'i', int64_t>();
    app.add_argument("--max-candidates")
        .help("max candidates to try (0 = unlimited)")
        .default_value(static_cast<int64_t>(0))
        .scan<'i', int64_t>();

    try {
        app.parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::println(stderr, "{}", e.what());
        std::println(stderr, "{}", app.help().str());
        return 1;
    }

    Config config;
    config.n_states = app.get<int>("n_states");
    config.max_steps = app.get<int64_t>("max_steps");
    config.max_candidates = app.get<int64_t>("--max-candidates");

    for (int symbol = 0; symbol < 2; symbol++) {
        for (int dir = 0; dir < 2; dir++) {
            for (int state = 0; state < config.n_states; state++) {
                Instruction instr = {.write = static_cast<Symbol>(symbol),
                                     .dir = dir == 0 ? Dir::L : Dir::R,
                                     .next = state};
                instruction_candidates.push_back(instr);
            }
        }
    }

    std::println("n={}, max_steps={} max_candidates={}", config.n_states, config.max_steps,
                 config.max_candidates);
    quest_main(config);
}
