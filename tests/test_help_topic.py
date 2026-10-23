#!/usr/bin/env python3
"""Help dialog: Enter on a binding opens its full help topic, Esc goes back."""
import harness

s = harness.launch(cols=120, rows=30)
try:
    # open the help dialog
    s.send(b'?')
    s.require('? Help', 'help dialog should open')
    s.require('Move selection up', 'the short description should be listed')

    # the first entry is TREE ArrowUp, which carries a full help topic
    s.send(b'\r')
    s.require('? Help - TREE ArrowUp', 'the full help topic should be shown')
    s.require('Enter/Esc back', 'the topic view should be scrollable and closable')
    s.forbid('Filter:', 'the binding list should be replaced by the topic')

    # Esc goes back to the list of key bindings
    s.send(b'\x1b')
    s.require('Filter:', 'the list of key bindings should be back')
    s.require('Move selection up', 'the binding list should be shown again')
    s.forbid('Enter/Esc back', 'the topic view should be gone')

    # Esc closes the dialog
    s.send(b'\x1b')
    s.forbid('? Help', 'the help dialog should be closed')
finally:
    s.quit()

print('PASS')
