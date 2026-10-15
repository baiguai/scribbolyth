#!/usr/bin/env python3
"""Feature test for last-used-directory memory.

Saving / opening / importing / exporting records the parent directory (with a
trailing separator) of the file that was used. The next path prompt is
prefilled with it, so the user extends it instead of retyping a full path:

    :saveas /home/bob/temp/
    :open   /home/bob/temp/

The open/save family and the import/export family keep independent directories,
both persisted to init.conf, and pressing '\\' in the prompt clears just the
auto-inserted directory prefix (leaving anything typed after it).

Permanent regression for the last-dir memory added to editor_state.hpp,
config/, op/ and treeview/.
"""
import os
import shutil

import harness

DATA_DIR = '/tmp/scribbolyth_tests/last_dir_data'
shutil.rmtree(DATA_DIR, ignore_errors=True)
os.makedirs(DATA_DIR)

open_dir = os.path.join(DATA_DIR, 'open_docs')
exp_dir = os.path.join(DATA_DIR, 'exp_docs')
os.makedirs(open_dir)
os.makedirs(exp_dir)

init_conf = os.path.join(DATA_DIR, 'init.conf')


def setting(name):
    """Value of `name = ...` from init.conf, or None when absent."""
    with open(init_conf, encoding='utf-8') as f:
        for line in f:
            if line.strip().startswith(name + ' ='):
                return line.split('=', 1)[1].strip()
    return None


def cmdline(s):
    """The rendered ':' command buffer (second-to-last screen row)."""
    return s.row_text(s.rows - 2).rstrip()


s = harness.launch(cols=110, workdir=DATA_DIR)
try:
    # --- open/save family ------------------------------------------------
    # Save a document inside open_docs/; the family remembers that folder.
    s.send(b'a'); s.send(b'Doc'); s.send(b'\r'); s.step(0.4)
    s.send(b'S'); s.require('saveas')
    s.send(os.path.join(open_dir, 'doc.json').encode()); s.send(b'\r')
    s.require('Saved', 'save should succeed')

    if setting('open_dir') != open_dir + '/':
        print('FAIL: open_dir should be %r, got %r'
              % (open_dir + '/', setting('open_dir')))
        s.dump()
        raise SystemExit(1)

    # Pressing S again prefills :saveas with the remembered directory.
    s.send(b'S'); s.step(0.5)
    if not cmdline(s).endswith(':saveas ' + open_dir + '/'):
        print('FAIL: S should prefill the last-used open dir, got %r' % cmdline(s))
        s.dump()
        raise SystemExit(1)

    # '\' strips the auto-inserted directory; the command name survives.
    s.send(b'\\'); s.step(0.3)
    if cmdline(s).endswith(open_dir + '/'):
        print('FAIL: \\ should clear the prefilled dir, got %r' % cmdline(s))
        s.dump()
        raise SystemExit(1)
    s.require('saveas', 'the command name should remain after clearing')
    s.send(b'\x1b'); s.step(0.3)

    # The open prompt shares the same remembered directory.
    s.send(b'o'); s.step(0.5)
    if not cmdline(s).endswith(':o ' + open_dir + '/'):
        print('FAIL: o should prefill the last-used open dir, got %r' % cmdline(s))
        s.dump()
        raise SystemExit(1)
    s.send(b'\x1b'); s.step(0.3)

    # --- import/export family (independent) ------------------------------
    # A note to export.
    s.send(b'a'); s.send(b'Exp'); s.send(b'\r'); s.step(0.4)
    s.send(b'I'); s.send(b'export body'); s.send(b'\x1b'); s.send(b'\x1b')
    s.step(0.3)

    # First export has no remembered export dir yet, so no prefill.
    s.send(b't'); s.step(0.5)
    s.send(os.path.join(exp_dir, 'note.txt').encode()); s.send(b'\r')
    s.require('Exported note to', 'note export should succeed')

    if setting('export_dir') != exp_dir + '/':
        print('FAIL: export_dir should be %r, got %r'
              % (exp_dir + '/', setting('export_dir')))
        s.dump()
        raise SystemExit(1)

    # The export prompt now prefills its own directory, independent of open_dir.
    s.send(b't'); s.step(0.5)
    if not cmdline(s).endswith(':export_note_txt ' + exp_dir + '/'):
        print('FAIL: t should prefill the last-used export dir, got %r' % cmdline(s))
        s.dump()
        raise SystemExit(1)
    s.send(b'\\'); s.step(0.3)
    if cmdline(s).endswith(exp_dir + '/'):
        print('FAIL: \\ should clear the prefilled export dir, got %r' % cmdline(s))
        s.dump()
        raise SystemExit(1)
    s.send(b'\x1b'); s.step(0.3)
finally:
    s.quit()

# --- both directories survive a relaunch --------------------------------
s = harness.launch(cols=110, workdir=DATA_DIR)
try:
    s.send(b'o'); s.step(0.5)
    if not cmdline(s).endswith(':o ' + open_dir + '/'):
        print('FAIL: open dir should persist across relaunch, got %r' % cmdline(s))
        s.dump()
        raise SystemExit(1)
    s.send(b'\x1b'); s.step(0.3)

    s.send(b't'); s.step(0.5)
    if not cmdline(s).endswith(':export_note_txt ' + exp_dir + '/'):
        print('FAIL: export dir should persist across relaunch, got %r' % cmdline(s))
        s.dump()
        raise SystemExit(1)
    s.send(b'\x1b'); s.step(0.3)
finally:
    s.quit()

# --- typing a command name then Tab also pre-fills the directory --------
# (the '\' escape works here too)
cases = [
    (b'o', ':o ', open_dir + '/'),
    (b'open', ':open ', open_dir + '/'),
    (b'saveas', ':saveas ', open_dir + '/'),
    (b'U', ':U ', exp_dir + '/'),
    (b'X', ':X ', exp_dir + '/'),
]
s = harness.launch(cols=110, workdir=DATA_DIR)
try:
    for typed, prefix, directory in cases:
        s.send(b':'); s.step(0.3)
        s.send(typed); s.step(0.2)
        s.send(b'\t'); s.step(0.4)
        want = prefix + directory
        if not cmdline(s).endswith(want):
            print('FAIL: :%s + Tab should yield %r, got %r'
                  % (typed.decode(), want, cmdline(s)))
            s.dump()
            raise SystemExit(1)
        # '\' strips the inserted directory again.
        s.send(b'\\'); s.step(0.3)
        if cmdline(s).endswith(directory):
            print('FAIL: \\ should clear the Tab-inserted dir for :%s'
                  % typed.decode())
            s.dump()
            raise SystemExit(1)
        s.send(b'\x1b'); s.step(0.3)
finally:
    s.quit()

print('PASS')
