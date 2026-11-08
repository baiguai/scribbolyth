#!/usr/bin/env python3
"""Feature test for the TREE-mode link tree dialog ('V').

Builds a small link graph, opens the dialog, checks the tree structure,
filters it, folds/unfolds it, and finally opens a node with Enter.

    Alpha -> Beta, Gamma
    Beta  -> Delta
    Gamma -> Delta
    Epsilon -> Zeta
"""
import harness


def add_node(s, name):
    s.send(b'a')
    s.send(name)
    s.send(b'\r')


def set_text(s, text):
    s.send(b'I')
    s.send(text)
    s.send(b'\x1b')   # INSERT -> NORMAL
    s.send(b'\x1b')   # NORMAL -> TREE


s = harness.launch()
try:
    add_node(s, 'Alpha')
    set_text(s, '_Beta_ _Gamma_')
    add_node(s, 'Beta')
    set_text(s, '_Delta_')
    add_node(s, 'Gamma')
    set_text(s, '_Delta_')
    add_node(s, 'Delta')
    set_text(s, 'delta leaf text')
    add_node(s, 'Epsilon')
    set_text(s, '_Zeta_')
    add_node(s, 'Zeta')
    set_text(s, 'zeta leaf text')

    # V opens the link tree. Alpha and Epsilon are the roots; Beta/Gamma are
    # children of Alpha, Delta appears under both, Zeta under Epsilon.
    s.send(b'\x1b')   # deselect so V is not interpreted as an edit
    s.send(b'V')
    s.require('V Link Tree', 'V should open the link tree')
    s.require('Alpha', 'Alpha should be a root')
    s.require('Beta', 'Beta should be a child of Alpha')
    s.require('Gamma', 'Gamma should be a child of Alpha')
    s.require('Epsilon', 'Epsilon should be a root')
    s.require('1/7', 'all link-tree rows should be visible')

    # h folds the selected root (Alpha); l unfolds it again.
    s.send(b'h')
    s.require('1/3', 'folding Alpha should hide its branch')
    s.send(b'l')
    s.require('1/7', 'unfolding Alpha should show its branch again')

    # '/' filters; a matching note keeps all of its children, while branches
    # with no matching note disappear.
    s.send(b'/')
    s.send(b'gamma')
    s.require('Filter: gamma', 'filter line should show the query')
    s.require('1/3', 'Alpha, Gamma and Gamma\'s child Delta should remain')
    s.require('Gamma', 'Gamma should be visible')
    s.require('Delta', 'children of a matching note should be visible')

    # Escape while filtering cancels the filter but keeps the dialog open.
    s.send(b'\x1b')
    s.require('V Link Tree', 'dialog should stay open when cancelling a filter')
    s.require('1/7', 'cancelling the filter should restore every row')

    # Escape outside filtering closes the dialog.
    s.send(b'\x1b')
    s.forbid('V Link Tree', 'Escape should close the dialog')

    # Reopen, move down to Delta (under Beta) and open it with Enter.
    s.send(b'V')
    s.require('V Link Tree', 'dialog should reopen')
    s.send(b'j')   # Beta
    s.send(b'j')   # Delta under Beta
    s.send(b'\r')
    s.forbid('V Link Tree', 'Enter should close the dialog')
    s.require('delta leaf text', 'Enter should display the selected node')
finally:
    s.quit()

print('PASS')
