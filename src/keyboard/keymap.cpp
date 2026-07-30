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

    // Check if completing a pending multi-key sequence
    if (!pending_.empty()) {
        auto candidate = pending_;
        candidate.push_back(input);
        auto it = sequences_.find(candidate);
        if (it != sequences_.end()) {
            pending_.clear();
            return {it->second, false};
        }
        // Sequence broken — clear and fall through
        pending_.clear();
    }

    // Direct match
    {
        auto it = singles_.find(input);
        if (it != singles_.end())
            return {it->second, false};
    }

    // Could this start a multi-key sequence?
    for (auto& [seq, _] : sequences_) {
        if (!seq.empty() && seq[0] == input) {
            pending_ = {input};
            return {Action::None, true};
        }
    }

    return {Action::None, false};
}
