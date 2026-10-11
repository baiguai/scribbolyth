#!/usr/bin/env python3
"""Feature test for follow-link (Ctrl+L in NORMAL): with the cursor on a URL it
opens it externally, with the cursor on a `_Note_` it reveals the target node in
the tree, and with the cursor off any link it reports it in the status area.
"""
import harness

s = harness.launch(cols=120, rows=24)
try:
    # Alpha with a standalone URL line and a _Second Note_ line.
    s.send(b'a')
    s.require('create_node')
    s.send(b'Alpha'); s.send(b'\r')
    s.send(b'I')
    s.send('Visit https://example.com/page for details'.encode()); s.send(b'\r')
    s.send('Write to _Second Note_ when done'.encode())
    s.send(b'\x1b')           # INSERT -> NORMAL
    s.send(b'\x1b')           # NORMAL -> TREE

    # Second Note node: the target of the _Second Note_ link.
    s.send(b'a')
    s.require('create_node')
    s.send(b'Second Note'); s.send(b'\r')
    s.require('Second Note')
    s.send(b'k')              # back to Alpha (row 0)

    # Enter NORMAL mode (TREE `i`), cursor at (0,0).
    s.send(b'i')

    # Cursor off any link: line 0, col 0 is 'V'.
    s.send(b'\x0c')
    s.require('No link under cursor', 'Ctrl+L off a link should report it')

    # Cursor onto the URL: "Visit " is 6 chars, so col 6 is 'h' of http.
    s.send(b'llllll')
    s.send(b'\x0c')
    s.require('Opened: https://example.com/page',
              'Ctrl+L on a URL should open it and report in the status bar')

    # Cursor onto _Second Note_ (starts at col 9 of line 1).
    s.send(b'j')              # cursor_down -> line 1, col 6
    s.send(b'lll')            # col 6 -> 9, the opening '_'
    s.send(b'\x0c')
    s.require('Followed: Second Note',
              'Ctrl+L on a note link should reveal the target node')
    if s.inv_rows(0, 30) != [1]:
        print('FAIL: Second Note should be selected in the tree, got %r'
              % s.inv_rows(0, 30))
        s.dump()
        raise SystemExit(1)
finally:
    s.quit()

print('PASS')
