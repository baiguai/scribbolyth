#!/usr/bin/env python3
"""Test the '\' key: like '/' it opens a search dialog, but Enter creates a new
"Search results: <query>" node whose body holds one _Title_ link per matching
document node, instead of jumping to a single result."""
import json
import os

import harness

doc = {
    'version': 1,
    'roots': [
        {'id': 'n1', 'name': 'Alpha', 'expanded': True,
         'text': 'vodka lime cooler'},
        {'id': 'n2', 'name': 'Beta', 'expanded': True,
         'text': 'vodka martini'},
        {'id': 'n3', 'name': 'Gamma', 'expanded': True,
         'text': 'lime cooler'},
    ],
}


def make_doc(s):
    doc_path = os.path.join(s.workdir, 'doc.json')
    with open(doc_path, 'w', encoding='utf-8') as f:
        f.write(harness.SIGNATURE)
        json.dump(doc, f)
    return doc_path


# The dialog window's rows (the tree rows sit at the top of the screen).
DLG = range(3, 27)

s = harness.launch(cols=92, rows=30)
try:
    s.send((':open %s\r' % make_doc(s)).encode())
    s.require('Loaded', 'doc should load')

    # ---- TREE: '\' opens the collect dialog; Enter creates the node ----
    s.send(b'\\')
    s.require('\\ Results', 'backslash opens the collect-search dialog')
    s.require('Search:', 'the search field is shown')

    s.send(b'vodka')
    s.require('Alpha', 'Alpha contains vodka and is listed')
    s.require('Beta', 'Beta contains vodka and is listed')

    s.send(b'\r')
    s.require('Search results: vodka', 'results node title carries the query')
    s.require('Created search-node with 2 links', 'status reports the created node')
    s.forbid('Search:', 'Enter closes the dialog', rows=DLG)
    s.require('[+]', 'creating the node marks the document changed')

    # ---- 'i' shows the created node in the editor: one link per match ----
    s.send(b'i')
    s.step(0.3)
    s.require('_Alpha_', 'body links Alpha')
    s.require('_Beta_', 'body links Beta')
    s.forbid('_Gamma_', 'Gamma did not match, so it is not linked')

    # ---- NORMAL: '\' works the same from inside a note ----
    s.send(b'\\')
    s.require('\\ Results', 'backslash also opens the collect dialog in NORMAL')
    s.send(b'lime')
    s.send(b'\r')
    s.require('Search results: lime', 'second results node created from NORMAL')
    s.require('Created search-node with 2 links', 'lime matches Alpha and Gamma')
    s.forbid('Search:', 'dialog closed after creating', rows=DLG)
    s.forbid('\\ Results', 'dialog window is gone')

    # the editor now shows the newly created node's links
    s.require('_Gamma_', 'editor shows the lime results node linking Gamma')
    s.forbid('_Beta_', 'Beta has no lime, so it is not linked')

finally:
    s.quit()

# ---- deferred '+:' search through the '\' dialog ----
d = harness.launch(cols=92, rows=30)
try:
    d.send((':open %s\r' % make_doc(d)).encode())
    d.require('Loaded', 'doc should load')

    d.send(b'\\')
    d.require('\\ Results', 'collect dialog opens')
    d.send(b'+:vodka lime')
    d.require('Search: +:vodka lime_', 'filter shows the +: query')
    d.require('Enter to search', '+: is deferred until Enter')
    d.forbid('Alpha', 'no results are listed before Enter', rows=DLG)

    d.send(b'\r')
    d.require('Alpha', 'first Enter runs the deferred search and lists Alpha')
    d.forbid('Gamma', 'Gamma has lime but no vodka', rows=DLG)

    d.send(b'\r')
    d.require('Search results: vodka lime', 'title strips the +: prefix')
    d.require('Created search-node with 1 link', 'only Alpha matched')
    d.forbid('Search:', 'second Enter creates the node and closes', rows=DLG)
    d.require('[+]', 'document marked changed')

finally:
    d.quit()

print('PASS')