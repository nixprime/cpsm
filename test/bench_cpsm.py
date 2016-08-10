#!/usr/bin/env python

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

import argparse

import bench
import cpsm_py
import linuxclock

if __name__ == "__main__":
    argp = argparse.ArgumentParser()
    argp.add_argument("-c", "--count", nargs="?", type=int, default=1,
                      help="number of matches to show")
    argp.add_argument("-n", "--iterations", nargs="?", type=int,
                      default=bench.DEFAULT_ITERATIONS,
                      help="number of iterations per query")
    argp.add_argument("-t", "--threads", nargs="?", type=int, default=0,
                      help="number of matcher threads")
    args = argp.parse_args()
    for query in bench.QUERIES:
        times = []
        for _ in xrange(args.iterations):
            start = linuxclock.monotonic()
            results, _ = cpsm_py.ctrlp_match(bench.ITEMS, query.query,
                                             limit=bench.LIMIT, ispath=True,
                                             crfile=query.cur_file,
                                             max_threads=args.threads)
            finish = linuxclock.monotonic()
            times.append(finish - start)
        print("%s: avg time %fs, results: %s" % (
                query, sum(times) / len(times), results[:args.count]))
