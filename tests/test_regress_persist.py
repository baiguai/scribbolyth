#!/usr/bin/env python3
"""Regression test: :w warning, :S save, on-disk JSON, :open reload.

Uses an explicit workdir so the saved file survives a relaunch.
"""
import os
import shutil

import harness

DATA_DIR = '/tmp/scribbolyth_tests/persist_data'
shutil.rmtree(DATA_DIR, ignore_errors=True)
os.makedirs(DATA_DIR)

s = harness.launch(workdir=DATA_DIR)
try:
    # blank start, and :w with no stored path warns instead of saving
    s.require('Select a node to edit', 'app must start blank')
    s.send(b':w'); s.send(b'\r')
    s.require('use :saveas', ':w with no path should suggest :S')

    # build a node with text (including chars that stress the JSON escaping)
    s.send(b'a')
    s.require('create_node')
    s.send(b'Read Me'); s.send(b'\r')
    s.require('Read Me')
    s.send(b'I')
    s.send('Hello "world" \\ back'.encode())
    s.send(b'\r')
    s.send('second line é'.encode())
    s.send(b'\x1b')

    # S opens :saveas; check the JSON that lands on disk
    path = os.path.join(DATA_DIR, 'scribbolyth.json')
    s.send(b'S')
    s.require('saveas', 'S should open the :saveas prompt')
    s.send(path)
    s.send(b'\r')
    s.require('Saved', 'status bar should show Saved after :S')
    if not os.path.exists(path):
        print('FAIL: %s was not created' % path)
        s.dump()
        raise SystemExit(1)
    with open(path, encoding='utf-8') as f:
        content = f.read()
    for fragment in ('"version": 1', '"name": "Read Me"',
                     'second line é', '\\"world\\"', '\\\\ back'):
        if fragment not in content:
            print('FAIL: %r missing from saved JSON' % fragment)
            s.dump()
            raise SystemExit(1)

    # quit, relaunch in the same directory, and :open the file back
    s.quit()
    s = harness.launch(workdir=DATA_DIR)
    s.require('Select a node to edit', 'relaunch must start blank')
    s.send(b':open ' + path.encode())
    s.send(b'\r')
    s.require('Read Me', ':open should load the saved tree')
    s.send(b'j')              # select the node so the editor shows its text
    s.require('Hello', 'edited text should survive reload')
    s.require('second line', 'second line should survive reload')
finally:
    s.quit()

print('PASS')
