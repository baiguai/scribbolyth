#pragma once

#include <ftxui/component/event.hpp>
#include <map>
#include <utility>
#include <vector>
#include <string>

/*!
    Synthetic Event Detection

    FTXUI delivers its own internal events (terminal resize, screen wakeups)
    as "special" events carrying a single NUL byte. They look like a key
    press but are not one: treating them as a key aborts the pending key
    sequence (e.g. "d d") and drops a pending count prefix.

    ----
*/
//>>
inline bool IsSyntheticEvent(const ftxui::Event& event)
{
    const std::string& input = event.input();
    return input.size() == 1 && input[0] == '\0';
}
//<<
//!

/*!
    Binding Struct

    ----
*/
//>>
struct Binding
{
    std::string op;
    std::string command;
    std::string args;
    bool can_repeat = false;
};
//<<
//!

/*!
    Keymap Class
*/
class Keymap
{
    public:
        /*!
            Public Members
        */

        /*!
            Ftxui Event Wrappers

            ----
        */
        //>>
        void Bind(ftxui::Event event, Binding binding);
        void Bind(std::vector<ftxui::Event> sequence, Binding binding);
        //<<
        //!

        /*!
            Result Struct
        */
        //>>
        struct Result
        {
            std::string op;
            std::string command;
            std::string args;
            bool pending = false;
            int count = 1;
        };
        //<<
        //!

        /*!
            Forward Definitions
        */
        //>>
        Result Handle(ftxui::Event event);
        void ResetPending() { pending_.clear(); }
        void EnableCounts() { enable_count_ = true; }
        //<<
        //!

        //!

    private:
        /*!
            Private Members
        */

        /*!
            Variables

            ----
        */
        //>>
        std::map<std::string, Binding> single_bindings_;
        std::map<std::vector<std::string>, Binding> sequence_bindings_;
        std::vector<std::string> pending_;
        bool enable_count_ = false;
        int count_ = 0;
        //<<
        //!

        //!
};
//!
