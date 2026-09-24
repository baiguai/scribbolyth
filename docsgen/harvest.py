#!/usr/bin/env python3
"""Harvest Scribbolyth's doc comments and inject them into devnotes.html.

Comment grammar (all harvested from files under src/):

    //!_key=value          config marker. Recognized keys:
                             coderoot=/path   where source-code nodes live
                                              (default: /src)
    //[/path]              folder-only node at the (absolute) note path.
    /*[/path] <body> */    node at the (absolute) note path with content;
                           the last path segment is its title.
    /*! <body> */          source-code doc block. Opens a *region* that stays
                           open until a /*!*/ marker (the marker is dropped
                           from the body). Its title is the first content
                           line.  Doc and sub blocks found inside the open
                           region nest under it.
    /*+ <body> */          sub-note. A child of the innermost open doc
                           region in the same file. Ignored when no doc
                           region is open.
    /*!*/  (or a //! line)   Closes the innermost open doc region.  Creates
                           no node of its own.
    //>> ... //<<          Raw source snippet.  The markers sit on their own
                           lines; the code between them is appended verbatim
                           to the current documentation node (the innermost
                           open doc region; if none is open, the most recent
                           doc/sub node in the same file).
    /*| <body> */          Appends its body text to the current
                           documentation node's content (same attachment
                           rules as the //>> ... //<< snippet).
    !_method               Marker inside a doc comment.  Replaced with the
                           signature, method name, return type and
                           parameters of the function definition that
                           immediately follows the comment (left as-is when
                           no definition follows).
    !_ctor                 Marker inside a doc comment, like !_method but for
                           a constructor definition; also lists the initializer
                           members (and qualifiers such as explicit/noexcept).

Within any comment body, a line containing just dashes (e.g. ----) is
expanded to an 80-character horizontal rule.  Raw code captured by
//>> ... //<< is never rewritten.

Every file that contains a /*! */ doc block produces a *folder* node named
after the file (e.g. editor/editor.cpp -> editor > editor.cpp), hanging off
the coderoot node.

The output HTML is a full rebuild: config/scribboleth.html is copied fresh,
its <title> rewritten, and the harvested tree injected into `let treeData`.
"""

import hashlib
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEMPLATE = os.path.join(ROOT, "config", "scribboleth.html")
OUTPUT = os.path.join(ROOT, "developers", "devnotes.html")
SRC_ROOT = os.path.join(ROOT, "src")
TITLE = "Scribbolyth Developer Notes"

EXTENSIONS = (".cpp", ".hpp", ".h", ".cc", ".cxx", ".c")


# ----------------------------------------------------------------------
# Comment scanning
# ----------------------------------------------------------------------

METHOD_MARK_RE = re.compile(r"^[ \t]*!_method[ \t]*$", re.M)
CTOR_MARK_RE = re.compile(r"^[ \t]*!_ctor[ \t]*$", re.M)


def extract_signature(src, pos):
    """Scan src[pos:] for a function/method declaration right after a doc
    comment.  Whitespace and comments are skipped; scanning stops at the
    first '{' or ';' at parenthesis-depth 0.  Returns
        (signature_text, is_definition)
    with signature_text whitespace-collapsed, or None when no signature
    is found before the end of the file."""
    n = len(src)
    i = pos
    depth = 0
    buf = []
    while i < n:
        if src.startswith("//", i):
            j = src.find("\n", i)
            i = n if j == -1 else j
            buf.append(" ")
            continue
        if src.startswith("/*", i):
            j = src.find("*/", i + 2)
            i = n if j == -1 else j + 2
            buf.append(" ")
            continue
        c = src[i]
        if c in "\"'":
            q = c
            i += 1
            while i < n:
                if src[i] == "\\":
                    i += 2
                    continue
                if src[i] == q:
                    i += 1
                    break
                i += 1
            buf.append(" ")
            continue
        if c == "(":
            depth += 1
        elif c == ")":
            depth = max(0, depth - 1)
        elif c in "{;":
            if depth == 0:
                return " ".join("".join(buf).split()), c == "{"
        buf.append(c)
        i += 1
    return None


def split_top_level(text):
    """Split text on commas that are not nested inside (), [], or <>."""
    parts, depth, cur = [], 0, []
    for ch in text:
        if ch in "<([":
            depth += 1
        elif ch in ">)]":
            depth = max(0, depth - 1)
        if ch == "," and depth == 0:
            parts.append("".join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    if cur:
        parts.append("".join(cur).strip())
    return parts


def matching_paren(sig, open_):
    depth = 0
    for i in range(open_, len(sig)):
        c = sig[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
    return -1


def describe_signature(sig, ctor=False):
    """Break a collapsed signature into (name, returns, params, tail) or
    return None when it does not look like a function/method."""
    if ctor:
        open_ = sig.find("(")
        close_ = matching_paren(sig, open_) if open_ != -1 else -1
    else:
        open_ = sig.rfind("(")
        close_ = sig.find(")", open_) if open_ != -1 else -1
    if open_ == -1 or close_ == -1:
        return None
    decl = sig[:open_].strip()
    param_text = sig[open_ + 1:close_].strip()
    quals = sig[close_ + 1:].strip()
    returns = decl
    name = None
    op = re.search(r"\boperator\b", decl)
    if op:
        name = decl[op.start():].strip()
        returns = decl[:op.start()].strip()
    else:
        m = re.search(r"[A-Za-z_~][A-Za-z0-9_]*\s*$", decl)
        if m:
            name = m.group(0).strip()
            returns = decl[:m.start()].strip()
            if returns.endswith("::"):
                returns = returns[:-2].strip()
    params = []
    if param_text:
        for p in split_top_level(param_text):
            p = p.strip()
            if not p:
                continue
            if "=" in p:
                p = p.split("=", 1)[0].strip()
            pm = re.search(r"[A-Za-z_][A-Za-z0-9_]*\s*$", p)
            if pm:
                nm = pm.group(0).strip()
                ty = p[:pm.start()].strip()
                params.append((ty, nm) if ty else (nm, ""))
    return name, returns, params, quals


def member_inits(tail):
    """Split a constructor tail `: a(x), b({...})` into init-list members.
    Returns (members, qualifiers).  A qualifier prefix (e.g. `noexcept : ...`)
    is peeled off before the ':'."""
    t = tail.strip()
    colon = t.find(":")
    if colon == -1:
        return [], t
    quals = t[:colon].strip()
    members = [p.strip() for p in split_top_level(t[colon + 1:]) if p.strip()]
    return members, quals


def render_members(members):
    """Render ctor initializers as an aligned `name  args` column."""
    rows = []
    for m in members:
        o = m.find("(")
        if o != -1 and m.endswith(")"):
            rows.append((m[:o].strip(), m[o + 1:-1].strip()))
        else:
            rows.append((m, ""))
    w = max(len(a) for a, b in rows)
    out = []
    for a, b in rows:
        out.append("    " + a.ljust(w + 1) + b if b else "    " + a)
    return out


def _content_base(body):
    """Leading-whitespace count of the first content line, measured the same
    way deindent will measure it (i.e. after the doc/sub marker char)."""
    txt = body
    lead = len(txt) - len(txt.lstrip(" \t"))
    if txt[lead:lead + 1] in ("!", "+"):
        txt = txt[lead + 1:]
    for ln in txt.split("\n"):
        t = ln.lstrip(" \t")
        if t:
            return len(ln) - len(t)
    return 0


def callable_description(src, code_pos, body):
    """Replace a `!_method` / `!_ctor` marker line in a comment body with a
    description of the function/constructor definition immediately following
    the comment.  Leaves the marker untouched when no definition follows."""
    kinds = [("ctor", CTOR_MARK_RE) if rx is CTOR_MARK_RE else ("method", rx)
             for rx in (CTOR_MARK_RE, METHOD_MARK_RE) if rx.search(body)]
    if not kinds:
        return body
    found = extract_signature(src, code_pos)
    if not found or not found[1]:
        return body
    kind, rx = kinds[0]
    described = describe_signature(found[0], ctor=kind == "ctor")
    if described is None:
        return body
    name, returns, params, tail = described
    lines = ["Signature: " + found[0]]
    if kind == "ctor":
        members, quals = member_inits(tail)
        specs = [returns] if returns in ("explicit", "inline") else []
        if specs:
            quals = " ".join(specs + ([quals] if quals else []))
        lines.append("Constructor: " + (name or "?"))
        if quals:
            lines.append("Qualifiers: " + quals)
    else:
        members, quals = [], tail
        lines.append("Method: " + (name or "?"))
        if returns:
            lines.append("Returns: " + returns)
        if quals:
            lines.append("Qualifiers: " + quals)
    if params:
        width = max(len(nm) for ty, nm in params if nm)
        lines.append("Parameters:")
        for ty, nm in params:
            pad = nm.ljust(width + 1) if nm else " " * (width + 1)
            lines.append("    " + pad + ty)
    if kind == "ctor" and members:
        lines.append("Initializers:")
        lines.extend(render_members(members))
    base = _content_base(body)
    ind = " " * base
    # The body is de-indented later by base, so pad every inserted line by
    # base: the inner indents then survive in the final content.
    return kinds[0][1].sub(
        lambda m: "\n".join(ind + ln if ln else "" for ln in lines), body)


def scan_file(path):
    """Return source comments/segments as (kind, body, line) in source order.

    kind is "line" for // comments, "block" for /* ... */ comments, or
    "code" for the raw source captured between a `//>>` and `//<<` line
    (each on its own line).  Body excludes the // or /* */ delimiters.
    Strings and char literals are skipped so // inside them is not
    mistaken for a comment.
    """
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        src = f.read()

    items = []
    i, n = 0, len(src)
    line = 1
    while i < n:
        c = src[i]
        if c in "\"'":
            quote = c
            line_start = line
            i += 1
            while i < n:
                if src[i] == "\\":
                    line += src.count("\n", i, i + 2)
                    i += 2
                    continue
                if src[i] == quote:
                    i += 1
                    break
                if src[i] == "\n":
                    line += 1
                i += 1
            continue
        if src.startswith("//", i):
            end = src.find("\n", i)
            if end == -1:
                end = n
            if src[i + 2:end].strip() == ">>":
                # Capture raw source until a `//<<` line (both markers sit
                # on their own lines); the captured text lands in the
                # current documentation node as a code snippet.
                j = end + 1
                seg = []
                while j < n:
                    lf = src.find("\n", j)
                    if lf == -1:
                        lf = n
                    if src[j:lf].strip().startswith("//<<"):
                        items.append(("code", "\n".join(seg), line))
                        line += 1
                        i = n if lf == n else lf + 1
                        break
                    seg.append(src[j:lf].rstrip())
                    line += 1
                    j = n if lf == n else lf + 1
                else:
                    items.append(("code", "\n".join(seg), line))
                    i = n
                continue
            items.append(("line", src[i + 2:end], line))
            i = end
            continue
        if src.startswith("/*", i):
            start_line = line
            end = src.find("*/", i + 2)
            if end == -1:
                items.append(("block", src[i + 2:], start_line))
                line += src.count("\n", i, n)
                i = n
            else:
                body = callable_description(src, end + 2, src[i + 2:end])
                items.append(("block", body, start_line))
                line += src.count("\n", i, end + 2)
                i = end + 2
            continue
        if c == "\n":
            line += 1
        i += 1
    return items


def classify(items):
    """Turn raw comments into structured actions.

    Actions are dicts with a "kind" and the source "line":
        config {key, value}            from //!_key=value
        folder {path}                  from //[path]
        manual {path, body}            from /*[path] body*/
        doc    {body}                  from /*! body*/  (opens a region)
        docclose                       from /*!*/        (closes the region)
        sub    {body}                  from /*+ body*/
        code   {body}                  from //>> ... //<< (raw source)
        append {body}                  from /*| body*/
    Doc/sub/code/append nesting is resolved by the builder using a region
    stack.
    """
    actions = []
    doc_open = None
    for kind, body, line in items:
        if kind == "code":
            actions.append({"kind": "code", "body": body, "line": line})
            continue
        if kind == "line":
            text = body.strip()
            if text == "!":
                actions.append({"kind": "docclose", "line": line})
                continue
            m = re.match(r"!_\s*([A-Za-z0-9_]+)\s*=\s*(.*)$", text)
            if m:
                actions.append({"kind": "config", "key": m.group(1),
                                "value": m.group(2).strip(), "line": line})
                continue
            if text.startswith("["):
                end = text.find("]")
                if end != -1:
                    actions.append({"kind": "folder",
                                    "path": text[1:end].strip(),
                                    "line": line})
                continue
            continue

        # block comment
        if body.startswith("!"):
            text = strip_marker(body[1:], "/*!")
            if text.strip():
                actions.append({"kind": "doc", "body": text, "line": line})
            else:
                actions.append({"kind": "docclose", "line": line})
            continue
        if body.startswith("+"):
            text = strip_marker(body[1:], "/*+")
            if text.strip():
                actions.append({"kind": "sub", "body": text, "line": line})
            continue
        if body.startswith("|"):
            text = body[1:]
            if text.strip():
                actions.append({"kind": "append", "body": text, "line": line})
            continue
        manual = body.lstrip()
        if manual.startswith("["):
            end = manual.find("]")
            if end != -1:
                actions.append({"kind": "manual",
                                "path": manual[1:end].strip(),
                                "body": manual[end + 1:],
                                "line": line})
                continue
    return actions


# ----------------------------------------------------------------------
# Content shaping
# ----------------------------------------------------------------------

def strip_marker(text, marker):
    """Drop an explicit end marker (/*!*/ or /*+*/ borders).

    In C the whole block is terminated by the final */, so the body of
    `/*! ... /*!*/` is `! ... /*!` — the trailing /*! is just the explicit
    "stop harvesting here" token and must not become content.
    """
    trimmed = text.rstrip()
    if trimmed.endswith(marker):
        return trimmed[:-len(marker)]
    return text

def deindent(raw):
    """Strip the first content line's indentation from every line.

    Leading/trailing blank lines are dropped and trailing spaces trimmed,
    so extra indentation used for readability in future lines is preserved.
    """
    lines = raw.split("\n")
    i = 0
    while i < len(lines) and lines[i].strip() == "":
        i += 1
    lines = lines[i:]
    while lines and lines[-1].strip() == "":
        lines.pop()
    if not lines:
        return ""
    base = lines[0]
    n = len(base) - len(base.lstrip(" \t"))
    out = []
    for ln in lines:
        k = 0
        while k < n and k < len(ln) and ln[k] in " \t":
            k += 1
        out.append(ln[k:])
    return "\n".join(out)


DASH_RULE_RE = re.compile(r"^[ \t]*-{3,}[ \t]*$")


def expand_rules(text):
    """Rewrite a line of just dashes (e.g. ----) as a full-width (80-char)
    horizontal rule."""
    out = []
    for ln in text.split("\n"):
        if DASH_RULE_RE.match(ln):
            out.append("-" * 80)
        else:
            out.append(ln)
    return "\n".join(out)


def doc_text(raw):
    """Shaped content for a doc-comment body (deindented + rule expansion)."""
    return expand_rules(deindent(raw))


def first_line(text):
    """First non-empty, trimmed line (mirrors the app's title sync)."""
    for line in text.split("\n"):
        t = line.strip()
        if t:
            return t[:80]
    return ""


# ----------------------------------------------------------------------
# Tree
# ----------------------------------------------------------------------

class Tree:
    def __init__(self):
        self.children = []
        self.by_path = {}
        self._ids = {}

    def _node_id(self, key):
        h = hashlib.sha1(("scribbolyth:" + key).encode("utf-8")).hexdigest()
        while h[:8] in self._ids and self._ids[h[:8]] != key:
            h = hashlib.sha1((h + "x").encode("utf-8")).hexdigest()
        self._ids[h[:8]] = key
        return h[:8]

    def ensure(self, parts):
        """Return the node for an absolute '/' path, creating folders as
        needed.  parts is a tuple of node titles."""
        node = None
        for i in range(len(parts)):
            key = tuple(parts[:i + 1])
            if key not in self.by_path:
                n = {"id": self._node_id("/" + "/".join(key)),
                     "title": parts[i],
                     "content": "",
                     "children": [],
                     "expanded": False}
                self.by_path[key] = n
                if i == 0:
                    self.children.append(n)
                else:
                    self.by_path[key[:-1]]["children"].append(n)
            node = self.by_path[key]
        return node


def split_path(path):
    """'/Source Code' -> ('Source Code',);  '' -> ()."""
    return tuple(s.strip() for s in path.split("/") if s.strip())


ANCHOR_RE = re.compile(r"\bint\s+main\s*\(")


def find_anchor(src_files):
    """Return (rel_path, line) of the file containing 'int main', or
    (None, None) if no source file has an entry point."""
    for path in src_files:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            src = f.read()
        m = ANCHOR_RE.search(src)
        if m:
            rel = os.path.relpath(path, SRC_ROOT).replace(os.sep, "/")
            return rel, src.count("\n", 0, m.start()) + 1
    return None, None


def build_tree(file_actions, coderoot, anchor_rel=None, anchor_line=None):
    tree = Tree()
    coderoot_parts = split_path(coderoot)
    made_source_node = False

    def apply_custom(a):
        if a["kind"] == "manual":
            node = tree.ensure(split_path(a["path"]))
            node["content"] = doc_text(a["body"])
        elif a["kind"] == "folder":
            tree.ensure(split_path(a["path"]))

    # Phase 1: custom nodes defined before 'int main' (in the anchor file)
    # lead the tree.  Custom nodes from other files are treated as leading
    # too, so the coderoot section stays anchored to main.cpp's flow.
    for rel, actions in file_actions:
        led_anchor = rel == anchor_rel and anchor_line is not None
        for a in actions:
            if a["kind"] not in ("manual", "folder"):
                continue
            if led_anchor and a.get("line", 1) >= anchor_line:
                continue
            apply_custom(a)

    # Phase 2: source-code nodes (coderoot + per-file folders + doc blocks).
    # Doc regions nest: a /*! opens a region that stays open until the next
    # /*!*/ , so /*! and /*+ blocks inside it become children.  Raw code
    # captured with //>> ... //<< is appended to the current doc node.
    for rel, actions in file_actions:
        file_parts = tuple(rel.split("/"))
        stack = []
        last = None
        ev = 0
        for a in actions:
            if a["kind"] in ("code", "append"):
                body = (deindent(a["body"]) if a["kind"] == "code"
                        else expand_rules(deindent(a["body"])))
                if not body:
                    continue
                current = stack[-1] if stack else last
                if current is not None:
                    current["content"] = (current["content"] + "\n\n" if current["content"] else "") + body
                continue
            if a["kind"] not in ("doc", "docclose", "sub"):
                continue
            ev += 1
            if a["kind"] == "docclose":
                if stack:
                    stack.pop()
                continue
            if not made_source_node:
                tree.ensure(coderoot_parts)
                made_source_node = True
            if stack:
                parent = stack[-1]
            else:
                parent = tree.ensure(coderoot_parts + file_parts)
            content = doc_text(a["body"])
            node = {"id": tree._node_id(rel + "#n" + str(ev)),
                    "title": first_line(content) or "(untitled)",
                    "content": content,
                    "children": [],
                    "expanded": False}
            parent["children"].append(node)
            last = node
            if a["kind"] == "doc":
                stack.append(node)

    # Phase 3: custom nodes defined after 'int main' trail the tree
    if anchor_rel is not None and anchor_line is not None:
        for rel, actions in file_actions:
            if rel != anchor_rel:
                continue
            for a in actions:
                if a["kind"] not in ("manual", "folder"):
                    continue
                if a.get("line", 1) < anchor_line:
                    continue
                apply_custom(a)
    return tree.children


# ----------------------------------------------------------------------
# Main
# ----------------------------------------------------------------------

def count_nodes(nodes):
    total = 0
    for n in nodes:
        total += 1 + count_nodes(n.get("children", []))
    return total


def main():
    if not os.path.isfile(TEMPLATE):
        print("! Error: template not found: %s" % TEMPLATE, file=sys.stderr)
        return 1

    file_actions = []
    coderoot = None

    # walk src/ recursively
    src_files = []
    for dirpath, dirnames, filenames in os.walk(SRC_ROOT):
        dirnames[:] = sorted(d for d in dirnames if not d.startswith("."))
        for fn in sorted(filenames):
            if fn.lower().endswith(EXTENSIONS):
                src_files.append(os.path.join(dirpath, fn))
    src_files.sort()

    for path in src_files:
        rel = os.path.relpath(path, SRC_ROOT).replace(os.sep, "/")
        actions = classify(scan_file(path))
        file_actions.append((rel, actions))
        for a in actions:
            if a["kind"] == "config" and a["key"] == "coderoot":
                coderoot = a["value"]

    if coderoot is None:
        coderoot = "/src"

    anchor_rel, anchor_line = find_anchor(src_files)
    nodes = build_tree(file_actions, coderoot, anchor_rel, anchor_line)
    payload = json.dumps(nodes, indent=2)

    with open(TEMPLATE, "r", encoding="utf-8") as f:
        html = f.read()

    html = re.sub(r"<title>.*?</title>", lambda m: "<title>%s</title>" % TITLE,
                  html, count=1)
    html = re.sub(r"let\s+treeData\s*=\s*\[[\s\S]*?\];",
                  lambda m: "let treeData = " + payload + ";",
                  html, count=1)

    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
    with open(OUTPUT, "w", encoding="utf-8") as f:
        f.write(html)

    print("Generated: %s" % OUTPUT)
    print("  title:      %s" % TITLE)
    print("  coderoot:   %s" % coderoot)
    print("  files:      %d" % len(src_files))
    print("  doc nodes:  %d" % count_nodes(nodes))
    return 0


if __name__ == "__main__":
    sys.exit(main())
