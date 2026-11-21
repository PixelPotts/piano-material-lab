"""Check actual C-renderer parameter sweeps, not mocked UI events."""
import csv
from pathlib import Path

root = Path(__file__).resolve().parent.parent
with (root / 'control-sweep.csv').open() as file:
    rows = list(csv.DictReader(file))
assert len(rows) == 85, 'Expected reference plus 42 minimum/maximum pairs'
reference = rows[0]
assert reference['parameter'] == 'default'
assert abs(float(reference['fundamental']) - 261.625565) < 1e-5
names = set()
for i in range(1, len(rows), 2):
    low, high = rows[i:i+2]
    name = low['parameter']
    assert name == high['parameter'] and name not in names
    names.add(name)
    assert any(row['signature'] != reference['signature'] for row in (low, high)), name
    for row in (low, high):
        assert int(row['nonfinite']) == 0, name
        assert 0 < float(row['peak']) < .99, name
        assert 0 < float(row['rms']) < .99, name
assert len(names) == 42
transition = (root / 'transition-test.txt').read_text()
assert 'nonfinite=0' in transition
print('PASS: all 42 physical controls affect rendered sound; 84 endpoint renders and transitions remain finite.')
