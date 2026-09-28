"""A failing check inside assert must execute even in Release."""
import subprocess
import sys

result = subprocess.run([sys.argv[1]], capture_output=True, text=True, timeout=10)
if result.returncode != 23 or result.stdout.strip() != 'assert-expression-evaluated':
    raise SystemExit(f'Assertion witness did not execute its failure: {result}')
print('Assertion failure witness PASS')
