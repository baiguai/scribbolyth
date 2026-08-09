#!/usr/bin/env python3
"""Test Vim-style find: '/' in NORMAL mode, n/N step the cursor through the
matches inside the current node's text, ':noh' clears the highlight."""
import harness

s = harness.launch(cols=80, rows=24)
try:
    def joined():
        return "\n".join(s.screen())

    def tree_has_yellow(row):
        return any('bgyellow' in st for c in range(0, 30)
                   for st in s.grid[row][c][1])

    def tree_sel(row):
        return 'inv' in s.grid[row][2][1]

    def editor_cursor():
        """(row, col) of the editor's inverted cursor char in the pane."""
        for r in range(0, s.rows - 2):
            for c in range(31, s.cols):  # editor pane sits right of the tree
                if 'inv' in s.grid[r][c][1]:
                    return (r, c)
        return None

    # three root nodes: Alpha, Beta, Gamma
    s.send(b'a')
    s.send(b'Alpha')
    s.send(b'\r')
    s.step(0.3)
    s.send(b'a')
    s.send(b'Beta')
    s.send(b'\r')
    s.step(0.3)
    s.send(b'a')
    s.send(b'Gamma')
    s.send(b'\r')
    s.step(0.3)
    s.require('Alpha', 'tree node created')
    s.require('Gamma', 'tree node created')

    # 1) '/' in NORMAL opens the command field prefilled with '/'
    s.send(b'i')
    s.step(0.3)
    assert 'NORMAL' in s.row_text(s.rows - 1), 'mode must be NORMAL'
    s.send(b'/')
    s.step(0.3)
    assert 'COMMAND' in s.row_text(s.rows - 1), 'mode must be COMMAND'
    assert s.row_text(22).startswith('/'), 'command field must show / prompt'
    print('ok: / opens the command field with a / prompt')

    # 2) typing a query shows it in the field; Enter runs the find
    s.send(b'a')
    s.step(0.3)
    assert s.row_text(22).startswith('/a'), 'command field must show /a'
    s.send(b'\r')
    s.step(0.3)
    assert 'NORMAL' in s.row_text(s.rows - 1), 'mode must return to NORMAL'
    assert 'Match 1 of 3' in s.row_text(s.rows - 1), 'status must show match 1 of 3'
    print('ok: /a Enter finds 3 matches (Alpha, Beta, Gamma)')

    # 3) every matching node is highlighted yellow; the first is selected
    assert tree_has_yellow(0) and tree_has_yellow(1) and tree_has_yellow(2), \
        'all matching nodes must be highlighted'
    assert tree_sel(0), 'first match (Alpha) must be selected'
    print('ok: matches are highlighted and the first is selected')

    # 4) n/N step the cursor through the matches inside the note text
    s.send(b'a')
    s.send(b'Body')
    s.send(b'\r')
    s.step(0.3)
    s.send(b'i')
    s.step(0.3)
    s.send(b'i')
    s.step(0.3)
    s.send(b'gain first gain second gain third')
    s.step(0.3)
    s.send(b'\x1b')
    s.step(0.3)
    s.send(b'/gain')
    s.step(0.3)
    s.send(b'\r')
    s.step(0.3)
    assert 'Match 1 of 1' in s.row_text(s.rows - 1), 'body search status'
    cur = editor_cursor()
    assert cur is not None, 'cursor must land on the first match'
    first_col = cur[1]
    assert s.row_text(cur[0])[first_col:first_col + 4] == 'gain', \
        'cursor must sit on the first occurrence'
    s.send(b'n')
    s.step(0.2)
    cur = editor_cursor()
    assert cur is not None and cur[1] > first_col, 'n must move to the next match'
    assert s.row_text(cur[0])[cur[1]:cur[1] + 4] == 'gain', 'next match is gain'
    second_col = cur[1]
    s.send(b'n')
    s.step(0.2)
    cur = editor_cursor()
    assert cur is not None and cur[1] > second_col, 'n must move to the third match'
    last_col = cur[1]
    s.send(b'n')
    s.step(0.2)
    cur = editor_cursor()
    assert cur is not None and cur[1] == first_col, 'n must wrap to the first match'
    s.send(b'N')
    s.step(0.2)
    cur = editor_cursor()
    assert cur is not None and cur[1] == last_col, 'N must wrap to the last match'
    s.send(b'N')
    s.step(0.2)
    cur = editor_cursor()
    assert cur is not None and cur[1] == second_col, 'N must move to the previous match'
    print('ok: n/N step through the matches inside the note text with wrap-around')

    # 5) :noh hides the highlight but keeps the search (n still works)
    s.send(b':noh')
    s.step(0.2)
    s.send(b'\r')
    s.step(0.3)
    assert 'Search highlight cleared' in s.row_text(s.rows - 1), ':noh status'
    for r in range(0, s.rows - 2):
        for c in range(0, s.cols):
            assert 'bgyellow' not in s.grid[r][c][1], \
                ':noh must clear every highlight'
    before = editor_cursor()
    s.send(b'n')
    s.step(0.2)
    after = editor_cursor()
    assert after is not None and after != before, 'n must still step after :noh'
    print('ok: :noh hides highlights, n still navigates')

    # 6) no matches reports the pattern
    s.send(b'/zzz')
    s.step(0.3)
    s.send(b'\r')
    s.step(0.3)
    assert 'Pattern not found: zzz' in s.row_text(s.rows - 1), 'no-match status'
    print('ok: unmatched pattern is reported')

    # 7) an empty / re-runs the previous search
    s.send(b'/')
    s.step(0.3)
    s.send(b'\r')
    s.step(0.3)
    assert 'Pattern not found: zzz' in s.row_text(s.rows - 1), 'empty / reruns search'
    print('ok: empty / re-runs the previous search')

    # 8) the match is case-insensitive
    s.send(b'/GAIN')
    s.step(0.3)
    s.send(b'\r')
    s.step(0.3)
    assert 'Match 1 of 1' in s.row_text(s.rows - 1), 'case-insensitive match'
    cur = editor_cursor()
    assert cur is not None and s.row_text(cur[0])[cur[1]:cur[1] + 4] == 'gain', \
        'cursor must land on the (case-insensitive) match'
    print('ok: search is case-insensitive')
finally:
    s.quit()

print('PASS')
