#!/usr/bin/env python3
"""Test the '+:' search prefix: every word must appear, in any order, due to a
generated lookahead regex `^(?=.*\bw1\b)(?=.*\bw2\b)...(?=.*\bwn\b).*$`."""
import json
import os

import harness

s = harness.launch(cols=90, rows=24)
try:
    def editor_cursor():
        """(row, col) of the editor's inverted cursor char in the pane."""
        for r in range(0, s.rows - 2):
            for c in range(31, s.cols):
                if 'inv' in s.grid[r][c][1]:
                    return (r, c)
        return None

    def cursor_lies_on(row, token):
        cur = editor_cursor()
        if cur is None or cur[0] != row:
            return False
        return token in s.row_text(row)[cur[1]:cur[1] + len(token)]

    # Alpha with a three-line body: apple/banana on lines 1-2, and vodka+lime
    # split across line 3 so a cross-line all-words search can be asserted.
    s.send(b'a')
    s.send(b'Alpha')
    s.send(b'\r')
    s.step(0.3)
    s.send(b'i')
    s.step(0.3)
    s.send(b'i')
    s.step(0.3)
    s.send(b'apple banana cherry pie\r')
    s.step(0.2)
    s.send(b'apple mango banana\r')
    s.step(0.2)
    s.send(b'vodka lime cooler')
    s.step(0.3)
    s.send(b'\x1b')
    s.step(0.3)
    s.require('cooler', 'alpha body entered')

    def search(query):
        """Type '/' (opens the command field), then the query, then Enter."""
        s.send(b'/')
        s.step(0.2)
        s.send(query.encode())
        s.step(0.2)
        s.send(b'\r')
        s.step(0.3)

    # 1) +: matches when every word is present, in any order
    search('+:banana apple')
    assert 'Match 1 of 1' in s.row_text(s.rows - 1), 'node containing all words matches'
    cur = editor_cursor()
    assert cur is not None, 'cursor must land on a word'
    assert s.row_text(cur[0])[cur[1]] in 'ab', 'cursor must sit on apple or banana'
    print('ok: +: matches a node holding every word')

    # 2) a word can appear anywhere in the line (reversed order)
    search('+:cherry pie')
    assert 'Match 1 of 1' in s.row_text(s.rows - 1), 'order of the query does not matter'
    cur = editor_cursor()
    assert cur is not None and s.row_text(cur[0])[cur[1]] == 'c', 'cursor on cherry'
    print('ok: the query order is irrelevant')

    # 3) words on DIFFERENT lines still satisfy the query (the regression:
    #    a note whose words never share a single line must still match)
    search('+:vodka lime')
    assert 'Match 1 of 1' in s.row_text(s.rows - 1), \
        'words on different lines must match'
    cur = editor_cursor()
    assert cur is not None and 'vodka' in s.row_text(cur[0]), \
        'cursor lands on the vodka line'
    print('ok: +: matches words spread across different lines')

    # 4) a word absent everywhere rejects the search
    search('+:banana mango kiwi')
    assert 'Pattern not found' in s.row_text(s.rows - 1), \
        'kiwi is nowhere in the node, so the query must not match'
    print('ok: a missing word rejects the search')

    # 5) case-insensitive
    search('+:APPLE CHERRY')
    assert 'Match 1 of 1' in s.row_text(s.rows - 1), 'all-words search is case-insensitive'
    print('ok: +: search is case-insensitive')

    # 6) n/N step through every word occurrence, wrapping at the end
    search('+:banana apple')
    start = editor_cursor()
    assert start is not None, 'cursor lands on the first word occurrence'
    s.send(b'n')
    s.step(0.2)
    nxt = editor_cursor()
    assert nxt is not None and nxt != start, 'n must step to the next word'
    for _ in range(3):  # 4 presses total from the start wraps back to it
        s.send(b'n')
        s.step(0.05)
    cur = editor_cursor()
    assert cur == start, 'n must wrap around to the first occurrence'
    s.send(b'N')
    s.step(0.2)
    back = editor_cursor()
    assert back is not None and back != start, 'N must step back one occurrence'
    print('ok: n/N step through every word occurrence')

finally:
    s.quit()

# the TREE-mode / dialog filters the whole document through the same +: logic
d = harness.launch(cols=92, rows=30)
try:
    doc_path = os.path.join(d.workdir, 'doc.json')
    doc = {
        'version': 1,
        'roots': [
            {'id': 'n1', 'name': 'Alpha', 'expanded': True,
             'text': 'sphinx under the pyramid'},
            {'id': 'n2', 'name': 'Beta', 'expanded': True,
             'text': 'pyramid at the sphinx'},
            {'id': 'n3', 'name': 'Gamma', 'expanded': True,
             'text': 'pyramid only'},
        ],
    }
    with open(doc_path, 'w', encoding='utf-8') as f:
        f.write(harness.SIGNATURE)
        json.dump(doc, f)
    d.send((':open %s\r' % doc_path).encode())
    d.require('Loaded', 'doc should load')

    d.send(b'/')
    d.require('/ Search', 'search dialog should open')
    d.send(b'+:pyramid sphinx')
    d.require('Search: +:pyramid sphinx_', 'filter should show the +: query')
    d.require('Enter to search', 'regex/all-words filters defer until Enter')
    d.forbid('Alpha', 'results must not appear before Enter', rows=range(3, 26))

    d.send(b'\x7f' * 7)  # '+:pyramid sphinx' -> '+:pyramid'
    d.send(b' cat')       # '+:pyramid cat'
    d.require('Search: +:pyramid cat_', 'filter should show +:pyramid cat')
    d.require('Enter to search', 'still deferred while typing')
    d.send(b'\r')
    d.require('Search: +:pyramid cat_', 'dialog stays open on no match')
    d.require('No matches', 'no node holds both pyramid and cat')

    # Enter runs the deferred search and lists the matching notes
    d.send(b'\x7f' * 4)  # '+:pyramid cat' -> '+:pyramid'
    d.send(b' sphinx')
    d.require('Search: +:pyramid sphinx_', 'rebuilt the matching query')
    d.require('Enter to search', 'deferred until Enter')
    d.send(b'\r')
    d.require('Alpha', 'Alpha holds pyramid and sphinx')
    d.require('Beta', 'Beta holds pyramid and sphinx')
    d.forbid('Gamma', 'Gamma has no sphinx', rows=range(3, 26))
    d.send(b'\r')
    d.forbid('Search:', 'Enter on a note should close the dialog',
             rows=range(3, 26))
    print('ok: the tree dialog applies +: across the whole document')
finally:
    d.quit()

print('PASS')