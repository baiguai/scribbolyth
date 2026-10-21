#!/usr/bin/env python3
"""Regression tests for multi-key sequences interrupted by a synthetic event.

FTXUI delivers its own events (the status-bar repaint wakeup, the terminal
resize) as "special" events carrying a single NUL byte. They look like a key
press but are not one: handling them as a key aborted the pending key
sequence, so "d d" stopped deleting the line and "g t" fell through to the
single-key "t" binding (the export_note_txt prompt).
"""
import harness


def editor_with_two_lines():
    """A note with two lines, cursor on the first line, NORMAL mode."""
    s = harness.launch()
    s.send(b'a')
    s.send(b'Alpha')
    s.send(b'\r')
    s.send(b'I')
    s.send(b'first line')
    s.send(b'\r')
    s.send(b'second line')
    s.send(b'\x1b')
    s.send(b'k')                 # cursor -> first line
    s.step(0.2)
    s.require('first line', 'setup')
    return s


def test_dd_survives_status_message():
    """'d d' must delete the line even with a status message on screen."""
    s = editor_with_two_lines()
    try:
        s.send(b'y')                     # copy -> status message
        s.require('Copied', 'status message should be on screen')

        s.send(b'd', wait=0.05)
        s.step(0.5)                      # longer than the 200ms status ticker
        s.send(b'd', wait=0.4)

        s.forbid('first line', '"d d" should delete the line despite the ticker')
        s.require('second line', 'the other line must stay')
    finally:
        s.quit()


def test_gt_does_not_fall_through():
    """'g t' must not fall through to the single-key 't' (export prompt)."""
    s = harness.launch()
    try:
        s.send(b'a')
        s.send(b'Alpha')
        s.send(b'\r')
        s.step(0.3)
        s.send(b'm')                     # bookmark -> status message
        s.require('Bookmarked', 'status message should be on screen')

        s.send(b'g', wait=0.05)
        s.step(0.5)                      # longer than the 200ms status ticker
        s.send(b't', wait=0.4)

        s.forbid('export_note_txt',
                 '"g t" must stay a sequence, not fall through to "t"')
    finally:
        s.quit()


def test_dd_survives_terminal_resize():
    """'d d' must survive a terminal resize (SIGWINCH) in the middle."""
    s = editor_with_two_lines()
    try:
        s.send(b'd', wait=0.1)
        s.resize(rows=30, cols=100)      # SIGWINCH -> synthetic event
        s.step(0.3)
        s.send(b'd', wait=0.4)

        s.forbid('first line', '"d d" should survive a terminal resize')
    finally:
        s.quit()


test_dd_survives_status_message()
test_gt_does_not_fall_through()
test_dd_survives_terminal_resize()

print('PASS')
