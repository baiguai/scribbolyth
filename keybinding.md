# Adding a New Command — Step-by-Step Guide

Every command and key binding in scribbolyth has two parts:

1. A **C++ operation** registered in `state->operations` — where the actual
   work happens.
2. A **config line** in `config/commands.conf` — which binds a key and/or a
   `:command` name to that operation.

`build.sh` copies `commands.conf` next to the app executable (`build/bin/`
on Linux, `build-windows/bin/` on Windows).

---

## Step 1 — Register the operation in the `.cpp` file

Operations live in `state->operations`, a map of name → handler:

    using Operation = std::function<void(const std::string& args, int count)>;

Register yours in the constructor of the component that owns the data:

| Operation belongs to | Register it in                                 |
|----------------------|------------------------------------------------|
| tree (nodes)         | `src/treeview/treeview.cpp` — `TreeView::TreeView` |
| text (editor)        | `src/editor/editor.cpp` — `Editor::Editor`     |
| app-level (e.g. quit)| `src/main.cpp`                                 |

For example, adding a `sort_nodes` operation to the tree:

    state->operations["sort_nodes"] = [this](const std::string&, int)
    {
        std::sort(selected_->children.begin(), selected_->children.end(),
                  [](const auto& a, const auto& b) { return a.name < b.name; });
    };

Rules:

- The op name must match the `op` column of the config line **exactly**
  (case-sensitive). `LoadConfig` validates every row against this map and
  fails the **whole** config if an op is missing — so an unregistered op
  disables all bindings, not just its own row.
- Components register in their constructors, which run before `main` calls
  `LoadConfig`, so ordering is safe.
- If the op should only act in one mode, guard it:

      if (state->mode == Mode::TREE) { ... }

- `args` is `""` for `args = -`, the typed argument for `prompt`, or the
  literal string. `count` is the repeat prefix (`3 j`); always `1` when
  `repeat` is `no`.

## Step 2 — Add a line to `config/commands.conf`

One entry per line:

    mode  key  repeat  command  args  op  function

Blank lines and lines starting with `#` are ignored.

| Column    | Meaning                                                                                          |
|-----------|--------------------------------------------------------------------------------------------------|
| `mode`    | `TREE`, `NORMAL`, `INSERT`, `VISUAL` — binds `key` in that mode. `GLOBAL` — registers a command only (no key). |
| `key`     | The key that triggers the binding. `-` means no key.                                             |
| `repeat`  | `yes`/`no` — accepts a count prefix (`3 j`).                                                     |
| `command` | Optional `:name` usable from the command line. `-` means no command-line access.                 |
| `args`    | `-`, `prompt`, or a literal string. See below.                                                   |
| `op`      | The operation name from Step 1. `-` means no dispatch.                                           |
| `function`| Documentation only — the function the op calls. Ignored by the loader.                           |

`key` values:

- A single character: `j`, `:`, `0`, `$`, ...
- A special token: `Esc`, `Return`, `Tab`, `Backspace`, `Space`, `ArrowUp`,
  `ArrowDown`, `ArrowLeft`, `ArrowRight`, `PageUp`, `PageDown`, `Home`, `End`.
- A quoted multi-key sequence: `"g g"` (keys separated by spaces).

`args` values:

- `-` — no arguments; the op receives `""`.
- `prompt` — pressing the key opens the command line with `:command `
  prefilled so the user can type the argument; `Enter` runs the op with it.
  Requires `command` to be set.
- `<text>` — a literal string passed verbatim to the op.

Examples — the same op bound four ways:

    # key-only
    TREE   s   no   -   -   sort_nodes   SortNodes()

    # key + :command
    TREE   s   no   sort_nodes   -   sort_nodes   SortNodes()

    # command-only (no key) — run as :sort_nodes from any mode
    GLOBAL   -   -   sort_nodes   -   sort_nodes   SortNodes()

    # key that prompts for an argument before running
    TREE   a   no   create_note   prompt   new_note   InsertNote(name)

For the last one: press `a` → the command line opens with `:create_note `
prefilled → type a name → `Enter` creates the note. The same op is also
reachable directly as `:create_note Name`. (With nothing selected, `a`
adds a new top-level node; `A` uses `:create_child` / `InsertChild` to add
a child of the selected node, or a top-level node when nothing is selected.)

## Step 3 — Rebuild

    ./build.sh

The config is copied next to the binary automatically — no extra step.

---

## How dispatch works (when you press a key or run `:command`)

- **Key press** → the active keymap returns the binding's `op` → `Dispatch`
  opens the prompt (if `args = prompt`) or calls `operations[op]`.
- **`:command ...`** → `ExecuteCommand` looks up the `commands` map
  (populated from the `command` column of every row) → calls `operations[op]`.
