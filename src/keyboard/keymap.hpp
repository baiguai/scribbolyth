#pragma once

#include <ftxui/component/event.hpp>
#include <map>
#include <vector>
#include <string>
#include "action.hpp"

class Keymap {
public:
    void Bind(ftxui::Event event, Action action);
    void Bind(std::vector<ftxui::Event> sequence, Action action);

    struct Result {
        Action action;
        bool pending;
    };

    Result Handle(ftxui::Event event);
    void ResetPending() { pending_.clear(); }

private:
    std::map<std::string, Action> singles_;
    std::map<std::vector<std::string>, Action> sequences_;
    std::vector<std::string> pending_;
};
