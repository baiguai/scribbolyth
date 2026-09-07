#!/usr/bin/env python3
"""Feature test: 'p' in VISUAL/VISUAL_LINE/VISUAL_BLOCK replaces the selection
with the clipboard (like Vim), instead of inserting at the cursor.

The clipboard is seeded through xclip (stdout/stderr redirected so the
daemonized xclip does not keep the test's output pipe open).
"""
import subprocess

import harness


def set_clip(text):
    subprocess.run(['xclip', '-selection', 'clipboard'],
                   input=text.encode(),
                   stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, check=True)


def ed(row):
    return s.row_text(row)[31:]


s = harness.launch(cols=80, rows=24)
try:
    # Build a document: the first line 'Alpha' becomes the node title (hidden
    # from the editor pane); the editor shows the body lines so far.
    s.send(b'a'); s.send(b'Alpha'); s.send(b'\r'); s.send(b'I')
    s.send(b'hello there world'); s.send(b'\r'); s.send(b'second line here')
    s.send(b'\r'); s.send(b'third line'); s.send(b'\x1b')
    s.step(0.3)

    # --- VISUAL: replace the word 'there' (cols 6-10) with 'XXX' ----------
    s.send(b'gg')            # cursor to the first editor line
    s.step(0.2)
    s.send(b'0')
    for _ in range(6):
        s.send(b'l')         # cursor to col 6 ('t' of 'there')
    set_clip('XXX')
    s.send(b'v')             # anchor at col 6
    s.send(b'e')             # extend selection to end of 'there'
    s.send(b'p')             # paste should replace 'there'
    s.step(0.3)
    assert ed(0).startswith('hello XXX world'), (
        'VISUAL p must replace the selection, got %r' % ed(0))
    assert 'NORMAL' in s.row_text(s.rows - 1), (
        'mode must return to NORMAL after pasting over a selection')

    # --- VISUAL_LINE: replace the current line with a multi-line clipboard ---
    set_clip('AAA\nBBB')
    s.send(b'V')             # line-wise visual on the 'hello XXX world' line
    s.send(b'p')             # paste should replace the whole line
    s.step(0.3)
    s.send(b'gg')
    s.step(0.2)
    assert ed(0).strip() == 'AAA', "the pasted block should start at the replaced line, got %r" % ed(0).strip()
    assert ed(1).strip() == 'BBB', "there should be a second pasted line 'BBB', got %r" % ed(1).strip()
    assert ed(2).startswith('second line here'), 'the line after the selection must survive, got %r' % ed(2)
    assert ed(3).startswith('third line'), 'the last line must survive, got %r' % ed(3)

    # --- VISUAL_BLOCK: replace a 2x2 block with a single-char paste ---------
    set_clip('z')
    s.send(b'j')             # cursor to editor row 1 ('BBB')
    s.step(0.2)
    s.send(b'0')             # col 0
    s.step(0.2)
    s.send(b'\x16')          # Ctrl+V enters VISUAL BLOCK, anchored at (1,0)
    s.step(0.2)
    s.send(b'j')             # block rows 1-2
    s.step(0.2)
    s.send(b'l')             # block cols 0-1
    s.step(0.2)
    s.send(b'p')             # erase the block, insert 'z' at its top-left
    s.step(0.3)
    assert ed(0).strip() == 'AAA', 'the first line must be untouched, got %r' % ed(0).strip()
    assert ed(1).strip() == 'zB', "block paste must delete the block columns and insert 'z', got %r" % ed(1).strip()
    assert ed(2).startswith('cond line here'), 'block paste must erase the block cols on the second row, got %r' % ed(2)
    assert 'NORMAL' in s.row_text(s.rows - 1), (
        'mode must return to NORMAL after VISUAL_BLOCK p')
finally:
    s.quit()

print('PASS')