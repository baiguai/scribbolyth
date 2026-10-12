#!/usr/bin/env python3
"""TREE mode shows the selected node's recursive descendant count in the
status bar, and updates it when a different node is selected.

Builds Doc -> Alpha -> AlphaKid -> Deep and checks:
  - selecting Alpha reports 2 descendants (AlphaKid + Deep),
  - selecting AlphaKid reports 1 descendant (singular),
  - selecting a leaf (Deep) or a childless node (Doc) reports 0 descendants.
The count is recursive and independent of expand/collapse state.
"""
import harness

s = harness.launch(cols=80, rows=24)
try:
    # Doc, Alpha(AlphaKid(Deep)) -- Deep is the deepest leaf.
    s.send(b'a'); s.send(b'Doc'); s.send(b'\r')
    s.send(b'a'); s.send(b'Alpha'); s.send(b'\r')
    s.send(b'A'); s.send(b'AlphaKid'); s.send(b'\r')
    s.send(b'A'); s.send(b'Deep'); s.send(b'\r')

    # Deep -> AlphaKid -> Alpha: Alpha holds AlphaKid and Deep.
    s.send(b'k'); s.send(b'k')
    s.require('2 descendants', 'Alpha should report 2 descendants')

    # AlphaKid has one child (Deep).
    s.send(b'j')
    s.require('1 descendant', 'AlphaKid should report 1 descendant')

    # Deep is a leaf.
    s.send(b'j')
    s.require('0 descendants', 'Deep should report 0 descendants')

    # Doc (a childless root) also reports none.
    s.send(b'k'); s.send(b'k'); s.send(b'k')
    s.require('0 descendants', 'Doc should report 0 descendants')

    # Collapsing Alpha must not change the recursive count.
    s.send(b'j')                 # Doc -> Alpha
    s.send(b'C')                 # collapse all
    s.require('2 descendants', 'count must survive collapse')
finally:
    s.quit()

print('PASS')
