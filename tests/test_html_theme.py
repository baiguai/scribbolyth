#!/usr/bin/env python3
"""Regression test: an export over an EXISTING .html file must not touch that
file's theme material at all.

When the target already exists, `:X` patches the tree/bookmark/history data in
place. Any theme captured from an earlier `.html` import must not be written
into it: the target keeps its own `currentTheme`, `themes` and
`<style id="theme_styles">` verbatim. The captured theme still applies when
building a NEW export from the template. Loading a `.scribboleth` document
drops any imported theme, so a later new-file export cannot inherit an
unrelated document's theme.
"""
import os
import shutil

import harness

DATA_DIR = '/tmp/scribbolyth_tests/html_theme_data'
shutil.rmtree(DATA_DIR, ignore_errors=True)
os.makedirs(DATA_DIR)

SOURCE = os.path.join(DATA_DIR, 'source.html')
TARGET = os.path.join(DATA_DIR, 'target.html')
PLAIN = os.path.join(DATA_DIR, 'plain.scribboleth')
FRESH = os.path.join(DATA_DIR, 'fresh.html')

SOURCE_THEME = 'zzImportedThemeMarker'
TARGET_THEME = 'zzTargetThemeMarker'
SOURCE_STYLE = 'zzImportedStyleMarker'
TARGET_STYLE = 'zzTargetStyleMarker'


def html(theme_name, style_marker, node_id, title):
    return (
        '<!DOCTYPE html>\n'
        '<html><head>\n'
        '<style id="theme_styles">\n'
        '%s\n'
        '</style>\n'
        '</head><body>\n'
        '<script>\n'
        'let treeData = [\n'
        '  { "id": "%s", "title": "%s", "content": "note body", '
        '"expanded": true, "children": [] }\n'
        '];\n'
        'let bookmarks = [ ];\n'
        'let historyStack = [ ];\n'
        'let currentTheme = { name: "%s" };\n'
        'let themes = [ { name: "%s" } ];\n'
        '</script>\n'
        '</body></html>\n'
        % (style_marker, node_id, title, theme_name, theme_name)
    )


with open(SOURCE, 'w', encoding='utf-8') as f:
    f.write(html(SOURCE_THEME, SOURCE_STYLE, 'imp1', 'ImportedAlpha'))

with open(TARGET, 'w', encoding='utf-8') as f:
    f.write(html(TARGET_THEME, TARGET_STYLE, 'tgt1', 'TargetBeta'))

s = harness.launch(workdir=DATA_DIR)
try:
    s.require('Keycode help, use:', 'app must start blank')

    # Import the themed source document; its theme material is captured.
    s.send((':U ' + SOURCE).encode()); s.send(b'\r')
    s.require('1 descendant', 'import should report the node count')

    # Export the imported document over the EXISTING target file.
    s.send((':X ' + TARGET).encode()); s.send(b'\r')
    s.require('Exported', 'status bar should report the export')

    with open(TARGET, encoding='utf-8') as f:
        out = f.read()

    # The data is updated from the imported document...
    if 'ImportedAlpha' not in out:
        print('FAIL: overwriting export should carry the imported tree data')
        s.dump()
        raise SystemExit(1)
    # ...but the target's own theme is untouched in every form.
    for marker in (TARGET_THEME, TARGET_STYLE):
        if marker not in out:
            print('FAIL: overwriting export clobbered the target theme '
                  '(%s missing)' % marker)
            s.dump()
            raise SystemExit(1)
    for marker in (SOURCE_THEME, SOURCE_STYLE):
        if marker in out:
            print('FAIL: imported theme leaked into the existing target '
                  '(%s present)' % marker)
            s.dump()
            raise SystemExit(1)

    # Save the imported document, then load it back as a .scribboleth file:
    # that drops the imported theme, so a NEW export must not inherit it.
    s.send((':x ' + PLAIN).encode()); s.send(b'\r')
    s.require('Saved', 'save-as should report the save')
    s.send((':o ' + PLAIN).encode()); s.send(b'\r')
    s.require('1 descendant', 'loading the .scribboleth should report its nodes')

    s.send((':X ' + FRESH).encode()); s.send(b'\r')
    s.require('Exported', 'status bar should report the fresh export')
    with open(FRESH, encoding='utf-8') as f:
        fresh = f.read()
    for marker in (SOURCE_THEME, SOURCE_STYLE):
        if marker in fresh:
            print('FAIL: imported theme leaked into a new export after loading '
                  'a .scribboleth (%s present)' % marker)
            s.dump()
            raise SystemExit(1)
finally:
    s.quit()

print('PASS')
