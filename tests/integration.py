import subprocess
import sys
import os
import re

# A single file in `examples/` is executed as a
# single program. Any directories in `examples/`
# are viewed as a larger program and executed in
# that way.

if len(sys.argv) <= 1:
    print('Usage: python3 integration.py EXEC_NAME')
    exit(1)

COMMAND = sys.argv[1]
BASE_PATH = 'examples/'

def run_test(files):
    test_res = ''
    if len(files) == 1:
        with open(files[0]) as f:
            match = re.search('// Test result: (.*)\n', f.readline())
            if match:
                test_res = match.group(1)
            else:
                print(f'Invalid test file {files[0]}')
                exit(1)
    else:
        idx = [i for i, e in enumerate(files) if os.path.basename(e) == 'test_result.txt']
        if len(idx) < 1:
            print(f'You must place a `test_result.txt` file in each test directory.')
        elif len(idx) > 1:
            print(f'You cannot place more than one `test_result.txt` file in a test directory.')
        else:
            with open(files[idx[0]]) as f:
                match = re.search('Test result: (.*)\n', f.readline())
                if match:
                    test_res = match.group(1)
                else:
                    print(f'Invalid test directory file {files[idx[0]]}')
                    exit(1)
                files.pop(idx[0])

    if test_res == 'skip':
        print(f'Skip {files}')
    else:
        res = subprocess.run([COMMAND] + files, capture_output=True, text=True)
        stdout = res.stdout.strip('\n')
        if stdout == test_res:
            print(f'Ok   {files}')
        else:
            print(f'Err  {files}, {stdout} != {test_res}')
            print(f'    Stderr `{res.stderr.strip()}`')
            print(f'    Stdout `{res.stdout.strip()}`')

with os.scandir(BASE_PATH) as entries:
    for entry in entries:
        if entry.is_file():
            run_test([entry.path])
        else:
            with os.scandir(entry.path) as sub:
                files = [s.path for s in sub]
                run_test(files)
