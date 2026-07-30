#!/bin/bash

set -e

echo "=== scribbolyth setup ==="

# Check prerequisites
command -v cmake >/dev/null 2>&1 || { echo "ERROR: cmake is required (3.16+). Install it and try again."; exit 1; }
CMAKE_VER=$(cmake --version | head -1 | grep -oP '\d+\.\d+')
if (( $(echo "$CMAKE_VER < 3.16" | bc -l) )); then
    echo "ERROR: cmake 3.16+ required (found $CMAKE_VER)"
    exit 1
fi
echo "[OK] cmake $CMAKE_VER"

if command -v g++ &>/dev/null; then
    echo "[OK] g++ $(g++ -dumpversion)"
elif command -v clang++ &>/dev/null; then
    echo "[OK] clang++"
else
    echo "ERROR: no C++ compiler found (g++ or clang++)"
    exit 1
fi

# Create module directories
mkdir -p src/editor src/treeview

# Create placeholder files
[ -f src/editor/editor.hpp ] || cat > src/editor/editor.hpp << 'EOF'
#pragma once

#include <ftxui/component/component.hpp>

namespace scribbolyth::editor {

ftxui::Component MakeEditor();

} // namespace scribbolyth::editor
EOF
echo "[OK] src/editor/editor.hpp"

[ -f src/editor/editor.cpp ] || cat > src/editor/editor.cpp << 'EOF'
#include "editor.hpp"

namespace scribbolyth::editor {

class Editor : public ftxui::ComponentBase {
public:
    Editor() {}
    ftxui::Element Render() override {
        return ftxui::text("Editor") | ftxui::center | ftxui::border;
    }
};

ftxui::Component MakeEditor() {
    return ftxui::Make<Editor>();
}

} // namespace scribbolyth::editor
EOF
echo "[OK] src/editor/editor.cpp"

[ -f src/treeview/treeview.hpp ] || cat > src/treeview/treeview.hpp << 'EOF'
#pragma once

#include <ftxui/component/component.hpp>

namespace scribbolyth::treeview {

ftxui::Component MakeTreeView();

} // namespace scribbolyth::treeview
EOF
echo "[OK] src/treeview/treeview.hpp"

[ -f src/treeview/treeview.cpp ] || cat > src/treeview/treeview.cpp << 'EOF'
#include "treeview.hpp"

namespace scribbolyth::treeview {

class TreeView : public ftxui::ComponentBase {
public:
    TreeView() {}
    ftxui::Element Render() override {
        return ftxui::text("TreeView") | ftxui::center | ftxui::border;
    }
};

ftxui::Component MakeTreeView() {
    return ftxui::Make<TreeView>();
}

} // namespace scribbolyth::treeview
EOF
echo "[OK] src/treeview/treeview.cpp"

echo ""
echo "=== Setup complete ==="
echo "Run ./build.sh to build."
