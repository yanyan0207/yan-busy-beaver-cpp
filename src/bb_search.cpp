#include <argparse/argparse.hpp>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <unordered_map>
#include <vector>

#include "bb_machine.hpp"

using Config = std::unordered_map<std::string, std::string>;

std::string git_version() {
    std::array<char, 128> buf{};
    std::string result;
    FILE* pipe = _popen("git describe --tags 2>nul", "r");
    if (!pipe)
        return "unknown";
    while (fgets(buf.data(), buf.size(), pipe))
        result += buf.data();
    _pclose(pipe);
    if (!result.empty() && result.back() == '\n')
        result.pop_back();
    return result.empty() ? "unknown" : result;
}

void save_csv(int n_states, const Config& config, double elapsed_sec) {
    std::filesystem::create_directories("results");
    const std::string path = std::format("results/bb_{}.csv", n_states);

    std::vector<std::string> lines;
    bool updated = false;
    {
        std::ifstream f(path);
        std::string line;
        while (std::getline(f, line))
            lines.push_back(line);
    }

    const std::string version = config.at("git_version");
    const std::string max_steps = config.at("max_steps");
    const std::string key = std::format("{},{}", version, max_steps);
    const std::string new_row = std::format("{},{},{:.3f}", version, max_steps, elapsed_sec);

    if (lines.empty())
        lines.emplace_back("git_version,max_steps,elapsed_seconds");

    for (auto& l : lines) {
        if (l.starts_with(key)) {
            l = new_row;
            updated = true;
            break;
        }
    }
    if (!updated)
        lines.push_back(new_row);

    std::ofstream f(path);
    for (const auto& l : lines)
        f << l << '\n';

    std::println("saved: {}", path);
}

std::string table_to_notation(const std::vector<Instruction>& table) {
    std::string result;
    for (size_t i = 0; i < table.size(); ++i) {
        if (i > 0)
            result += ' ';
        const auto& t = table[i];
        if (t.next == -1)
            result += std::format("{}{}H", static_cast<int>(t.write), t.dir == Dir::R ? 'R' : 'L');
        else
            result += std::format("{}{}{}", static_cast<int>(t.write), t.dir == Dir::R ? 'R' : 'L',
                                  static_cast<char>('A' + t.next));
    }
    return result;
}

void save_log(int n_states, size_t max_steps_arg, uint64_t best_steps, size_t candidates_tried,
              size_t timeout_count, const std::vector<std::string>& max_patterns,
              const std::string& version) {
    std::filesystem::create_directories("results");
    const std::string path = std::format("results/{}states_{}steps.log", n_states, max_steps_arg);

    std::ofstream f(path);
    f << "git_tag: " << version << '\n';
    f << "candidates_tried: " << candidates_tried << '\n';
    f << "max_steps: " << best_steps << '\n';
    f << "timeout_count: " << timeout_count << '\n';
    f << "max_patterns: [";
    for (size_t i = 0; i < max_patterns.size(); ++i) {
        if (i > 0)
            f << ", ";
        f << '"' << max_patterns[i] << '"';
    }
    f << "]\n";
    std::println("saved: {}", path);
}

const Instruction HALT = {.write = Symbol::ZERO, .dir = Dir::R, .next = -1};

class PossibleInstructions {
public:
    PossibleInstructions(int n_states) {
        for (Symbol w : {Symbol::ZERO, Symbol::ONE})
            for (Dir d : {Dir::L, Dir::R})
                for (State s = 0; s < n_states; ++s)
                    instructions_.push_back({.write = w, .dir = d, .next = s});
        size_ = static_cast<int>(instructions_.size());
        std::println("PossibleInstructions: {} instructions", table_to_notation(instructions_));
    }

    [[nodiscard]] const Instruction* select_next() {
        if (current_index_ + 1 < size_) {
            current_index_++;
            return &instructions_[current_index_];
        } else {
            return nullptr;
        }
    }
    void reset() { current_index_ = 0; }
    void set_index(const Instruction& instr) {
        for (int i = 0; i < size_; ++i) {
            if (instructions_[i].write == instr.write && instructions_[i].dir == instr.dir &&
                instructions_[i].next == instr.next) {
                current_index_ = i;
                return;
            }
        }
        assert(false && "Transition not found in possible instructions");
        current_index_ = -1;  // not found
    }
    [[nodiscard]] int get_index() const { return current_index_; }
    [[nodiscard]] int size() const { return size_; }

    [[nodiscard]] const Instruction& get(int index) const { return instructions_[index]; }

private:
    std::vector<Instruction> instructions_;
    int size_ = 0;
    int current_index_ = 0;
};

class TransitionList {
public:
    TransitionList(int n_states) : table_size_(n_states * 2) {
        for (int i = 0; i < table_size_; ++i) {
            transitions_.push_back(HALT);
            index_stack_.push_back(-1);
        }
    }

    bool push(State state, Symbol value, const Instruction& instr) {
        assert(index_stack_size_ <= table_size_);
        if (index_stack_size_ == table_size_ - 1)
            return false;

        int index = state * 2 + static_cast<int>(value);
        index_stack_[index_stack_size_++] = index;
        assert(transitions_[index].is_halt() && "Transition already set for this state and symbol");
        transitions_[index] = instr;
        return true;
    }

    void set(const Instruction& instr) {
        assert(index_stack_size_ > 0);
        transitions_[index_stack_[index_stack_size_ - 1]] = instr;
    }

    void pop() {
        assert(index_stack_size_ > 0);
        int index = index_stack_[--index_stack_size_];
        transitions_[index] = HALT;
    }

    [[nodiscard]] const Instruction& last_instruction() const {
        assert(index_stack_size_ > 0);
        int index = index_stack_[index_stack_size_ - 1];
        return transitions_[index];
    }

    [[nodiscard]] int last() const {
        assert(index_stack_size_ > 0);
        return index_stack_[index_stack_size_ - 1];
    }

    [[nodiscard]] bool empty() const { return index_stack_size_ == 0; }
    [[nodiscard]] const Instruction& get(State state, Symbol value) const {
        return transitions_[state * 2 + static_cast<int>(value)];
    }
    [[nodiscard]] const std::vector<Instruction>& get_instructions() const { return transitions_; }

private:
    std::vector<Instruction> transitions_;
    std::vector<int> index_stack_;
    int index_stack_size_ = 0;
    int table_size_ = 0;
};

class MachineEnumerator {
public:
    MachineEnumerator(int n_states)
        : n_states_(n_states), possible_instructions_(n_states), transition_list_(n_states) {
        slots_num_ = n_states_ * 2;

        const auto& fist_instr = possible_instructions_.get(0);
        transition_list_.push(0, Symbol::ZERO, fist_instr);
    }

    bool next() {
        while (true) {
            // get next possible instruction for the current slot
            const Instruction* next_instr = possible_instructions_.select_next();

            if (next_instr) {
                transition_list_.set(*next_instr);
                return true;
            } else {
                if (!pop())
                    return false;
            }
        }
    }

    bool push_or_next(State state, Symbol value) {
        bool pushed = transition_list_.push(state, value, possible_instructions_.get(0));
        if (pushed) {
            possible_instructions_.reset();
            return true;
        } else {
            return next();
        }
    }

    [[nodiscard]] const Instruction& get_instruction(State state, Symbol value) const {
        return transition_list_.get(state, value);
    }

    [[nodiscard]] const std::vector<Instruction>& get_instructions() const {
        return transition_list_.get_instructions();
    }

    bool pop() {
        transition_list_.pop();
        if (!transition_list_.empty()) {
            possible_instructions_.set_index(transition_list_.last_instruction());
            return true;
        }
        return false;
    }

private:
    TransitionList transition_list_;
    PossibleInstructions possible_instructions_;
    int n_states_;
    int slots_num_ = 0;
};

void search(const Config& config) {
    const int n_states = std::stoi(config.at("n_states"));
    const size_t max_steps = std::stoull(config.at("max_steps"));
    const bool save_time = config.count("save_time") > 0;

    const Instruction halt = {.write = Symbol::ZERO, .dir = Dir::R, .next = -1};

    uint64_t best_steps = 0;
    std::vector<std::string> max_patterns;
    size_t candidates_tried = 0;
    size_t timeout_count = 0;

    const auto t_start = std::chrono::steady_clock::now();

    MachineEnumerator enumerator(n_states);

    for (int i = 0;; ++i) {
        if (i % 1000000 == 0) {
            std::println("enumerating... {}", i);
            const double elapsed =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - t_start).count();
            std::println("candidates_tried: {}, elapsed: {:.3f} sec", candidates_tried, elapsed);
            std::println("transitions: {}", table_to_notation(enumerator.get_instructions()));
        }

        BbMachine m(n_states, max_steps);
        for (int state = 0; state < n_states; ++state)
            for (int symbol = 0; symbol < 2; ++symbol) {
                const auto& instr = enumerator.get_instruction(state, static_cast<Symbol>(symbol));
                m.set_instruction(state, static_cast<Symbol>(symbol), instr);
            }

        ++candidates_tried;
        const auto& last_transition = m.run();
#ifdef DEBUG_PRINT
        std::println("result: {} steps, last transition: {}{}{}", m.count(),
                     static_cast<int>(last_transition.instr.write),
                     last_transition.instr.dir == Dir::R ? 'R' : 'L',
                     last_transition.instr.next == -1
                         ? 'H'
                         : static_cast<char>('A' + last_transition.instr.next));
#endif
        if (last_transition.instr.is_halt() == false) {
            ++timeout_count;
            bool has_next = enumerator.next();
            if (!has_next) {
                break;
            }
        } else {
            if (m.count() > best_steps) {
                best_steps = m.count();
                max_patterns.clear();
                max_patterns.push_back(table_to_notation(m.get_instructions()));
            } else if (m.count() == best_steps) {
                max_patterns.push_back(table_to_notation(m.get_instructions()));
            }
            bool has_next = enumerator.push_or_next(last_transition.state, last_transition.value);
            if (!has_next) {
                break;
            }
        }
    }

    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t_start).count();

    std::println("最大: {} steps", best_steps);
    std::println("経過時間: {:.3f} 秒", elapsed);

    const std::string version = git_version();
    save_log(n_states, max_steps, best_steps, candidates_tried, timeout_count, max_patterns,
             version);

    if (save_time) {
        Config c = config;
        c["git_version"] = version;
        save_csv(n_states, c, elapsed);
    }
}

int main(int argc, char* argv[]) {
    argparse::ArgumentParser app("bb_search");

    app.add_argument("n_states").help("number of states").scan<'i', int>();
    app.add_argument("max_steps")
        .help("max steps")
        .default_value(static_cast<size_t>(1000))
        .scan<'u', size_t>();
    app.add_argument("--save-time")
        .help("save elapsed time to CSV")
        .default_value(false)
        .implicit_value(true);

    try {
        app.parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::println(stderr, "{}", e.what());
        std::println(stderr, "{}", app.help().str());
        return 1;
    }

    Config config;
    config["n_states"] = std::to_string(app.get<int>("n_states"));
    config["max_steps"] = std::to_string(app.get<size_t>("max_steps"));
    if (app.get<bool>("--save-time"))
        config["save_time"] = "1";

    std::println("n={}, max_steps={}", config["n_states"], config["max_steps"]);
    search(config);
}
