#!/usr/bin/env python3
"""Test Vim-style find: '/' in NORMAL mode, n/N next/prev, ':noh' clear."""
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

    # 4) n / N cycle through the matches with wrap-around
    s.send(b'n')
    s.step(0.2)
    assert 'Match 2 of 3' in s.row_text(s.rows - 1), 'n must move to match 2'
    assert tree_sel(1), 'Beta must be selected after n'
    s.send(b'n')
    s.step(0.2)
    assert 'Match 3 of 3' in s.row_text(s.rows - 1), 'n must move to match 3'
    assert tree_sel(2), 'Gamma must be selected after n'
    s.send(b'n')
    s.step(0.2)
    assert 'Match 1 of 3' in s.row_text(s.rows - 1), 'n must wrap to match 1'
    assert tree_sel(0), 'wrap must select Alpha again'
    s.send(b'N')
    s.step(0.2)
    assert 'Match 3 of 3' in s.row_text(s.rows - 1), 'N must wrap to match 3'
    assert tree_sel(2), 'N wrap must select Gamma'
    print('ok: n/N cycle with wrap-around')

    # 5) :noh hides the highlight but keeps the search (n still works)
    s.send(b':noh')
    s.step(0.2)
    s.send(b'\r')
    s.step(0.3)
    assert 'Search highlight cleared' in s.row_text(s.rows - 1), ':noh status'
    assert not (tree_has_yellow(0) or tree_has_yellow(1) or tree_has_yellow(2)), \
        ':noh must clear the highlight'
    s.send(b'n')
    s.step(0.2)
    assert 'Match 1 of 3' in s.row_text(s.rows - 1), 'n must still work after :noh'
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
    s.send(b'/ALPHA')
    s.step(0.3)
    s.send(b'\r')
    s.step(0.3)
    assert 'Match 1 of 1' in s.row_text(s.rows - 1), 'case-insensitive match'
    print('ok: search is case-insensitive')

    # 9) a match inside the note body is revealed and highlighted in the editor
    s.send(b'a')
    s.send(b'Body')
    s.send(b'\r')
    s.step(0.3)
    s.send(b'i')
    s.step(0.3)
    s.send(b'i')
    s.step(0.3)
    s.send(b'GPS satellite clocks gain about 38 microseconds')
    s.step(0.3)
    s.send(b'\x1b')
    s.step(0.3)
    s.send(b'/gain')
    s.step(0.3)
    s.send(b'\r')
    s.step(0.3)
    assert 'Match 1 of 1' in s.row_text(s.rows - 1), 'body search status'
    editor_row = None
    match_col = None
    for r in range(0, s.rows - 2):
        idx = s.row_text(r).find('gain')
        if idx >= 31:  # inside the editor pane (tree pane is ~30 cols)
            editor_row = r
            match_col = idx
            break
    assert editor_row is not None, 'match must be visible in the editor'
    assert 'inv' in s.grid[editor_row][match_col][1], \
        'editor cursor must sit on the match'
    assert 'bgyellow' in s.grid[editor_row][match_col + 1][1], \
        'the rest of the match must be highlighted in the editor'
    print('ok: note-body matches are revealed and highlighted in the editor')

    # 10) :noh also clears the editor-side highlight
    s.send(b':noh')
    s.step(0.2)
    s.send(b'\r')
    s.step(0.3)
    assert 'Search highlight cleared' in s.row_text(s.rows - 1), ':noh status'
    for c in range(31, s.cols):
        assert 'bgyellow' not in s.grid[editor_row][c][1], \
            ':noh must clear the editor highlight'
    print('ok: :noh clears the editor highlight too')
finally:
    s.quit()

print('PASS')
