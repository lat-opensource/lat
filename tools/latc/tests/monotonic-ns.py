#!/usr/bin/env python3
"""Print CLOCK_MONOTONIC in nanoseconds for shell integration tests."""

import time

print(time.monotonic_ns())
