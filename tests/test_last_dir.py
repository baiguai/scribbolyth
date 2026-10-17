#!/usr/bin/env python3
"""Feature test for path-prompt default directories.

Each path prompt is prefilled with a directory chosen per command:

    open / open!  -> the folder the last document was opened from
    import (U)    -> the folder the last HTML file was imported from
    saveas / X / x / t / T -> the folder the current document lives in

`last_open_dir` and `last_import_dir` persist in init.conf (`open_dir` /
`import_dir`); the save/export default is derived from the open document, so
after a save-as to a new folder the exports follow the document while `:o `
still returns to where you last opened from.

Typing the command name then Tab pre-fills the same directory, and '\\'
strips the auto-inserted directory back out.

Permanent regression for the path defaults added to editor_state.hpp,
config/, op/ and treeview/.
"""
import os
import shutil

import harness

DATA_DIR = '/tmp/scribbolyth_tests/last_dir_data'
shutil.rmtree(DATA_DIR, ignore_errors=True)

open_src = os.path.join(DATA_DIR, 'open_src')
save_dst = os.path.join(DATA_DIR, 'save_dst')
import_src = os.path.join(DATA_DIR, 'import_src')
for d in (open_src, save_dst, import_src):
    os.makedirs(d)

feed_html = os.path.join(import_src, 'feed.html')
init_conf = os.path.join(DATA_DIR, 'init.conf')


def setting(name):
    """Value of `name = ...` from init.conf, or None when absent."""
    try:
        with open(init_conf, encoding='utf-8') as f:
            for line in f:
                if line.strip().startswith(name + ' ='):
                    return line.split('=', 1)[1].strip()
    except FileNotFoundError:
        pass
    return None


def cmdline(s):
    """The rendered ':' command buffer (second-to-last screen row)."""
    return s.row_text(s.rows - 2).rstrip()


s = harness.launch(cols=110, workdir=DATA_DIR)
try:
    # A document to work with.
    s.send(b'a'); s.require('create_node')
    s.send(b'Alpha'); s.send(b'\r'); s.step(0.4)
    s.send(b'I'); s.send(b'hello world'); s.send(b'\x1b'); s.send(b'\x1b')
    s.step(0.3)

    # Save-as into open_src/: from now on the save/export prompts follow the
    # document. No document has been *opened* yet, so :o has no default.
    s.send(b'S'); s.require('saveas')
    s.send(os.path.join(open_src, 'doc.json').encode()); s.send(b'\r')
    s.require('Saved', 'save should succeed')

    s.send(b'S'); s.step(0.5)
    if not cmdline(s).endswith(':saveas ' + open_src + '/'):
        print('FAIL: saveas should default to the document dir, got %r' % cmdline(s))
        s.dump()
        raise SystemExit(1)
    s.send(b'\x1b'); s.step(0.3)

    s.send(b'o'); s.step(0.5)
    if open_src in cmdline(s):
        print('FAIL: o should have no default until a file is opened, got %r' % cmdline(s))
        s.dump()
        raise SystemExit(1)
    s.send(b'\x1b'); s.step(0.3)

    # Open the saved file back from open_src: this records the open directory.
    s.send(b':o ' + os.path.join(open_src, 'doc.json').encode()); s.send(b'\r')
    s.require('Alpha', 'reopen should succeed')
    s.step(0.4)

    # Export an HTML fixture for the import test (exports remember nothing).
    s.send(b':X ' + feed_html.encode()); s.send(b'\r')
    s.require('Exported', 'html export should succeed')

    # Save-as to a *different* folder: the document moves, the open dir does not.
    s.send(b'S'); s.step(0.5)
    s.send(b'\\'); s.send(os.path.join(save_dst, 'doc.json').encode()); s.send(b'\r')
    s.require('Saved', 'second save should succeed')

    # :o still returns to where we last opened from ...
    s.send(b'o'); s.step(0.5)
    if not cmdline(s).endswith(':o ' + open_src + '/'):
        print('FAIL: o should default to the last open dir, got %r' % cmdline(s))
        s.dump()
        raise SystemExit(1)
    s.send(b'\x1b'); s.step(0.3)

    # ... while save-as and exports follow the moved document.
    for key, cmd, where in ((b'S', 'saveas', save_dst),
                            (b't', 'export_note_txt', save_dst)):
        s.send(key); s.step(0.5)
        if not cmdline(s).endswith(':' + cmd + ' ' + where + '/'):
            print('FAIL: %s should default to the document dir (%s), got %r'
                  % (cmd, where, cmdline(s)))
            s.dump()
            raise SystemExit(1)
        s.send(b'\x1b'); s.step(0.3)

    # Import from import_src: records the import directory.
    s.send(b':U ' + feed_html.encode()); s.send(b'\r')
    s.require('Alpha', 'import should succeed')
    if setting('import_dir') != import_src + '/':
        print('FAIL: import_dir should be %r, got %r'
              % (import_src + '/', setting('import_dir')))
        s.dump()
        raise SystemExit(1)

    if setting('open_dir') != open_src + '/':
        print('FAIL: open_dir should be %r, got %r'
              % (open_src + '/', setting('open_dir')))
        s.dump()
        raise SystemExit(1)

    # Typing a command name then Tab pre-fills the same default.
    for typed, want in ((b'o', ':o ' + open_src + '/'),
                        (b'U', ':U ' + import_src + '/')):
        s.send(b':'); s.step(0.3)
        s.send(typed); s.send(b'\t'); s.step(0.4)
        if not cmdline(s).endswith(want):
            print('FAIL: :%s + Tab should yield %r, got %r'
                  % (typed.decode(), want, cmdline(s)))
            s.dump()
            raise SystemExit(1)
        # '\' strips the inserted directory again.
        s.send(b'\\'); s.step(0.3)
        if want.split(' ', 1)[1] in cmdline(s):
            print('FAIL: \\ should clear the Tab-inserted dir for :%s' % typed.decode())
            s.dump()
            raise SystemExit(1)
        s.send(b'\x1b'); s.step(0.3)
finally:
    s.quit()

# Both remembered directories survive a relaunch.
s = harness.launch(cols=110, workdir=DATA_DIR)
try:
    s.send(b'o'); s.step(0.5)
    if not cmdline(s).endswith(':o ' + open_src + '/'):
        print('FAIL: open dir should persist across relaunch, got %r' % cmdline(s))
        s.dump()
        raise SystemExit(1)
    s.send(b'\x1b'); s.step(0.3)

    s.send(b':'); s.step(0.3)
    s.send(b'U'); s.send(b'\t'); s.step(0.4)
    if not cmdline(s).endswith(':U ' + import_src + '/'):
        print('FAIL: import dir should persist across relaunch, got %r' % cmdline(s))
        s.dump()
        raise SystemExit(1)
    s.send(b'\x1b'); s.step(0.3)
finally:
    s.quit()

print('PASS')
