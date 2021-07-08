#!/usr/bin/env python3

# zsim wrapper-driver that supports multi-threaded fast-forward.
# This is accomplished via live-teeing stdout and stderr and looking for magic
# strings that indicate that we should toggle.
#

import argparse
import os
import signal
import subprocess
import sys

ZSIM_ROOT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
ZSIM_PGID = None


argparser = argparse.ArgumentParser(
        description='Wrapper driver for zsim')
argparser.add_argument('-t', '--n_threads', type=int, required=True,
        help='Number of software threads (and hardware cores; 1:1 ratio)')
argparser.add_argument('-c', '--config', type=str, required=True,
        help='zsim .cfg config file')
argparser.add_argument('-s', '--toggle_str', type=str, required=False,
        default='', help='The string to search for to toggle fast-forward')
args = argparser.parse_args(sys.argv[1:])



def set_environment_variables():
    os.environ['OMP_NUM_THREADS'] = str(args.n_threads)

def gen_zsim_cmd():
    zsim_path = os.path.join(ZSIM_ROOT_DIR, 'build/opt/zsim')
    return f'{zsim_path} {args.config}'

def do_toggle(shmid, proc_idx):
    fftoggle_path = os.path.join(ZSIM_ROOT_DIR, 'build/opt/fftoggle')
    cmd = f'{fftoggle_path} ff {shmid} {proc_idx}'
    print(cmd)
    subprocess.Popen(cmd.split()).wait()

def check_line_for_marker(line):
    return args.toggle_str.encode() in line

def find_shmid(line):
    marker = b'Global segment shmid = '
    if marker in line:
        pos = line.index(marker) + len(marker)
        s = line[pos:]
        return int(s)
    else:
        return None

def signal_handler(signum, frame):
    global ZSIM_PGID
    os.killpg(ZSIM_PGID, signum)

def set_signal_forwarding():
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)


def main():
    global ZSIM_PGID

    set_environment_variables()
    set_signal_forwarding()

    is_toggle_mode = args.toggle_str != ''

    # get a pgid with os.setsid
    p = subprocess.Popen(gen_zsim_cmd(), stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, preexec_fn=os.setsid, shell=True)
    ZSIM_PGID = os.getpgid(p.pid)

    shmid = None
    for line in iter(p.stdout.readline, b''):
        sys.stdout.buffer.write(line)
        sys.stdout.flush()

        if is_toggle_mode:
            if shmid == None:
                shmid = find_shmid(line)

            should_toggle = check_line_for_marker(line)
            if (should_toggle):
                assert shmid != -1
                do_toggle(shmid, 0)


if __name__ == '__main__':
    main()
