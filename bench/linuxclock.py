# cpsm - fuzzy path matcher
# Copyright (C) 2015 the Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import print_function

import ctypes
import os

# From <linux/time.h>
CLOCK_REALTIME = 0
CLOCK_MONOTONIC = 1
CLOCK_PROCESS_CPUTIME_ID = 2
CLOCK_THREAD_CPUTIME_ID = 3
CLOCK_MONOTONIC_RAW = 4
CLOCK_REALTIME_COARSE = 5
CLOCK_MONOTONIC_COARSE = 6
CLOCK_BOOTTIME = 7
CLOCK_REALTIME_ALARM = 8
CLOCK_BOOTTIME_ALARM = 9
CLOCK_SGI_CYCLE = 10
CLOCK_TAI = 11

class Timespec(ctypes.Structure):
    _fields_ = [
        ('tv_sec', ctypes.c_long),
        ('tv_nsec', ctypes.c_long),
    ]

    def to_seconds(self):
        return self.tv_sec + (self.tv_nsec * 1e-9)

_clock_gettime = ctypes.CDLL("librt.so.1", use_errno=True).clock_gettime
_clock_gettime.argtypes = [ctypes.c_int, ctypes.POINTER(Timespec)]

def gettime(clock):
    """Returns the current time on the given clock as a Timespec."""
    t = Timespec()
    if _clock_gettime(clock, ctypes.pointer(t)) != 0:
        errno = ctypes.get_errno()
        raise OSError(errno, os.strerror(errno))
    return t

def monotonic():
    """Returns the value (in fractional seconds) of a monotonic clock."""
    return gettime(CLOCK_MONOTONIC_RAW).to_seconds()
