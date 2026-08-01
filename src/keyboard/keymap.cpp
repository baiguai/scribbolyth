#include "keymap.hpp"

void Keymap::Bind(ftxui::Event event, Action action) {
    singles_[event.input()] = action;
}

void Keymap::Bind(std::vector<ftxui::Event> sequence, Action action) {
    std::vector<std::string> keys;
    keys.reserve(sequence.size());
    for (auto& e : sequence)
        keys.push_back(e.input());
    sequences_[keys] = action;
}

Keymap::Result Keymap::Handle(ftxui::Event event) {
    auto& input = event.input();

    // Count prefix
    if (enable_count_ && input.size() == 1 && input[0] >= '0' && input[0] <= '9')
    {
        int digit = input[0] - '0';
        if (digit >= 1 || count_ > 0)
        {
            count_ = count_ * 10 + digit;
            return {Action::None, true};
        }
    }

    auto finish = [&](Action action) -> Result
    {
        int c = count_;
        count_ = 0;
        if (c > 0) return {action, false, c};
        return {action, false};
    };

    // Check if completing a pending multi-key sequence
    if (!pending_.empty())
    {
        auto candidate = pending_;
        candidate.push_back(input);
        auto it = sequences_.find(candidate);
        if (it != sequences_.end()) {
            pending_.clear();
            return finish(it->second);
        }
        // Sequence broken — clear and fall through
        pending_.clear();
    }

    // Direct match
    {
        auto it = singles_.find(input);
        if (it != singles_.end())
            return finish(it->second);
    }

    // Could this start a multi-key sequence?
    for (auto& [seq, _] : sequences_) {
        if (!seq.empty() && seq[0] == input) {
            pending_ = {input};
            return {Action::None, true};
        }
    }

    count_ = 0;
    return {Action::None, false};
}
