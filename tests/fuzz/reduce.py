#!/usr/bin/env python3
"""reduce.py IN.c OUT.c [nmcc flags]: shrink a program from gen.py while
one.sh goes on failing in the way it does for IN.c.  Statements are blanked
to ';' in halving chunks, then whole if, for and switch blocks are removed,
then what single statements, case labels and breaks can go.  The lines of
gen.py's programs are each a whole statement, which is what makes this
simple; it is no use on other C."""
import os, subprocess, sys

here = os.path.dirname(os.path.abspath(sys.argv[0]))
src, out, flags = sys.argv[1], sys.argv[2], sys.argv[3:]
tmp = '%s.%d.c' % (out, os.getpid())
KEEP = ('unsigned x', 'unsigned acc', 'return', 'print_hex')
LABELS = ('case', 'default', 'break')

def result(ls):
    with open(tmp, 'w') as f: f.write('\n'.join(ls))
    return subprocess.run([here + '/one.sh', tmp] + flags, capture_output=True).returncode

lines = open(src).read().split('\n')
want = result(lines)
if want not in (1, 3): sys.exit('%s: one.sh does not fail on it' % src)
def fails(ls): return result(ls) == want

def statement(l):
    t = l.strip()
    return (l.startswith('    ') and t not in (';', '{', '}', '')
            and not t.startswith(KEEP + LABELS)
            and t.count('{') == t.count('}') and t.endswith((';', '}')))

def blocks(ls):
    out = []
    for i, l in enumerate(ls):
        t = l.strip()
        if l.startswith('    ') and t.endswith('{') and t.startswith(('if (', 'for (', 'switch (')):
            ind = len(l) - len(l.lstrip())
            j = i + 1
            while not (ls[j].strip() == '}' and len(ls[j]) - len(ls[j].lstrip()) == ind): j += 1
            out.append((i, j))
    return out

# statements, in chunks of half of them, then a quarter, and so on
changed = True
while changed:
    changed = False
    cand = [i for i, l in enumerate(lines) if statement(l)]
    n = max(len(cand) // 2, 1)
    while n >= 1:
        for i in range(0, len(cand), n):
            trial = list(lines)
            for c in cand[i:i + n]: trial[c] = ';'
            if trial != lines and fails(trial): lines, changed = trial, True
        cand = [i for i, l in enumerate(lines) if statement(l)]
        n //= 2

# whole blocks, the largest first
changed = True
while changed:
    changed = False
    for i, j in sorted(blocks(lines), key=lambda b: b[0] - b[1]):
        trial = lines[:i] + ['    ;'] + lines[j + 1:]
        if fails(trial):
            lines, changed = trial, True
            break

# single statements once more, then the labels
for i in range(len(lines)):
    t = lines[i].strip()
    if statement(lines[i]) or t.startswith(LABELS):
        trial = list(lines)
        trial[i] = ';' if statement(lines[i]) else ''
        if fails(trial): lines = trial

lines = [l for l in lines if l != '']
lines = [l for i, l in enumerate(lines) if not (l.strip() == ';' and i and lines[i - 1].strip() == ';')]
if not fails(lines): sys.exit('reduce.py: lost the failure while tidying')
with open(out, 'w') as f: f.write('\n'.join(lines) + '\n')
os.remove(tmp)
print('%s: %d lines' % (out, len(lines)))
