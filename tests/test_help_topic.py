#!/usr/bin/env python3
"""Help dialog: filter, navigate, and open the full help topic of a binding."""
import re

import harness


def highlighted(s):
    rows = s.inv_rows()
    if len(rows) != 1:
        print('FAIL: expected exactly one highlighted row, got %r' % rows)
        s.dump()
        raise SystemExit(1)
    return s.row_text(rows[0])


s = harness.launch(cols=120, rows=30)
try:
    # open the help dialog
    s.send(b'?')
    s.require('? Help', 'help dialog should open')
    s.require('Move selection up', 'the short description should be listed')

    # filter to the arrow-key bindings, then navigate the filtered list
    s.send(b':arrow')
    first = highlighted(s)
    s.send(b'\x1b[B')
    second = highlighted(s)
    if first == second:
        print('FAIL: Down did not move the selection after filtering')
        s.dump()
        raise SystemExit(1)

    m = re.search(r'│ (\S+)\s+(\S+)\s+', second)
    if not m:
        print('FAIL: could not read the selected binding from %r' % second)
        raise SystemExit(1)
    mode, key = m.group(1), m.group(2)

    # Enter opens the full help topic of the selected binding
    s.send(b'\r')
    s.require('? Help - %s %s' % (mode, key), 'Enter should open the selected topic')
    s.require('Enter/Esc back', 'the topic view should be scrollable and closable')
    s.forbid('Filter:', 'the binding list should be replaced by the topic')

    # Esc goes back to the same filtered list, with the selection kept
    s.send(b'\x1b')
    s.require('Filter: :arrow', 'the filtered list should be back')
    if highlighted(s) != second:
        print('FAIL: the selection was lost when returning from the topic')
        s.dump()
        raise SystemExit(1)

    # Esc closes the dialog
    s.send(b'\x1b')
    s.forbid('? Help', 'the help dialog should be closed')

    # A GLOBAL binding without a topic of its own still opens help: it reuses
    # the topic of the binding that runs the same operation (TREE '?').
    s.send(b'?')
    s.send(b'help')
    first = highlighted(s)
    s.send(b'\x1b[B')
    second = highlighted(s)
    if 'GLOBAL' not in second:
        print('FAIL: expected the second match to be a GLOBAL binding, got %r' % second)
        s.dump()
        raise SystemExit(1)
    s.send(b'\r')
    s.require('? Help - GLOBAL', 'Enter should open help for a topic-less binding too')
    s.require('Enter/Esc back', 'the fallback topic view should open')
finally:
    s.quit()

print('PASS')
