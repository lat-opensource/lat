#!/usr/bin/env python3
"""Append one atomic JSON record for a complex-application guest command."""

import argparse
import json
import os


parser = argparse.ArgumentParser()
parser.add_argument("output")
parser.add_argument("--exit-code", type=int, required=True)
parser.add_argument("--stderr", required=True)
parser.add_argument("--environment", default="")
parser.add_argument("command", nargs=argparse.REMAINDER)
args = parser.parse_args()
command = args.command[1:] if args.command[:1] == ["--"] else args.command
stdout = (args.stderr[:-7] + ".stdout"
          if args.stderr.endswith(".stderr") else None)
if stdout and not os.path.exists(stdout):
    stdout = None

record = {
    "command": command,
    "environment": args.environment,
    "exit_code": args.exit_code,
    "new_process_group": True,
    "stderr": args.stderr,
    "stdout": stdout,
}
data = (json.dumps(record, sort_keys=True) + "\n").encode()
fd = os.open(args.output, os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600)
try:
    written = os.write(fd, data)
    if written != len(data):
        raise OSError("short JSONL write")
finally:
    os.close(fd)
