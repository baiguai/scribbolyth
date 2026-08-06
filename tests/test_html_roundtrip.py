#!/usr/bin/env python3
"""Regression test: history round-trips through the HTML export/import.

`:X` writes the viewed-node history into the exported HTML's `historyStack`;
`:U` reads it back, so history persists per document.
"""
import os
import shutil

import harness

DATA_DIR = '/tmp/scribbolyth_tests/html_roundtrip_data'
HTML = os.path.join(DATA_DIR, 'roundtrip.html')
shutil.rmtree(DATA_DIR, ignore_errors=True)
os.makedirs(DATA_DIR)

s = harness.launch(workdir=DATA_DIR)
try:
    s.require('Select a node to edit', 'app must start blank')

    s.send(b'a')
    s.require('create_node')
    s.send(b'Alpha'); s.send(b'\r')
    s.require('Alpha')
    s.send(b'I')
    s.send(b'Hello world')
    s.send(b'\x1b')                       # recording the node in history

    # export the document (tree + history) to an HTML file
    s.send(b':X ' + HTML.encode())
    s.send(b'\r')
    s.require('Exported', 'status bar should report the export')
    with open(HTML, encoding='utf-8') as f:
        html = f.read()
    if 'let historyStack' not in html:
        print('FAIL: exported HTML has no historyStack')
        s.dump()
        raise SystemExit(1)
    if '"title": "Alpha"' not in html:
        print('FAIL: exported HTML history lacks the node title')
        s.dump()
        raise SystemExit(1)

    # new document clears history; importing the HTML brings it back
    s.send(b':n'); s.send(b'\r')
    s.require('New document', 'new document should reset the workspace')
    s.send(b':U ' + HTML.encode())
    s.send(b'\r')
    s.require('Imported', 'status bar should report the import')
    s.send(b'<')
    s.require(' < History ', 'history dialog should open')
    s.require('1/1', 'imported HTML should carry its history (1 entry)')
    s.send(b'\x1b')

finally:
    s.quit()

print('PASS')
