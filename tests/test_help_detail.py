#!/usr/bin/env python3
"""Help dialog: detailed topics read from commands_detail/<mode>_<key>.txt."""
import harness


s = harness.launch(cols=120, rows=30)
try:
    # TREE ArrowUp has a detail file, so Enter opens its topic.
    s.send(b'?')
    s.require('Move selection up', 'the help dialog should open')

    s.send(b':arrow')
    s.send(b'\r')
    s.require('? Help - TREE ArrowUp', 'Enter should open the ArrowUp topic')
    s.require('alternative to the', 'the commands_detail file should be shown')
    s.require('Enter/Esc back', 'the topic view should be scrollable and closable')
    s.send(b'\x1b')
    s.send(b'\x1b')

    # A binding with its own detail file opens that file, not another
    # binding's (TREE j has tree_j.txt).
    s.send(b'?')
    s.send(b':j')
    s.send(b'\r')
    s.require('? Help - TREE j', 'Enter should open the TREE j topic')
    s.require('Moves the cursor down', 'the TREE j detail file should be shown')
    s.forbid('alternative to the', "TREE j should not pick up ArrowUp's detail file")
    s.send(b'\x1b')
    s.send(b'\x1b')

    # NORMAL ArrowUp has no detail file, so Enter does nothing and the list
    # stays on screen.
    s.send(b'?')
    s.send(b':arrowup')
    s.send(b'\x1b[B')
    s.send(b'\r')
    s.require('Filter: :arrowup', 'Enter without a detail file should keep the list')
    s.forbid('? Help - NORMAL', 'Enter without a detail file should do nothing')
finally:
    s.quit()

print('PASS')
