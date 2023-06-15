from subprocess import Popen, PIPE, STDOUT, run
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
    line = ''

    if len(files) == 1:
        # Single file: get test result from comment.
        with open(files[0]) as f:
            line =  f.readline()
    else:
        # Multi-file: get test result from restult file.
        idx = [i for i, e in enumerate(files) if os.path.basename(e) == 'test_result.txt']
        if len(idx) < 1:
            print(f'You must place a `test_result.txt` file in each test directory.')
        elif len(idx) > 1:
            print(f'You cannot place more than one `test_result.txt` file in a test directory.')
        else:
            with open(files[idx[0]]) as f:
                line = f.readline()
                files.pop(idx[0])  # `test_result.txt` shouldn't be executed.

    test_out = ''
    test_in = ''
    is_io = False

    match = re.search('(?:// )?Test result: (.*)\n', line)
    if match:
        test_out = match.group(1)
        # Check if the test result has the IO format.
        io_match = re.search('io -> (.*):(.*)', test_out)
        if io_match:
            is_io = True
            test_in = io_match.group(1)
            test_out = io_match.group(2)
    else:
        print(f'Invalid test file {files[0]}')
        exit(1)

    stdout = ''
    stderr = ''

    if is_io == True:
        p = Popen([COMMAND] + files, stdout=PIPE, stdin=PIPE, stderr=PIPE)
        data = p.communicate(input=(test_in + '\n').encode('UTF-8'))
        stdout = data[0].decode('UTF-8').strip('\n')
        stderr = data[1].decode('UTF-8').strip('\n')
    else:
        res = run([COMMAND] + files, capture_output=True, text=True)
        stdout = res.stdout.strip('\n')
        stderr = res.stderr.strip('\n')

    if stdout == test_out:
        print(f'\033[32mOk\033[m   {files}')
    else:
        print(f'\033[31mErr\033[m  {files}, {stdout} != {test_out}')
        print(f'    Stderr `{stderr}`')
        print(f'    Stdout `{stdout}`')

with os.scandir(BASE_PATH) as entries:
    for entry in entries:
        if entry.is_file():
            run_test([entry.path])
        else:
            with os.scandir(entry.path) as sub:
                files = [s.path for s in sub]
                run_test(files)
