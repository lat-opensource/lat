#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 LAT Project Authors
# SPDX-License-Identifier: GPL-2.0-only
"""Reject unsupported-register diagnostics as well as wrong AVX results."""
from pathlib import Path
import subprocess
import sys

runner = Path(__file__).with_name("test-avx-regressions.py")
result = subprocess.run([sys.executable, str(runner), *sys.argv[1:]],
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                        text=True)
print(result.stdout, end="")
if "reg no match" in result.stdout:
    print("FAIL: conversion used a register check that does not support its operands")
    sys.exit(1)
sys.exit(result.returncode)
