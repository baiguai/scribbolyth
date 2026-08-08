#!/usr/bin/env python3
"""Feature test for the recent-files feature: saving/opening records paths
in init.conf (most recent first); 'O' (TREE/NORMAL) and ':O' open a dialog
that lists them; j/k move, Enter opens the selected file, Esc cancels; the
list survives relaunches and ':enew' (which only clears auto-restore).

Permanent regression for config/ReadInit, config/WriteInit,
treeview/PushRecentFile and src/recent/.
"""
import os
import shutil

import harness

DATA_DIR = '/tmp/scribbolyth_tests/recent_files_data'
shutil.rmtree(DATA_DIR, ignore_errors=True)
os.makedirs(DATA_DIR)

init_conf = os.path.join(DATA_DIR, 'init.conf')


def recent_lines():
    with open(init_conf, encoding='utf-8') as f:
        return [l.strip() for l in f if l.strip().startswith('recent_file')]


def last_line():
    with open(init_conf, encoding='utf-8') as f:
        for l in f:
            if l.strip().startswith('last_file'):
                return l.strip()
    return ''


# --- save two documents: init.conf should list them, newest first ---------
s = harness.launch(workdir=DATA_DIR)
try:
    s.require('Select a node to edit', 'first launch must start blank')

    # doc A
    s.send(b'a')
    s.require('create_node')
    s.send(b'A'); s.send(b'\r')
    s.send(b'I'); s.send(b'alpha body')
    s.send(b'\x1b'); s.send(b'\x1b')              # NORMAL -> TREE
    s.send(b'S')
    s.require('saveas')
    s.send(b'alpha.json'); s.send(b'\r')
    s.require('Saved', 'save should write the document')

    # fresh doc, then doc B (saved last -> auto-restore target)
    s.send(b':n')
    s.send(b'\r')
    s.require('Select a node to edit', ':n should blank the document')
    s.send(b'a')
    s.require('create_node')
    s.send(b'B'); s.send(b'\r')
    s.send(b'I'); s.send(b'beta body')
    s.send(b'\x1b'); s.send(b'\x1b')
    s.send(b'S')
    s.require('saveas')
    s.send(b'beta.json'); s.send(b'\r')
    s.require('Saved', 'save should write the document')
finally:
    s.quit()

lines = recent_lines()
if len(lines) != 2 or 'beta.json' not in lines[0] or 'alpha.json' not in lines[1]:
    print('FAIL: init.conf recents wrong: %r' % lines)
    raise SystemExit(1)

# --- relaunch: auto-restore AND the recents dialog both come back ---------
s = harness.launch(workdir=DATA_DIR)
try:
    s.require('Loaded 1 nodes from beta.json',
              'relaunch should auto-restore beta.json')
    s.send(b'j')                                  # select the node
    s.require('beta body', 'auto-restored doc should show its text')
    s.send(b'O')                                  # TREE mode, capital o
    s.require('Recent Files', 'O should open the recent-files dialog')
    s.require('1/2', 'two recents restored, selection on the newest')
    s.require('beta.json', 'newest entry should be listed')
    s.require('alpha.json', 'older entry should be listed')
    s.send(b'j')
    s.require('2/2', 'j should move down the list')
    s.send(b'k')
    s.require('1/2', 'k should move back up')
    s.send(b'\x1b')
    s.forbid('Recent Files', 'Esc should close the dialog')

    # the dialog consumes keys: `a` must not open create_node
    s.send(b'O')
    s.require('Recent Files')
    s.send(b'a')
    s.forbid('create_node', 'dialog must consume a (no create_node prompt)')
    s.send(b'\x1b')
finally:
    s.quit()

# --- Enter opens the selected file -----------------------------------------
s = harness.launch(workdir=DATA_DIR)
try:
    s.send(b'O')
    s.require('Recent Files')
    s.send(b'\r')                                 # Enter on beta.json
    s.forbid('Recent Files', 'Enter should close the dialog')
    s.require('Loaded 1 nodes from beta.json',
              'Enter should open the selected file')
finally:
    s.quit()

# --- :enew clears auto-restore but NOT the recents list --------------------
s = harness.launch(workdir=DATA_DIR)
try:
    s.send(b':enew')
    s.send(b'\r')
    s.require('Select a node to edit', ':enew should blank the document')
finally:
    s.quit()
if not last_line().endswith('='):
    print('FAIL: :enew should clear last_file, got %r' % last_line())
    raise SystemExit(1)
lines = recent_lines()
if len(lines) != 2:
    print('FAIL: :enew must not wipe recent_file lines, got %r' % lines)
    raise SystemExit(1)

s = harness.launch(workdir=DATA_DIR)
try:
    s.require('Select a node to edit', 'after :enew no auto-restore')
    s.send(b'O')
    s.require('Recent Files')
    s.require('1/2', 'recents should survive :enew + relaunch')
    s.send(b'\x1b')
finally:
    s.quit()

# --- NORMAL mode 'O' and the ':O' command -----------------------------------
s = harness.launch(workdir=DATA_DIR)
try:
    s.send(b'O')
    s.require('Recent Files', 'O should open the dialog in TREE mode')
    s.send(b'\x1b')

    s.send(b'i')                                  # TREE -> NORMAL
    s.send(b'O')
    s.require('Recent Files', 'O should open the dialog in NORMAL mode')
    s.send(b'\x1b')

    s.send(b':O')
    s.send(b'\r')
    s.require('Recent Files', ':O should open the dialog via the command')
    s.send(b'\x1b')
    s.forbid('Recent Files', 'Esc should close the dialog')
finally:
    s.quit()

print('PASS')
