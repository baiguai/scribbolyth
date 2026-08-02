# Keybindings & Commands

scribbolyth's key bindings and `:` commands are defined in one config file:

    config/commands.conf

`build.sh` copies it to the output directories so it always sits next to the
app executable (`build/bin/` on Linux, `build-windows/bin/` on Windows).

---

## Format

One entry per line:

    mode  key  repeat  command  args  op  function

Blank lines and lines starting with `#` are ignored.

| Column    | Meaning                                                                                                   |
|-----------|-----------------------------------------------------------------------------------------------------------|
| `mode`    | `TREE`, `NORMAL`, `INSERT`, `VISUAL` — binds `key` in that mode. `GLOBAL` — registers a command only (no key). |
| `key`     | The key that triggers the binding. `-` means no key.                                                      |
| `repeat`  | `yes`/`no` — whether the key accepts a count prefix (`3 j`).                                              |
| `command` | Optional `:name` usable from the command line. `-` means no command-line access.                          |
| `args`    | `-`, `prompt`, or a literal string. See below.                                                            |
| `op`      | The operation name the loader dispatches on (the registered handler). `-` means no dispatch.              |
| `function`| The ACTUAL function invoked when the entry triggers. `-` means not implemented yet.                       |

### `key` values

- A single character: `j`, `:`, `0`, `$`, etc.
- A special token: `Esc`, `Return`, `Tab`, `Backspace`, `Space`,
  `ArrowUp`, `ArrowDown`, `ArrowLeft`, `ArrowRight`,
  `PageUp`, `PageDown`, `Home`, `End`.
- A quoted multi-key sequence: `"g g"` (keys separated by spaces).

### `args` semantics

- `-` — no arguments are passed. The function is called with an empty string.
- `prompt` — pressing the key opens the command line with `:command ` already
  typed, so you can enter the arguments; `Enter` runs the function with them.
  Requires `command` to be set.
- `<text>` — a literal string passed verbatim to the function.

### Key-only vs. command access

If `command` is `-`, the function is reachable **only** through the key.
If `command` is set, it can also be run directly as `:command [args]`.

Example — `A` and `:create_folder` both drive the `new_folder` op, which calls `InsertFolder(name)`:

    TREE   A   no   create_folder   prompt   new_folder   InsertFolder(name)

- Press `A` → command line opens with `:create_folder ` prefilled → type a
  name → `Enter` creates the folder.
- Type `:create_folder New Notes` → `Enter` creates the folder directly,
  skipping the prompt.

A key-only binding (no command-line access):

    TREE   j   yes   -   -   move_down   MoveSelection(+1)

A command-only row (no key):

    GLOBAL   -   -   qa   -   quit   Quit()

---

## Operations (the dispatch registry)

Every binding dispatches to a registered **operation** (`op` column), which
calls the real **function** (`function` column):

    void op(const std::string& args, int count);         // registered handler
    void function(const std::string& args, int count);   // actual implementation

`args` is the argument string (from the command line, or `""` for `-`),
`count` is the repeat count (1 when not repeatable).

| Op                    | Called from               | Actual function (where implemented) | Behaviour                                 |
|-----------------------|---------------------------|-------------------------------------|-------------------------------------------|
| `move_up`             | keys (NORMAL/VISUAL/TREE) | `MoveSelection(-1)` (TREE)          | Move selection/line up                    |
| `move_down`           | keys                      | `MoveSelection(+1)` (TREE)          | Move selection/line down                  |
| `move_left`           | keys (NORMAL/VISUAL)      | -                                   | Move cursor left                          |
| `move_right`          | keys (NORMAL/VISUAL)      | -                                   | Move cursor right                         |
| `move_page_up/down`   | keys (NORMAL)             | -                                   | Page scroll                               |
| `move_line_start/end` | keys (NORMAL)             | -                                   | Move to start/end of line                 |
| `move_word_forward/back` | keys (NORMAL)          | -                                   | Word movement                             |
| `move_file_start`     | keys                      | `MoveToStart()` (TREE)              | `"g g"` top of tree / file                |
| `move_file_end`       | keys                      | `MoveToEnd()` (TREE)                | `G` bottom of tree / file                 |
| `tree_open`           | keys (TREE)               | `OpenSelected()`                    | Toggle folder expanded / open node        |
| `tree_collapse`       | keys (TREE)               | `CollapseSelected()`                | `h` — collapse folder, else go to parent  |
| `tree_expand`         | keys (TREE)               | `ExpandSelected()`                  | `l` — expand folder, else into first child|
| `expand_all`          | keys (TREE)               | `ExpandAll()`                       | Expand every folder (`E`)                 |
| `collapse_all`        | keys (TREE)               | `CollapseAll()`                     | Collapse every folder (`C`)               |
| `new_folder`          | key `A`, `:create_folder` | `InsertFolder(name)`                | Create a folder (name from args/prompt)   |
| `new_note`            | key `a`, `:create_note`   | `InsertNote(name)`                  | Create a note (name from args/prompt)     |
| `rename_node`         | key `R`, `:rename`        | `RenameSelected(name)`              | Rename the selected node                  |
| `enter_tree`          | keys                      | `EnterTree()`                       | Return to TREE mode                       |
| `enter_normal`        | keys                      | `EnterNormal()`                     | Enter NORMAL mode                         |
| `enter_insert`        | keys                      | `EnterInsert()`                     | Enter INSERT mode                         |
| `enter_visual`        | keys (NORMAL)             | `EnterVisual()`                     | Enter VISUAL mode                         |
| `enter_visual_line`   | keys (NORMAL)             | `EnterVisualLine()`                 | Enter VISUAL_LINE mode                    |
| `enter_command`       | keys (`:`)                | `OpenCommandLine()`                 | Open the command line                     |
| `insert_newline`      | keys (INSERT)             | -                                   | Insert a newline                          |
| `insert_tab`          | keys (INSERT)             | -                                   | Insert a tab                              |
| `backspace_char`      | keys (INSERT/NORMAL)      | -                                   | Delete char before cursor                 |
| `delete_char`         | keys (NORMAL)             | -                                   | Delete char at cursor                     |
| `delete_line`         | keys (NORMAL/VISUAL)      | -                                   | Delete current/selected line              |
| `undo`                | keys (NORMAL)             | -                                   | Undo                                      |
| `yank`                | keys (NORMAL/VISUAL)      | -                                   | Yank (copy)                               |
| `paste`               | keys (NORMAL)             | -                                   | Paste                                     |
| `quit`                | `:qa`                     | `Quit()`                            | Quit scribbolyth                          |

---

## Adding a new command

The three-step workflow:

1. **Write the function** and register it as an operation (in the component
   that owns the data — tree operations in `treeview`, text operations in
   `editor`, app-level ones in `main`).
2. **Add one line** to `config/commands.conf`.
3. Rebuild (`./build.sh` — the config is copied next to the app).

### Example: a `:sort_nodes` command

Register in the treeview:

    state->operations["sort_nodes"] = [this](const std::string& args, int count)
    {
        std::sort(selected_->children.begin(), selected_->children.end(),
                  [](const auto& a, const auto& b) { return a.name < b.name; });
    };

Add to `config/commands.conf`:

    GLOBAL   -   -   sort_nodes   -   sort_nodes   SortNodes()

Type `:sort_nodes` and `Enter` to run it.

### Example: give the same function a key too

    TREE   s   no   sort_nodes   -   sort_nodes   SortNodes()

Now `s` sorts the selected folder's children as well. Drop the `command`
column to make it key-only:

    TREE   s   no   -   -   sort_nodes   SortNodes()

### Example: a key that gathers arguments interactively

If a function needs arguments typed by the user, bind it with `args = prompt`
and give it a command:

    TREE   A   no   create_folder   prompt   new_folder   InsertFolder(name)

The prompt path is generic — the op only ever *executes*; the loader opens
`:create_folder ` for you.

---

## Notes

- `repeat: yes` keys accept count prefixes (`3 j`). Only the TREE mode
  enables counting today.
- Unknown `op`/`function` names, a missing config file, or malformed lines
  are reported at startup.
