#!/usr/bin/env python3
"""Feature test for root defaults on every file path prompt.

Opening, save-as, importing and exporting no longer remember a last-used
directory. Every path prompt starts at '/', and Tab completion inserts the
same root prefix. Directory bookmarks in the browser are the intended
shortcut instead.

Permanent regression for op/ path prompts and config/ (no open_dir /
import_dir persistence).
"""
import os
import shutil

import harness

DATA_DIR = '/tmp/scribbolyth_tests/root_defaults_data'
shutil.rmtree(DATA_DIR, ignore_errors=True)
os.makedirs(DATA_DIR)
doc_path = os.path.join(DATA_DIR, 'doc.json')
init_conf = os.path.join(DATA_DIR, 'init.conf')


def cmdline(s):
    """The rendered ':' command buffer (second-to-last screen row)."""
    return s.row_text(s.rows - 2).rstrip()


def require_default(s, want, label):
    if not cmdline(s).endswith(want):
        print('FAIL: %s should default to %r, got %r' % (label, want, cmdline(s)))
        s.dump()
        raise SystemExit(1)


s = harness.launch(cols=110, workdir=DATA_DIR)
try:
    # Save a document outside the root so any old last-directory logic would
    # produce a non-root default.
    s.send(b'a'); s.require('create_node')
    s.send(b'Alpha'); s.send(b'\r'); s.step(0.4)
    s.send(b':saveas ' + doc_path.encode()); s.send(b'\r')
    s.require('Saved', 'save should succeed')

    # Prompt key bindings all prefill '/'.
    for key, want, label in ((b'o', ':o /', 'o (open)'),
                             (b'S', ':saveas /', 'S (save as)'),
                             (b't', ':export_note_txt /', 't (export note)'),
                             (b'T', ':export_tree_txt /', 'T (export tree)')):
        s.send(key); s.step(0.5)
        require_default(s, want, label)
        s.send(b'\x1b'); s.step(0.3)

    # Command-name completion inserts the same root prefix for every path op.
    for name, want in (('open', ':open /'),
                       ('o', ':o /'),
                       ('o!', ':o! /'),
                       ('saveas', ':saveas /'),
                       ('U', ':U /'),
                       ('X', ':X /'),
                       ('x', ':x /'),
                       ('export_note_txt', ':export_note_txt /'),
                       ('export_tree_txt', ':export_tree_txt /')):
        s.send(b':'); s.step(0.3)
        s.send(name.encode()); s.send(b'\t'); s.step(0.4)
        require_default(s, want, ':%s + Tab' % name)

        # '\' strips the automatically inserted root again.
        s.send(b'\\'); s.step(0.3)
        if cmdline(s).endswith(want):
            print('FAIL: \\ should clear the root prefix for :%s' % name)
            s.dump()
            raise SystemExit(1)
        s.send(b'\x1b'); s.step(0.3)

    # Pressing o then Enter opens the browser at the filesystem root.
    s.send(b'o'); s.step(0.5)
    require_default(s, ':o /', 'o before opening the browser')
    s.send(b'\r')
    s.require(' File Browser ', 'a root path should open the browser')
    s.require('tmp/', 'the browser should list the root directory')
    s.forbid('doc.json', 'the browser should not reopen the document folder',
             rows=range(s.rows - 1))
    s.send(b'\x1b')
    s.forbid(' File Browser ')
finally:
    s.quit()

# The old per-operation directory keys are no longer written to init.conf.
with open(init_conf, encoding='utf-8') as f:
    conf = f.read()
for key in ('open_dir', 'import_dir'):
    if key in conf:
        print('FAIL: init.conf should no longer contain %s' % key)
        raise SystemExit(1)

print('PASS')
