#include <argparse/argparse.hpp>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <unordered_map>
#include <vector>

#include "bb_assert.hpp"
#include "bb_machine.hpp"
#include "bb_macro.hpp"
#include "bb_stat.hpp"

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
    const std::string ASSERT_ENABLED_str = ASSERT_ENABLED ? "true" : "false";
    const std::string key = std::format("{},{},{}", version, max_steps, ASSERT_ENABLED_str);

    const auto get = [&](const std::string& k) {
        auto it = config.find(k);
        return it != config.end() ? it->second : std::string("0");
    };
    const std::string new_row =
        std::format("{},{},{},{:.3f},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}", version,
                    max_steps, ASSERT_ENABLED_str, elapsed_sec, get("candidates_tried"),
                    get("halt_count"), get("timeout_count"), get("check_loop_detected_count"),
                    get("unstoppable_checker_detected_count"), get("stat_setup_count"),
                    get("stat_setup_total_sec"), get("stat_run_count"), get("stat_run_total_sec"),
                    get("stat_dfs_count"), get("stat_dfs_total_sec"), get("stat_check_loop_count"),
                    get("stat_check_loop_total_sec"), get("stat_unstoppable_checker_count"),
                    get("stat_unstoppable_checker_total_sec"));

    const std::string header =
        "git_version,max_steps,ASSERT_ENABLED,elapsed_seconds,"
        "candidates_tried,halt_count,timeout_count,check_loop_detected_count,"
        "unstoppable_checker_detected_count,"
        "setup_count,setup_total_sec,run_count,run_total_sec,dfs_count,dfs_total_sec,"
        "check_loop_count,check_loop_total_sec,"
        "unstoppable_checker_count,unstoppable_checker_total_sec";
    if (lines.empty())
        lines.emplace_back(header);
    else if (lines[0] != header)
        lines[0] = header;

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
              size_t halt_count, size_t timeout_count, size_t check_loop_count,
              size_t unstoppable_checker_count, const std::vector<std::string>& max_patterns,
              const std::string& version) {
    std::filesystem::create_directories("results");
    const std::string path = std::format("results/{}states_{}steps.log", n_states, max_steps_arg);

    std::ofstream f(path);
    f << "git_tag: " << version << '\n';
    f << "ASSERT_ENABLED: " << (ASSERT_ENABLED ? "true" : "false") << '\n';
    f << "candidates_tried: " << candidates_tried << '\n';
    f << "max_steps: " << best_steps << '\n';
    f << "halt_count: " << halt_count << '\n';
    f << "timeout_count: " << timeout_count << '\n';
    f << "check_loop_detected_count: " << check_loop_count << '\n';
    f << "unstoppable_checker_detected_count: " << unstoppable_checker_count << '\n';
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
    [[nodiscard]] const std::vector<int>& get_index_stack() const { return index_stack_; }
    [[nodiscard]] int get_index_stack_size() const { return index_stack_size_; }

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

    bool check_meaningfulness() {
        const auto& instructions = transition_list_.get_instructions();
        const auto& index_stack = transition_list_.get_index_stack();
        int index_stack_size = transition_list_.get_index_stack_size();

        // check A0 is "R"
        if (instructions[0].dir != Dir::R)
            return false;

        // check B,C,Dの状態が歯抜けでないかチェック
        std::vector<bool> state_used(n_states_, false);

        int max_state = -1;
        for (int i = 0; i < index_stack_size; ++i) {
            const auto& instruction = instructions[index_stack[i]];
            assert(!instruction.is_halt());
            state_used[instruction.next] = true;
            if (static_cast<int>(instruction.next) > max_state)
                max_state = static_cast<int>(instruction.next);
        }

        // Bから最大の状態までがすべて使われているかチェック
        for (int s = 1; s < max_state; ++s) {
            if (!state_used[s])
                return false;
        }

        // Haltが到達可能かチェック
        // HaltがAにあって、Aに遷移可能な場合はOK
        bool halt_reachable = false;
        if (instructions[1].is_halt() && state_used[0])
            halt_reachable = true;
        // Haltが到達可能な状態にあればOK
        else {
            for (int s = 1; s <= max_state; ++s) {
                if (instructions[s * 2].is_halt() || instructions[s * 2 + 1].is_halt()) {
                    halt_reachable = true;
                    break;
                }
            }
        }

        return halt_reachable;
    }

    bool next() {
        while (true) {
            // get next possible instruction for the current slot
            const Instruction* next_instr = possible_instructions_.select_next();

            if (next_instr) {
                transition_list_.set(*next_instr);

                // check meaningfulness
                bool meaningful = check_meaningfulness();
                if (!meaningful)
                    continue;  // try next instruction for the current slot
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
            if (!check_meaningfulness())
                return next();
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
    const size_t max_candidates = std::stoull(config.at("max_candidates"));
    const bool save_time = config.count("save_time") > 0;
    const bool output_csv = config.count("output_csv") > 0;

    const Instruction halt = {.write = Symbol::ZERO, .dir = Dir::R, .next = -1};

    uint64_t best_steps = 0;
    std::vector<std::string> max_patterns;
    size_t candidates_tried = 0;
    size_t halt_count = 0;
    size_t timeout_count = 0;
    size_t check_loop_count = 0;
    size_t unstoppable_checker_count = 0;

    std::ofstream csv_file;
    if (output_csv) {
        std::filesystem::create_directories("results");
        csv_file.open(std::format("results/bb_{}_patterns.csv", n_states));
        csv_file << "pattern,steps,result,check_loop_step,check_loop_detected,"
                    "unstoppable_checker_detected,unstoppable_checker_step,"
                    "setup_usec,run_usec,check_loop_usec,unstoppable_checker_usec\n";
    }

    Stat stat_setup;
    Stat stat_run;
    Stat stat_dfs;
    Stat stat_check_loop;
    Stat stat_unstoppable_checker;

    const auto t_start = std::chrono::steady_clock::now();

    MachineEnumerator enumerator(n_states);

    for (int i = 0; max_candidates == 0 || candidates_tried < max_candidates; ++i) {
        if (i % 1000000 == 0) {
            std::println("enumerating... {}", i);
            const double elapsed =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - t_start).count();
            std::println("candidates_tried: {}, elapsed: {:.3f} sec", candidates_tried, elapsed);
            std::println("transitions: {}", table_to_notation(enumerator.get_instructions()));
        }

        stat_setup.start();
        BbMachine m(n_states, max_steps);
        for (int state = 0; state < n_states; ++state)
            for (int symbol = 0; symbol < 2; ++symbol) {
                const auto& instr = enumerator.get_instruction(state, static_cast<Symbol>(symbol));
                m.set_instruction(state, static_cast<Symbol>(symbol), instr);
            }
        stat_setup.stop();

        ++candidates_tried;

        stat_run.start();
        const auto& last_transition = m.run();
        stat_run.stop();

#ifdef DEBUG_PRINT
        std::println("result: {} steps, last transition: {}{}{}", m.count(),
                     static_cast<int>(last_transition.instr.write),
                     last_transition.instr.dir == Dir::R ? 'R' : 'L',
                     last_transition.instr.next == -1
                         ? 'H'
                         : static_cast<char>('A' + last_transition.instr.next));
#endif
        if (last_transition.instr.is_halt() == false) {
            stat_check_loop.start();
            int64_t loop_step = m.check_loop();
            stat_check_loop.stop();
            stat_unstoppable_checker.start();
            BbMachineUnstoppableChecker checker(m);
            const int64_t unstoppable_checker_step = checker.check();
            const bool unstoppable_checker_detected = unstoppable_checker_step >= 0;
            stat_unstoppable_checker.stop();
            const bool check_loop_detected = loop_step >= 0;
            if (check_loop_detected)
                ++check_loop_count;
            if (unstoppable_checker_detected)
                ++unstoppable_checker_count;
            if (!check_loop_detected && !unstoppable_checker_detected)
                ++timeout_count;
            if (output_csv) {
                const std::string notation = table_to_notation(m.get_instructions());
                const int64_t steps =
                    check_loop_detected ? loop_step : static_cast<int64_t>(m.count());
                const char* result =
                    (check_loop_detected || unstoppable_checker_detected) ? "loop" : "timeout";
                csv_file << notation << ',' << steps << ',' << result << ','
                         << (check_loop_detected ? std::to_string(loop_step) : std::string()) << ','
                         << (check_loop_detected ? "true" : "false") << ','
                         << (unstoppable_checker_detected ? "true" : "false") << ','
                         << (unstoppable_checker_detected ? std::to_string(unstoppable_checker_step)
                                                          : std::string())
                         << ','
                         << std::format("{:.3f},{:.3f},{:.3f},{:.3f}",
                                        stat_setup.last_ms() * 1000.0, stat_run.last_ms() * 1000.0,
                                        stat_check_loop.last_ms() * 1000.0,
                                        stat_unstoppable_checker.last_ms() * 1000.0)
                         << '\n';
            }
            stat_dfs.start();
            bool has_next = enumerator.next();
            stat_dfs.stop();
            if (!has_next)
                break;
        } else {
            ++halt_count;
            const std::string notation = table_to_notation(m.get_instructions());
            if (output_csv) {
                csv_file << notation << ',' << m.count() << ',' << "halt" << ",,false,false,,"
                         << std::format("{:.3f},{:.3f},,", stat_setup.last_ms() * 1000.0,
                                        stat_run.last_ms() * 1000.0)
                         << '\n';
            }
            if (m.count() > best_steps) {
                best_steps = m.count();
                max_patterns.clear();
                max_patterns.push_back(notation);
            } else if (m.count() == best_steps) {
                max_patterns.push_back(notation);
            }
            stat_dfs.start();
            bool has_next = enumerator.push_or_next(last_transition.state, last_transition.value);
            stat_dfs.stop();
            if (!has_next)
                break;
        }
    }

    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t_start).count();

    std::println("最大: {} steps", best_steps);
    std::println("candidates: {}", candidates_tried);
    std::println("halt:                         {}", halt_count);
    std::println("timeout:                      {}", timeout_count);
    std::println("check_loop detected:          {}", check_loop_count);
    std::println("unstoppable_checker detected: {}", unstoppable_checker_count);
    std::println("経過時間: {:.3f} 秒", elapsed);
#ifdef BB_STAT
    std::println("{}", stat_setup.format("stat_setup"));
    std::println("{}", stat_run.format("stat_run"));
    std::println("{}", stat_dfs.format("stat_dfs"));
    std::println("{}", stat_check_loop.format("stat_check_loop"));
    std::println("{}", stat_unstoppable_checker.format("stat_unstoppable_checker"));
#endif

    const std::string version = git_version();
    save_log(n_states, max_steps, best_steps, candidates_tried, halt_count, timeout_count,
             check_loop_count, unstoppable_checker_count, max_patterns, version);

    if (save_time) {
        Config c = config;
        c["git_version"] = version;
#ifdef BB_STAT
        c["stat_setup_count"] = std::to_string(stat_setup.count);
        c["stat_setup_total_sec"] = std::format("{:.6f}", stat_setup.total_ms / 1000.0);
        c["stat_run_count"] = std::to_string(stat_run.count);
        c["stat_run_total_sec"] = std::format("{:.6f}", stat_run.total_ms / 1000.0);
        c["stat_dfs_count"] = std::to_string(stat_dfs.count);
        c["stat_dfs_total_sec"] = std::format("{:.6f}", stat_dfs.total_ms / 1000.0);
        c["stat_check_loop_count"] = std::to_string(stat_check_loop.count);
        c["stat_check_loop_total_sec"] = std::format("{:.6f}", stat_check_loop.total_ms / 1000.0);
        c["stat_unstoppable_checker_count"] = std::to_string(stat_unstoppable_checker.count);
        c["stat_unstoppable_checker_total_sec"] =
            std::format("{:.6f}", stat_unstoppable_checker.total_ms / 1000.0);
#endif
        c["candidates_tried"] = std::to_string(candidates_tried);
        c["halt_count"] = std::to_string(halt_count);
        c["timeout_count"] = std::to_string(timeout_count);
        c["check_loop_detected_count"] = std::to_string(check_loop_count);
        c["unstoppable_checker_detected_count"] = std::to_string(unstoppable_checker_count);
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
    app.add_argument("--max-candidates")
        .help("max candidates to try (0 = unlimited)")
        .default_value(static_cast<size_t>(0))
        .scan<'u', size_t>();
    app.add_argument("--output-csv")
        .help("save all halting patterns to results/bb_{n}_patterns.csv")
        .default_value(false)
        .implicit_value(true);
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
    config["max_candidates"] = std::to_string(app.get<size_t>("--max-candidates"));
    if (app.get<bool>("--output-csv"))
        config["output_csv"] = "1";
    if (app.get<bool>("--save-time"))
        config["save_time"] = "1";

    std::println("n={}, max_steps={}", config["n_states"], config["max_steps"]);
    search(config);
}
