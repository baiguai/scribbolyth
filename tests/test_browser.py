#!/usr/bin/env python3
"""Feature test for the file-browser dialog: a path command (:open/:saveas/
:import_html/:export_html) given a directory argument opens a modal browser.
h/j/k/l/gg/G navigate, Enter picks a file, l does not pick, Esc cancels, and
the picked file's full path is handed back to the invoking command.

Permanent regression for src/browser/.
"""
import os

import harness

s = harness.launch()
try:
    # Fixture: a docs dir (with a nested dir) and a loose file at the root.
    for rel in ('docs/info.txt', 'docs/notes.md', 'docs/nested/deep.txt',
                'root.txt'):
        path = os.path.join(s.workdir, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            f.write(rel + '\n')

    # A valid tree saved into docs/ so the browser can load it later.
    s.send(b'a')
    s.require('create_node')
    s.send(b'Saved'); s.send(b'\r')
    s.send(b'I')
    s.send(b'Saved body')
    s.send(b'\x1b'); s.send(b'\x1b')          # NORMAL -> TREE
    s.send(b':saveas docs/tree.json')
    s.send(b'\r')
    s.require('Saved 1 nodes to', 'saveas with a file path writes directly')

    # `:open docs` (a directory) opens the browser on that directory.
    s.send(b':open docs')
    s.send(b'\r')
    s.require(' File Browser ', 'a dir arg should open the file browser')
    s.require('1/5', 'selection should start on ../ (row 1 of 5)')
    for entry in ('../', 'info.txt', 'notes.md', 'nested/', 'tree.json'):
        s.require(entry, 'browser should list %r' % entry)

    # The dialog consumes every key: `a` must not open create_node.
    s.send(b'a')
    s.forbid('create_node', 'dialog must consume a (no create_node prompt)')

    # G jumps to the last row (tree.json); Enter picks it and loads it.
    s.send(b'G')
    s.require('5/5', 'G should jump to the last entry')
    s.send(b'\r')
    s.forbid(' File Browser ', 'Enter should close the dialog')
    s.require('Loaded 1 nodes from', 'Enter should run :open on the picked file')
    s.forbid('Saved body',
             'a fresh load starts with no active node (editor placeholder)')

    # gg -> first row (../), l enters it (goes up a level to the workdir).
    s.send(b':open docs')
    s.send(b'\r')
    s.require(' File Browser ')
    s.send(b'gg')
    s.require('1/5', 'gg should jump to the first row')
    s.send(b'l')
    s.require('docs/', 'l on ../ should go up to the workdir')
    s.require('root.txt', 'workdir listing should show root.txt')

    # l enters the docs dir again; then l enters nested/.
    s.send(b'j')
    s.require('2/3', 'j should move onto docs/')
    s.send(b'l')
    s.require('tree.json', 'l on docs/ should enter the directory')
    s.forbid('root.txt', 'inside docs/ there should be no root.txt')
    s.send(b'j')
    s.require('2/5', 'j should move onto nested/')
    s.send(b'l')
    s.require('deep.txt', 'l should enter nested/')

    # l on a file does nothing; only Enter picks it.
    s.send(b'j')
    s.require('2/2', 'j should move onto deep.txt')
    s.send(b'l')
    s.require('2/2', 'l on a file must not pick or move it')
    s.send(b'\r')
    s.forbid(' File Browser ', 'Enter should close the dialog')
    s.require('Error: could not parse',
              'Enter should run :open on nested/deep.txt (a non-JSON file)')

    # h goes up one level; Esc cancels without doing anything.
    s.send(b':open docs')
    s.send(b'\r')
    s.require(' File Browser ')
    s.require('tree.json', 'the browser should reopen on docs/')
    s.send(b'h')
    s.require('docs/', 'h should go up one level (back to the workdir)')
    s.require('root.txt', 'the workdir still lists root.txt')
    s.send(b'h')
    s.forbid('root.txt', 'a second h should go up to the parent directory')
    s.send(b'\x1b')
    s.forbid(' File Browser ', 'Esc should close the dialog')

    # `.` opens the browser on the current directory.
    s.send(b':open .')
    s.send(b'\r')
    s.require(' File Browser ', 'a `.` arg should open the browser on cwd')
    s.require('docs/', 'cwd listing should show docs/')
    s.send(b'\x1b')
    s.forbid(' File Browser ')

    # :saveas with a directory also opens the browser; the pick saves there.
    s.send(b':saveas docs')
    s.send(b'\r')
    s.require(' File Browser ', 'a dir arg to :saveas should open the browser')
    s.send(b'G')
    s.require('5/5')
    s.send(b'\r')
    s.require('Saved 1 nodes to', 'the pick should be saved via :saveas')
finally:
    s.quit()

print('PASS')
