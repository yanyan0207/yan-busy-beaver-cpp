#include <argparse/argparse.hpp>
#include <print>
#include <stdexcept>
#include <string>
#include <vector>

#include "bb_machine.hpp"

class DebugBbMachine : public BbMachine {
public:
    using BbMachine::BbMachine;

    Transition run_debug() {
        for (size_t i = this->count(); i < this->max_steps(); ++i) {
            const auto prev_state = this->current_state();
            const auto prev_sym = this->current_symbol();
            const auto& instr = this->move_one_step();

            const char next_char = instr.next == -1 ? 'H' : static_cast<char>('A' + instr.next);
            std::println("step {:6}: {}{} -> {}{}{}  {}", this->count(),
                         static_cast<char>('A' + prev_state), static_cast<int>(prev_sym),
                         static_cast<int>(instr.write), instr.dir == Dir::R ? 'R' : 'L', next_char,
                         this->format_tape());

            if (instr.is_halt())
                return {.state = prev_state, .value = prev_sym, .instr = instr};
        }
        // timeout: return current state without executing
        const auto& instrs = this->get_instructions();
        const auto s = this->current_state();
        const auto v = this->current_symbol();
        return {.state = s, .value = v, .instr = instrs[s * 2 + static_cast<int>(v)]};
    }
};

// 標準BB表記をパース: "1RB 1LB 1LA 1RH"
void parse(const std::string& notation, DebugBbMachine** m, size_t max_steps) {
    std::vector<std::string> tokens;
    size_t i = 0;
    while (i < notation.size()) {
        while (i < notation.size() && notation[i] == ' ')
            ++i;
        if (i + 3 <= notation.size()) {
            tokens.push_back(notation.substr(i, 3));
            i += 3;
        } else
            break;
    }

    if (tokens.size() % 2 != 0)
        throw std::runtime_error("invalid notation");
    const int num_states = static_cast<int>(tokens.size() / 2);

    *m = new DebugBbMachine(num_states, max_steps);

    for (int state = 0; state < num_states; ++state) {
        for (int read = 0; read < 2; ++read) {
            const auto& tok = tokens[state * 2 + read];
            const int write = tok[0] - '0';
            const auto dir = (tok[1] == 'R') ? Dir::R : Dir::L;
            const int next = (tok[2] == 'H') ? -1 : (tok[2] - 'A');
            (*m)->set_instruction(state, static_cast<Symbol>(read),
                                  {.write = static_cast<Symbol>(write), .dir = dir, .next = next});
        }
    }
}

int main(int argc, char* argv[]) {
    argparse::ArgumentParser app("bb");
    app.add_argument("notation").help("BB notation (e.g. \"1RB 1LB 1LA 1RH\")");
    app.add_argument("--max-steps")
        .help("max steps before timeout")
        .default_value(static_cast<size_t>(100))
        .scan<'u', size_t>();
    app.add_argument("--debug")
        .help("print each step with tape")
        .default_value(false)
        .implicit_value(true);

    try {
        app.parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::println(stderr, "{}", e.what());
        std::println(stderr, "{}", app.help().str());
        return 1;
    }

    const auto max_steps = app.get<size_t>("--max-steps");
    const bool debug = app.get<bool>("--debug");
    const auto notation = app.get<std::string>("notation");

    std::println("notation:  {}", notation);
    std::println("max_steps: {}", max_steps);

    DebugBbMachine* m = nullptr;
    parse(notation, &m, max_steps);

    std::println("running...");
    const auto result = debug ? m->run_debug() : m->run();
    std::println("steps: {}", m->count());

    if (result.instr.is_halt()) {
        std::println("result: halt");
    } else {
        std::println("result: timeout — running check_loop...");
        const bool loop = m->check_loop();
        std::println("check_loop: {}", loop ? "loop detected" : "no loop detected");
    }

    delete m;
}
