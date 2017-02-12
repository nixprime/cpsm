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

import os
import sys
import vim

script_dir = vim.eval("s:script_dir")
sys.path.append(script_dir)
import cpsm_py

def _escape_and_quote(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'

def ctrlp_match():
    # TODO: a:regex is unimplemented.
    try:
        results, regexes = cpsm_py.ctrlp_match(
                vim.eval("a:items"), vim.eval("a:str"),
                limit=int(vim.eval("a:limit")), mmode=vim.eval("a:mmode"),
                ispath=int(vim.eval("a:ispath")), crfile=vim.eval("a:crfile"),
                highlight_mode=vim.eval("g:cpsm_highlight_mode"),
                match_crfile=int(vim.eval("s:match_crfile")),
                max_threads=int(vim.eval("g:cpsm_max_threads")),
                query_inverting_delimiter=vim.eval("g:cpsm_query_inverting_delimiter"),
                regex_line_prefix=vim.eval("s:regex_line_prefix"),
                unicode=int(vim.eval("g:cpsm_unicode")))
        # Escape backslashes and ".
        vim.command("let s:results = [%s]" % ",".join(
                map(_escape_and_quote, results)))
        vim.command("let s:regexes = [%s]" % ",".join(
                map(_escape_and_quote, regexes)))
    except Exception as ex:
        ex_str = str(ex)
        if "function takes at most" in ex_str:
            # Most likely due to a new parameter being added to
            # cpsm_py.ctrlp_match.
            ex_str = "rebuild cpsm by running %s: %s" % (
                    os.path.normpath(os.path.join(
                            script_dir, "..", "install.sh")),
                    ex_str)
        vim.command("let s:results = [%s]" % _escape_and_quote(
                "ERROR: " + ex_str))
        vim.command("let s:regexes = []")
