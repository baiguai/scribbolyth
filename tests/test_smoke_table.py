#!/usr/bin/env python3
"""Smoke test: build/update a markdown table from VISUAL and NORMAL mode.

Select pipe rows in the editor (v + j...), press ';' and the app rebuilds
them into a padded table with a '+---+' separator row, mirroring the web
app's Ctrl+; (formatMarkdownTable). With no selection, ';' now auto-detects
the whole table at the cursor and rebuilds it top to bottom.
"""
import harness

s = harness.launch()
try:
    s.require('Keycode help', 'app must start blank')

    s.send(b'a')
    s.require('create_node', 'a should open the :create_node prompt')
    s.send(b'Alpha')
    s.send(b'\r')
    s.require('Alpha', 'Alpha should appear in the tree')

    # type the unformatted table body (4 lines)
    s.send(b'I')
    s.send(b'| name | age |')
    s.send(b'\r')
    s.send(b'|----|---|')
    s.send(b'\r')
    s.send(b'| alice | 30 |')
    s.send(b'\r')
    s.send(b'| bob | 5 |')
    s.send(b'\x1b')
    s.require('| name | age |', 'raw table rows should be visible')

    # cursor is on the last row after Esc; jump to the top and select all
    s.send(b'gg')
    s.send(b'v')
    s.require('VISUAL', 'v should enter VISUAL mode')
    s.send(b'j')
    s.send(b'j')
    s.send(b'j')
    if s.inv_rows(31, 80) != [0, 1, 2, 3]:
        print('FAIL: expected rows 0-3 selected, got %r' % s.inv_rows(31, 80))
        s.dump()
        raise SystemExit(1)

    # ';' rebuilds the padded table from the VISUAL selection
    s.send(b';')
    s.require('| name  | age |', 'header row should be padded')
    s.require('+-------+-----+', 'separator row should be rebuilt')
    s.require('| alice | 30  |', 'first data row should be padded')
    s.require('| bob   | 5   |', 'second data row should be padded')
    s.require('NORMAL', 'formatting should return to NORMAL mode')
    s.require('Table updated', 'status should report the update')

    # NORMAL auto-detect is a no-op on prose (no pipes nearby)
    s.send(b'G')
    s.send(b'a')
    s.send(b'\r')
    s.send(b'plain prose here')
    s.send(b'\x1b')
    s.send(b';')
    s.require('No table detected', 'prose without pipes should be a no-op')
    s.require('NORMAL', 'the no-op should leave NORMAL mode')
    s.require('| bob   | 5   |', 'the table should be left untouched')

    # NORMAL auto-detect rebuilds a whole ragged table from any one row
    s.send(b'a')
    s.send(b'\r')
    s.send(b'| x   | y |')
    s.send(b'\r')
    s.send(b'|-----|---|')
    s.send(b'\r')
    s.send(b'| longer | 2 |')
    s.send(b'\x1b')
    s.require('| longer | 2 |', 'raw table rows should be visible')
    s.send(b';')
    s.require('| x      | y |', 'header row should be padded via auto-detect')
    s.require('+--------+---+', 'separator row should be rebuilt via auto-detect')
    s.require('| longer | 2 |', 'data row should be padded via auto-detect')
    s.require('Table updated', 'status should report the auto-detected update')

finally:
    s.quit()

print('PASS')