#!/usr/bin/env python3
"""Feature test for the :calc command (expression calculator).

`:calc <expr>` evaluates basic arithmetic -- parentheses, unary signs,
implicit multiplication like `2(1+3)`, decimals, `%` and `^` -- and leaves
the result in the command field, which stays open like a calculator. Errors
close the field and report in the status bar.
"""
import harness

s = harness.launch(cols=80, rows=24)
try:
    s.require('Select a node to edit', 'app must start blank')

    def calc(expr, expect):
        s.send((':calc ' + expr).encode())
        s.send(b'\r')
        buf = s.row_text(s.rows - 2).rstrip()
        if buf != expect:
            print('FAIL: %r -> %r, expected %r' % (expr, buf, expect))
            s.dump()
            raise SystemExit(1)

    # basic arithmetic lands its result back in the command field (kept open)
    calc('2+2', ':4')
    s.require(':4', 'result should be visible in the command field')
    s.send(b'\x1b')                      # Esc closes the field

    # grouped / implicit-multiplication expressions
    calc('2(1+3)/4', ':2')
    s.send(b'\x1b')
    calc('(1+3)(2+1)', ':12')
    s.send(b'\x1b')

    # precedence, unary signs, decimals, exponent and modulo
    calc('1+2*3', ':7')
    s.send(b'\x1b')
    calc('-5+3', ':-2')
    s.send(b'\x1b')
    calc('0.1+0.2', ':0.3')
    s.send(b'\x1b')
    calc('2^10', ':1024')
    s.send(b'\x1b')
    calc('2^3^2', ':512')
    s.send(b'\x1b')
    calc('7%3', ':1')
    s.send(b'\x1b')

    # errors close the field and report in the status bar
    s.send(b':calc 5/0'); s.send(b'\r')
    s.require('Calc error: Division by zero', 'division by zero should error')
    s.forbid(':5', 'the field should close on error')
    s.send(b':calc'); s.send(b'\r')
    s.require('Calc error: Missing expression', 'an empty expression should error')
finally:
    s.quit()

print('PASS')