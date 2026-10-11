#!/usr/bin/env python3
"""Harvest Scribbolyth's doc comments and inject them into devnotes.html.

Comment grammar (all harvested from files under src/):

    //!_key=value          config marker. Recognized keys:
                             coderoot=/path   where source-code nodes live
                                              (default: /src)
    //[/path]              folder-only node at the (absolute) note path.
                           A leading './' makes the path relative to the
                           current file's folder under the source-code root
                           (e.g. in treeview/treeview.cpp, ./Introduction ->
                           <coderoot>/treeview/treeview.cpp/Introduction).
    /*[/path] <body> */    node at the (absolute) note path with content;
                           the last path segment is its title.  './'-relative
                           paths work here too.
    /*! <body> */          source-code doc block. Opens a *region* that stays
                           open until a /*!*/ marker (the marker is dropped
                           from the body). Its title is the first content
                           line.  Doc and sub blocks found inside the open
                           region nest under it.
    /*> <body> */          like /*! in every way (same region, same title
                           rule, nesting and attachment rules), except the
                           node is *pinned*: it keeps its source-order slot
                           instead of being alphabetized.  /*>*/ closes a
                           region, like /*!*/ does.
    //> <text>             single-line pinned doc block: the rest of the line
                           is the body, and the region it opens still needs a
                           close marker.
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
    //| <text>             Single-line append: the rest of the line is
                           appended to the current documentation node's
                           content, exactly like a one-line /*| ... */.
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

The delimiters and markers shown above are the C++ defaults: the comment
syntax, the grammar markers and the method/constructor regexes all come from
per-language JSON profiles in langs/ (see langs/cpp.json).  A profile is
selected for each source file by its extension, so a C++ .hpp and a Python
.py can be harvested side by side.  Add a langs/<lang>.json to teach the
harvester another language.

Every file that contains a /*! */ doc block produces a *folder* node named
after the file (e.g. editor/editor.cpp -> editor > editor.cpp), hanging off
the coderoot node.

The output HTML is a full rebuild: config/scribboleth.html is copied fresh,
its <title> rewritten, and the harvested tree injected into `let treeData`.
"""

import argparse
import hashlib
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HERE = os.path.dirname(os.path.abspath(__file__))
LANG_DIR = os.path.join(HERE, "langs")
MAIN_CONFIG = os.path.join(HERE, "config", "main.json")

# Default project layout, overridable via config/main.json.  Relative paths
# in that file are resolved against HERE (the docsgen/ directory).
DEFAULT_SOURCE = "../src"
DEFAULT_OUTPUT = "../developers"
DEFAULT_TEMPLATE = "../config/scribboleth.html"
OUTPUT_FILENAME = "devnotes.html"

TEMPLATE = os.path.normpath(os.path.join(HERE, DEFAULT_TEMPLATE))
OUTPUT = os.path.join(HERE, DEFAULT_OUTPUT, OUTPUT_FILENAME)
SRC_ROOT = os.path.normpath(os.path.join(HERE, DEFAULT_SOURCE))
TITLE = "Scribbolyth Developer Notes"

# Create/emit the source-code subtree nodes in alphabetical order (by title,
# case-insensitive) instead of source order.  Custom nodes (//[...] and
# /*[...] */) are never moved - they keep the order they were encountered
# in.  Toggle with --no-sort on the command line.
ALPHABETICAL = True


# ----------------------------------------------------------------------
# Language configuration
# ----------------------------------------------------------------------

class Language:
    """Comment/grammar profile for one language, loaded from langs/*.json.

    Everything that differs between languages lives here: which comment
    delimiters to scan, which marker characters open the doc/sub/folder/etc
    regions, and how to recognise and describe a following method or
    constructor.  harvest.py owns only the generic engine; adding a language
    means adding a langs/<name>.json file (and, when the C-like signature
    parser does not fit, a parser branch keyed by "callable.parser").
    """

    def __init__(self, data, source):
        self.source = source
        self.name = data.get("name", os.path.basename(source))
        self.extensions = tuple(e.lower() for e in data.get("extensions", []))
        self.line_comment = data.get("line_comment")
        block = data.get("block_comment") or {}
        self.block_open = block.get("open")
        self.block_close = block.get("close")
        if not (self.block_open and self.block_close):
            self.block_open = self.block_close = None
        self.string_delimiters = set(data.get("string_delimiters", []))
        self.config_prefix = data.get("config_prefix", "!_")
        self.line_markers = data.get("line_markers", {})
        self.block_markers = data.get("block_markers", {})

        call = data.get("callable", {})
        self.callable_parser = call.get("parser", "c_like")
        self.method_token = call.get("method_token", "!_method")
        self.ctor_token = call.get("ctor_token", "!_ctor")
        self.method_mark_re = re.compile(
            call.get("method_regex",
                     r"^[ \t]*%s[ \t]*$" % re.escape(self.method_token)), re.M)
        self.ctor_mark_re = re.compile(
            call.get("ctor_regex",
                     r"^[ \t]*%s[ \t]*$" % re.escape(self.ctor_token)), re.M)
        self.param_open = call.get("param_open", "(")
        self.param_close = call.get("param_close", ")")
        self.terminators = set(call.get("terminators", ["{", ";"]))
        self.definition_terminators = set(call.get("definition_terminators", ["{"]))
        self.operator_re = re.compile(call.get("operator_regex", r"\boperator\b"))
        self.name_re = re.compile(call.get("name_regex",
                                           r"[A-Za-z_~][A-Za-z0-9_]*\s*$"))
        self.param_name_re = re.compile(
            call.get("param_name_regex", r"[A-Za-z_][A-Za-z0-9_]*\s*$"))
        self.split_open = set(call.get("split_open", "<(["))
        self.split_close = set(call.get("split_close", ">)]"))

        anchor = data.get("anchor", {})
        self.anchor_re = (re.compile(anchor["regex"])
                          if anchor.get("enabled") and anchor.get("regex") else None)

        self.config_re = re.compile(
            "^" + re.escape(self.config_prefix) + r"\s*([A-Za-z0-9_]+)\s*=\s*(.*)$")


def load_languages(lang_dir):
    """Read every langs/*.json into a Language (sorted by file name)."""
    langs = []
    if not os.path.isdir(lang_dir):
        return langs
    for fn in sorted(os.listdir(lang_dir)):
        if not fn.lower().endswith(".json"):
            continue
        path = os.path.join(lang_dir, fn)
        with open(path, "r", encoding="utf-8") as f:
            langs.append(Language(json.load(f), path))
    return langs


def extension_map(langs):
    """Map each configured extension (lower-case, with dot) to its Language."""
    ext_map = {}
    for lang in langs:
        for ext in lang.extensions:
            if ext in ext_map and ext_map[ext] is not lang:
                print("! Warning: extension %s claimed by both %s and %s; "
                      "keeping %s" % (ext, ext_map[ext].name, lang.name,
                                      ext_map[ext].name), file=sys.stderr)
                continue
            ext_map[ext] = lang
    return ext_map


def load_settings(path):
    """Read config/main.json (project source/output/template paths)."""
    if not os.path.isfile(path):
        return {}
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def resolve_path(base, path):
    """Make `path` absolute; relative paths are resolved against `base`."""
    return path if os.path.isabs(path) else os.path.normpath(os.path.join(base, path))


# ----------------------------------------------------------------------
# Comment scanning
# ----------------------------------------------------------------------

def matching_paren(sig, open_, open_ch="(", close_ch=")"):
    depth = 0
    for i in range(open_, len(sig)):
        c = sig[i]
        if c == open_ch:
            depth += 1
        elif c == close_ch:
            depth -= 1
            if depth == 0:
                return i
    return -1


def param_parens(sig, lang):
    """The parameter list is the one whose closing ')' is the signature's
    last top-level ')'.  Backtrack from it to its matching '('.  Returns
    (open_pos, close_pos) or (-1, -1)."""
    close_ = sig.rfind(lang.param_close)
    if close_ == -1:
        return -1, -1
    depth = 0
    for i in range(close_, -1, -1):
        c = sig[i]
        if c == lang.param_close:
            depth += 1
        elif c == lang.param_open:
            depth -= 1
            if depth == 0:
                return i, close_
    return -1, -1


def extract_signature_python(src, pos, lang):
    """Scan src[pos:] for the first 'def'/'class' line, skipping blank and
    comment lines, and return (signature, True) up to the trailing ':'.
    Used when callable.parser == 'python'."""
    n = len(src)
    i = pos
    while i < n:
        line_end = src.find("\n", i)
        if line_end == -1:
            line_end = n
        stripped = src[i:line_end].strip()
        if not stripped or stripped.startswith("#"):
            i = line_end + 1
            continue
        if not re.match(r"(async\s+def|def|class)\b", stripped):
            return None
        buf = []
        depth = 0
        j = i
        while j < n:
            c = src[j]
            if c in "([{":
                depth += 1
            elif c in ")]}":
                depth = max(0, depth - 1)
            elif c == ":" and depth == 0:
                break
            if c != "\n":
                buf.append(c)
            j += 1
        return " ".join("".join(buf).split()), True
    return None


def extract_signature(src, pos, lang):
    """Scan src[pos:] for a function/method declaration right after a doc
    comment.  Whitespace and comments are skipped; scanning stops at the
    first definition/statement terminator (see lang.terminators) at
    parenthesis-depth 0.  Returns
        (signature_text, is_definition)
    with signature_text whitespace-collapsed, or None when no signature
    is found before the end of the file."""
    if lang.callable_parser == "python":
        return extract_signature_python(src, pos, lang)
    n = len(src)
    i = pos
    depth = 0
    buf = []
    open_ch, close_ch = lang.param_open, lang.param_close
    while i < n:
        if lang.line_comment and src.startswith(lang.line_comment, i):
            j = src.find("\n", i)
            i = n if j == -1 else j
            buf.append(" ")
            continue
        if lang.block_open and src.startswith(lang.block_open, i):
            j = src.find(lang.block_close, i + len(lang.block_open))
            i = n if j == -1 else j + len(lang.block_close)
            buf.append(" ")
            continue
        c = src[i]
        if c in lang.string_delimiters:
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
        if c == open_ch:
            depth += 1
        elif c == close_ch:
            depth = max(0, depth - 1)
        elif c in lang.terminators and depth == 0:
            return (" ".join("".join(buf).split()),
                    c in lang.definition_terminators)
        buf.append(c)
        i += 1
    return None


def split_top_level(text, lang):
    """Split text on commas that are not nested inside the language's
    bracket pairs ((), [], <> for C++)."""
    parts, depth, cur = [], 0, []
    for ch in text:
        if ch in lang.split_open:
            depth += 1
        elif ch in lang.split_close:
            depth = max(0, depth - 1)
        if ch == "," and depth == 0:
            parts.append("".join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    if cur:
        parts.append("".join(cur).strip())
    return parts


def describe_signature_python(sig, lang):
    """Break a collapsed Python def/class signature into
    (name, returns, params, tail)."""
    m = re.match(r"(?:async\s+)?(def|class)\s+([A-Za-z_][A-Za-z0-9_]*)", sig)
    if m is None:
        return None
    keyword, name = m.group(1), m.group(2)
    rest = sig[m.end():].strip()
    if keyword == "class":
        return name, "class", [], rest
    params = []
    tail = rest
    open_ = rest.find("(")
    if open_ != -1:
        close_ = matching_paren(rest, open_, "(", ")")
        if close_ != -1:
            param_text = rest[open_ + 1:close_].strip()
            tail = rest[close_ + 1:].strip()
            for p in split_top_level(param_text, lang):
                p = p.strip()
                if not p:
                    continue
                if "=" in p:
                    p = p.split("=", 1)[0].strip()
                if ":" in p:
                    nm, ty = p.split(":", 1)
                    params.append((ty.strip(), nm.strip()))
                else:
                    params.append(("", p))
    returns = ""
    if "->" in tail:
        returns = tail.split("->", 1)[1].strip()
    return name, returns, params, tail


def describe_signature(sig, lang, ctor=False):
    """Break a collapsed signature into (name, returns, params, tail) or
    return None when it does not look like a function/method."""
    if lang.callable_parser == "python":
        return describe_signature_python(sig, lang)
    if ctor:
        open_ = sig.find(lang.param_open)
        close_ = (matching_paren(sig, open_, lang.param_open, lang.param_close)
                  if open_ != -1 else -1)
    else:
        open_, close_ = param_parens(sig, lang)
    if open_ == -1 or close_ == -1:
        return None
    decl = sig[:open_].strip()
    param_text = sig[open_ + 1:close_].strip()
    quals = sig[close_ + 1:].strip()
    returns = decl
    name = None
    op = lang.operator_re.search(decl)
    if op:
        name = decl[op.start():].strip()
        returns = decl[:op.start()].strip()
    else:
        m = lang.name_re.search(decl)
        if m:
            name = m.group(0).strip()
            returns = decl[:m.start()].strip()
            if returns.endswith("::"):
                returns = returns[:-2].strip()
    params = []
    if param_text:
        for p in split_top_level(param_text, lang):
            p = p.strip()
            if not p:
                continue
            if "=" in p:
                p = p.split("=", 1)[0].strip()
            pm = lang.param_name_re.search(p)
            if pm:
                nm = pm.group(0).strip()
                ty = p[:pm.start()].strip()
                params.append((ty, nm) if ty else (nm, ""))
    return name, returns, params, quals


def member_inits(tail, lang):
    """Split a constructor tail `: a(x), b({...})` into init-list members.
    Returns (members, qualifiers).  A qualifier prefix (e.g. `noexcept : ...`)
    is peeled off before the ':'."""
    t = tail.strip()
    colon = t.find(":")
    if colon == -1:
        return [], t
    quals = t[:colon].strip()
    members = [p.strip() for p in split_top_level(t[colon + 1:], lang)
               if p.strip()]
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


def callable_description(src, code_pos, body, lang):
    """Replace a method/constructor marker line (see lang.method_mark_re /
    ctor_mark_re) in a comment body with a description of the
    function/constructor definition immediately following the comment.
    Leaves the marker untouched when no definition follows."""
    kinds = [("ctor", lang.ctor_mark_re) if rx is lang.ctor_mark_re
             else ("method", rx)
             for rx in (lang.ctor_mark_re, lang.method_mark_re) if rx.search(body)]
    if not kinds:
        return body
    found = extract_signature(src, code_pos, lang)
    if not found or not found[1]:
        return body
    kind, rx = kinds[0]
    described = describe_signature(found[0], lang, ctor=kind == "ctor")
    if described is None:
        return body
    name, returns, params, tail = described
    lines = ["Signature: " + found[0]]
    if kind == "ctor":
        members, quals = member_inits(tail, lang)
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


def scan_file(path, lang):
    """Return source comments/segments as (kind, body, line) in source order.

    kind is "line" for a single line comment, "block" for a delimited block
    comment, or "code" for raw source captured between the configured raw
    markers (e.g. `//>>` ... `//<<`).  Body excludes the comment delimiters.
    String literals are skipped so comment text inside them is not mistaken
    for a comment.  Delimiters and markers come from `lang`.
    """
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        src = f.read()

    items = []
    i, n = 0, len(src)
    line = 1
    lc = lang.line_comment
    bo, bc = lang.block_open, lang.block_close
    lm = lang.line_markers
    raw_open = lm.get("raw_open")
    raw_close = (lc + lm["raw_close"]) if (lc and lm.get("raw_close")) else None
    while i < n:
        c = src[i]
        if c in lang.string_delimiters:
            quote = c
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
        if lc and src.startswith(lc, i):
            end = src.find("\n", i)
            if end == -1:
                end = n
            body = src[i + len(lc):end]
            if raw_open and body.strip() == raw_open:
                # Capture raw source until a raw-close line (both markers
                # sit on their own lines); the captured text lands in the
                # current documentation node as a code snippet.
                j = end + 1
                seg = []
                while j < n:
                    lf = src.find("\n", j)
                    if lf == -1:
                        lf = n
                    if raw_close and src[j:lf].strip().startswith(raw_close):
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
            items.append(("line", body, line))
            i = end
            continue
        if bo and src.startswith(bo, i):
            start_line = line
            end = src.find(bc, i + len(bo))
            if end == -1:
                items.append(("block", src[i + len(bo):], start_line))
                line += src.count("\n", i, n)
                i = n
            else:
                body = callable_description(
                    src, end + len(bc), src[i + len(bo):end], lang)
                items.append(("block", body, start_line))
                line += src.count("\n", i, end + len(bc))
                i = end + len(bc)
            continue
        if c == "\n":
            line += 1
        i += 1
    return items


def _emit_marked(actions, action, marker, text, line,
                 close_marker=None, empty_closes=False):
    """If `text` starts with `marker`, record the matching action and return
    True.  `close_marker + marker` (e.g. `/*!`) is stripped as an explicit
    "stop harvesting here" token.  A doc/docseq marker with an empty body
    becomes a docclose when `empty_closes` (block comments), otherwise the
    empty marker line is simply ignored (line comments)."""
    if not marker or not text.startswith(marker):
        return False
    rest = text[len(marker):]
    if close_marker:
        rest = strip_marker(rest, close_marker + marker)
    if not rest.strip():
        if empty_closes and action in ("doc", "docseq"):
            actions.append({"kind": "docclose", "line": line})
        return True
    actions.append({"kind": action, "body": rest, "line": line})
    return True


def classify(items, lang):
    """Turn raw comments into structured actions.

    Actions are dicts with a "kind" and the source "line":
        config {key, value}   from the configured config marker
        folder {path}         from a line folder marker
        manual {path, body}   from a block manual marker
        doc    {body}         doc block (opens a region)
        docseq {body}         pinned doc block (same as doc, but pinned
                                        in source order)
        docclose              closes the innermost open region
        sub    {body}         sub-note of the innermost open region
        code   {body}         raw source snippet
        append {body}         appended to the current documentation node
    Doc/sub/code/append nesting is resolved by the builder using a region
    stack.
    """
    actions = []
    lm = lang.line_markers
    bm = lang.block_markers
    for kind, body, line in items:
        if kind == "code":
            actions.append({"kind": "code", "body": body, "line": line})
            continue
        if kind == "line":
            text = body.strip()
            m = lang.config_re.match(text)
            if m:
                actions.append({"kind": "config", "key": m.group(1),
                                "value": m.group(2).strip(), "line": line})
                continue
            close = lm.get("close")
            if close and text == close:
                actions.append({"kind": "docclose", "line": line})
                continue
            folder = lm.get("folder")
            if folder and text.startswith(folder):
                rest = text[len(folder):]
                end = rest.find("]")
                if end != -1:
                    actions.append({"kind": "folder",
                                    "path": rest[:end].strip(),
                                    "line": line})
                continue
            if _emit_marked(actions, "doc", lm.get("doc"), text, line):
                continue
            if _emit_marked(actions, "docseq", lm.get("pinned"), text, line):
                continue
            if _emit_marked(actions, "sub", lm.get("sub"), text, line):
                continue
            if _emit_marked(actions, "append", lm.get("append"), text, line):
                continue
            continue

        # block comment
        if _emit_marked(actions, "doc", bm.get("doc"), body, line,
                        lang.block_open, empty_closes=True):
            continue
        if _emit_marked(actions, "docseq", bm.get("pinned"), body, line,
                        lang.block_open, empty_closes=True):
            continue
        if _emit_marked(actions, "sub", bm.get("sub"), body, line,
                        lang.block_open):
            continue
        if _emit_marked(actions, "append", bm.get("append"), body, line):
            continue
        manual = bm.get("manual")
        if manual:
            text = body.lstrip()
            if text.startswith(manual):
                end = text.find("]")
                if end != -1:
                    actions.append({"kind": "manual",
                                    "path": text[len(manual):end].strip(),
                                    "body": text[end + 1:],
                                    "line": line})
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


def find_anchor(src_files):
    """Return (rel_path, line) of the entry-point file, or (None, None).

    src_files is a list of (path, lang) pairs; only languages whose config
    enables an anchor (lang.anchor_re) participate."""
    for path, lang in src_files:
        if lang.anchor_re is None:
            continue
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            src = f.read()
        m = lang.anchor_re.search(src)
        if m:
            rel = os.path.relpath(path, SRC_ROOT).replace(os.sep, "/")
            return rel, src.count("\n", 0, m.start()) + 1
    return None, None


def build_tree(file_actions, coderoot, anchor_rel=None, anchor_line=None):
    tree = Tree()
    coderoot_parts = split_path(coderoot)
    made_source_node = False

    def apply_custom(a, file_parts):
        p = a["path"].strip()
        if p.startswith("./"):
            # A './'-prefixed path is relative to the current file's node
            # under the source-code root (e.g. ./Introduction lands on
            # <coderoot>/treeview/treeview.cpp/Introduction).
            parts = coderoot_parts + file_parts + split_path(p[2:])
        else:
            parts = split_path(p)
        node = tree.ensure(parts)
        # Mark the custom node itself so the alphabetical source sort
        # leaves it at its encountered position.  Ancestor folders are
        # regular source/file folders and must stay sortable.
        node["pinned"] = True
        if a["kind"] == "manual":
            node["content"] = doc_text(a["body"])
        elif a["kind"] == "folder":
            pass

    # Phase 1: custom nodes defined before 'int main' (in the anchor file)
    # lead the tree.  Custom nodes from other files are treated as leading
    # too, so the coderoot section stays anchored to main.cpp's flow.
    for rel, actions in file_actions:
        file_parts = tuple(rel.split("/"))
        led_anchor = rel == anchor_rel and anchor_line is not None
        for a in actions:
            if a["kind"] not in ("manual", "folder"):
                continue
            if led_anchor and a.get("line", 1) >= anchor_line:
                continue
            apply_custom(a, file_parts)

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
            if a["kind"] not in ("doc", "docseq", "docclose", "sub"):
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
            if a["kind"] == "docseq":
                node["pinned"] = True
            parent["children"].append(node)
            last = node
            if a["kind"] in ("doc", "docseq"):
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
                apply_custom(a, tuple(rel.split("/")))

    if ALPHABETICAL:
        source_root = tree.by_path.get(coderoot_parts)
        if source_root is not None:
            sort_source_tree(source_root)

    # A node's display name is its first content line (mirroring the app's
    # title sync).  When a node's saved title differs from that line - e.g.
    # a custom node whose body doesn't restate its path name - prepend the
    # name so the document reads correctly.
    def _sync_title(ns):
        for n in ns:
            content = n.get("content", "")
            if content and first_line(content) != n["title"]:
                n["content"] = n["title"] + "\n\n" + content
            _sync_title(n.get("children", []))
    _sync_title(tree.children)

    return tree.children


# ----------------------------------------------------------------------
# Main
# ----------------------------------------------------------------------

def count_nodes(nodes):
    total = 0
    for n in nodes:
        total += 1 + count_nodes(n.get("children", []))
    return total


def _sort_levels(children):
    """Sort one sibling list: source nodes alphabetically (by title,
    case-insensitive) while pinned nodes (custom nodes and /*> doc blocks)
    stay at their encountered slots, then recurse into each child."""
    slots = [i for i, n in enumerate(children) if n.get("pinned")]
    sources = [n for n in children if not n.get("pinned")]
    sources.sort(key=lambda n: n["title"].lower())
    it = iter(sources)
    out = []
    for i in range(len(children)):
        out.append(children[i] if i in slots else next(it))
    children[:] = out
    for n in children:
        _sort_levels(n.get("children", []))


def sort_source_tree(node):
    """Alphabetically sort the descendants of the source-code root node
    only; pinned nodes everywhere keep their encountered order."""
    _sort_levels(node.get("children", []))


def main():
    global ALPHABETICAL, TEMPLATE, OUTPUT, SRC_ROOT

    parser = argparse.ArgumentParser(
        description="Harvest doc comments from src/ into devnotes.html")
    parser.add_argument("--sort", dest="alphabetical", action="store_true",
                        default=None, help="order nodes alphabetically (default)")
    parser.add_argument("--no-sort", dest="alphabetical", action="store_false",
                        help="keep nodes in source order")
    args = parser.parse_args()
    if args.alphabetical is not None:
        ALPHABETICAL = args.alphabetical

    settings = load_settings(MAIN_CONFIG)
    SRC_ROOT = resolve_path(HERE, settings.get("source", DEFAULT_SOURCE))
    OUTPUT = os.path.join(
        resolve_path(HERE, settings.get("output", DEFAULT_OUTPUT)),
        settings.get("output_file", OUTPUT_FILENAME))
    TEMPLATE = resolve_path(HERE, settings.get("template", DEFAULT_TEMPLATE))

    if not os.path.isfile(TEMPLATE):
        print("! Error: template not found: %s" % TEMPLATE, file=sys.stderr)
        return 1

    languages = load_languages(LANG_DIR)
    if not languages:
        print("! Error: no language configs found in %s" % LANG_DIR,
              file=sys.stderr)
        return 1
    ext_map = extension_map(languages)

    file_actions = []
    coderoot = None

    # walk src/ recursively, keeping the files a language config claims
    src_files = []
    for dirpath, dirnames, filenames in os.walk(SRC_ROOT):
        dirnames[:] = sorted(d for d in dirnames if not d.startswith("."))
        for fn in sorted(filenames):
            lang = ext_map.get(os.path.splitext(fn)[1].lower())
            if lang is not None:
                src_files.append((os.path.join(dirpath, fn), lang))
    src_files.sort(key=lambda p: p[0])

    for path, lang in src_files:
        rel = os.path.relpath(path, SRC_ROOT).replace(os.sep, "/")
        actions = classify(scan_file(path, lang), lang)
        file_actions.append((rel, actions))
        for a in actions:
            if a["kind"] == "config" and a["key"] == "coderoot":
                coderoot = a["value"]

    if coderoot is None:
        coderoot = "/src"

    anchor_rel, anchor_line = find_anchor(src_files)
    nodes = build_tree(file_actions, coderoot, anchor_rel, anchor_line)
    # drop the internal pin marker used only by the source sort
    def _strip(ns):
        for n in ns:
            n.pop("pinned", None)
            _strip(n.get("children", []))
    _strip(nodes)
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
    print("  source:     %s" % SRC_ROOT)
    print("  coderoot:   %s" % coderoot)
    print("  files:      %d" % len(src_files))
    print("  doc nodes:  %d" % count_nodes(nodes))
    return 0


if __name__ == "__main__":
    sys.exit(main())
