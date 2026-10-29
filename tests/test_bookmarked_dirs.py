#!/usr/bin/env python3
"""Feature test for bookmarked directories in the file browser.

In the browser, 'm' stores the shown directory in init.conf, '`' lists the
bookmarks, Enter opens the selected one, 'D' removes it and Esc returns to
the normal browser. Bookmarks survive a relaunch.

Permanent regression for src/browser/ and config/.
"""
import os
import shutil

import harness

DATA_DIR = '/tmp/scribbolyth_tests/bookmarked_dirs_data'
shutil.rmtree(DATA_DIR, ignore_errors=True)
os.makedirs(DATA_DIR)
dir_a = os.path.join(DATA_DIR, 'dir_a')
dir_b = os.path.join(DATA_DIR, 'dir_b')
os.makedirs(dir_a)
os.makedirs(dir_b)
init_conf = os.path.join(DATA_DIR, 'init.conf')


def bookmark_lines():
    lines = []
    try:
        with open(init_conf, encoding='utf-8') as f:
            for line in f:
                if line.startswith('bookmark_dir = '):
                    lines.append(line.split('=', 1)[1].strip())
    except FileNotFoundError:
        pass
    return lines


s = harness.launch(workdir=DATA_DIR)
try:
    # Bookmark the directory the browser opens on.
    s.send(b':open .')
    s.send(b'\r')
    s.require(' File Browser ', 'browser should open on cwd')
    s.send(b'm')
    s.require('Bookmarked directory:', 'm should bookmark the current directory')
    if bookmark_lines() != [DATA_DIR]:
        print('FAIL: init.conf bookmarks wrong: %r' % bookmark_lines())
        s.dump()
        raise SystemExit(1)

    # Navigate into dir_a and bookmark it too.
    s.send(b'/')
    s.send(b'dir_a')
    s.send(b'\r')
    s.send(b'l')
    s.require('1/1', 'dir_a should be the current directory')
    s.forbid('dir_b/', 'dir_a should not list its sibling')
    s.send(b'm')
    s.require('Bookmarked directory:', 'm should bookmark dir_a')
    if bookmark_lines() != [DATA_DIR, dir_a]:
        print('FAIL: second bookmark missing/wrong: %r' % bookmark_lines())
        s.dump()
        raise SystemExit(1)

    # '`' shows the list; Esc returns to the normal browser without closing it.
    s.send(b'`')
    s.require('File Browser - Bookmarks', 'backtick should show bookmarks')
    s.require('1/2', 'both bookmarks should be listed')
    s.send(b'\x1b')
    s.require(' File Browser ', 'Esc should return to the normal browser')
    s.forbid('File Browser - Bookmarks', 'Esc should leave bookmark mode')

    # Selecting a bookmark with Enter changes the browser directory.
    s.send(b'h')                        # go back to the workdir
    s.require('dir_b/', 'h should return to the workdir')
    s.send(b'`')
    s.send(b'G')                        # select the last bookmark (dir_a)
    s.send(b'\r')
    s.forbid('File Browser - Bookmarks', 'Enter should leave bookmark mode')
    s.require('1/1', 'Enter should switch to dir_a')
    s.forbid('dir_b/', 'Enter should leave the workdir listing')

    # D removes the selected bookmark (the first entry is the workdir).
    s.send(b'`')
    s.require('1/2')
    s.send(b'D')
    s.require('1/1', 'D should remove one bookmark')
    if bookmark_lines() != [dir_a]:
        print('FAIL: removed bookmark still in init.conf: %r' % bookmark_lines())
        s.dump()
        raise SystemExit(1)
    s.send(b'\x1b')
    s.send(b'\x1b')                    # close the browser
    s.forbid(' File Browser ')
finally:
    s.quit()

# Relaunch: the remaining bookmark is loaded from init.conf.
s = harness.launch(workdir=DATA_DIR)
try:
    s.send(b':open .')
    s.send(b'\r')
    s.require(' File Browser ')
    s.send(b'`')
    s.require('File Browser - Bookmarks')
    s.require('1/1', 'bookmark should survive a relaunch')
    s.require('dir_a', 'the saved bookmark should be listed')
    s.send(b'\x1b')
    s.send(b'\x1b')
finally:
    s.quit()

print('PASS')
