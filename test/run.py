#!/usr/bin/env python3

import argparse
import os
import subprocess

parser = argparse.ArgumentParser(
                    prog='Size Benchmark',
                    description='Benchmarks a DTSS program with variable input sized')
parser.add_argument('filename')

if __name__ == '__main__':
    args = parser.parse_args()

    for size in ['256M', '512M', '1G', '2G', '4G']:
        subprocess.run(['dd', 'if=/dev/urandom', 'of=in.' + size, 'bs=' + size, 'count=1', 'iflag=fullblock'])

        subprocess.run([args.filename, 'in.' + size])

        try:
            os.remove('in.' + size + '.out.0')
            os.remove('in.' + size + '.out.1')
            os.remove('in.' + size + '.out.2')
        except FileNotFoundError:
            continue

