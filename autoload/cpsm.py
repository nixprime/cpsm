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
import traceback
import vim

try:
    _vim_eval = vim.api.eval
except AttributeError:
    # vim.api is a neovim feature.
    _vim_eval = vim.eval

script_dir = _vim_eval("s:script_dir")
sys.path.append(script_dir)
import cpsm_py

def ctrlp_match_with(**kwargs):
    """
    Wrapper for cpsm_py.ctrlp_match() that converts Vim numbers from strings
    back to numbers, and handles exceptions.
    """
    try:
        for key in ("limit", "ispath", "match_crfile", "max_threads",
                    "unicode"):
            kwargs[key] = int(kwargs[key])
        return cpsm_py.ctrlp_match(**kwargs)
    except Exception as ex:
        # Log the exception. Unfortunately something CtrlP causes all messages
        # to be discarded, so this is only visible in Vim verbose logging.
        print("cpsm error:")
        traceback.print_exc(file=sys.stdout)
        # Return a short error message in the results.
        ex_str = str(ex)
        if (sys.exc_info()[0] is TypeError and
            "function takes at most" in ex_str):
            # Most likely due to a new parameter being added to
            # cpsm_py.ctrlp_match.
            ex_str = "rebuild cpsm by running %s: %s" % (
                    os.path.normpath(os.path.join(
                            script_dir, "..", "install.sh")),
                    ex_str)
        return ["ERROR:" + ex_str], []

def _ctrlp_match_evalinput():
    return ctrlp_match_with(**_vim_eval("s:input"))

def ctrlp_match():
    """
    Deprecated interface that gets arguments by calling vim.eval() and returns
    outputs by calling vim.command(). Kept for Denite. Use ctrlp_match_with()
    or cpsm_py.ctrlp_match() in new code.
    """
    # TODO: a:regex is unimplemented.
    results, regexes = ctrlp_match_with(
            items=_vim_eval("a:items"), query=_vim_eval("a:str"),
            limit=int(_vim_eval("a:limit")), mmode=_vim_eval("a:mmode"),
            ispath=int(_vim_eval("a:ispath")), crfile=_vim_eval("a:crfile"),
            highlight_mode=_vim_eval("g:cpsm_highlight_mode"),
            match_crfile=int(_vim_eval("s:match_crfile")),
            max_threads=int(_vim_eval("g:cpsm_max_threads")),
            query_inverting_delimiter=_vim_eval("g:cpsm_query_inverting_delimiter"),
            regex_line_prefix=_vim_eval("s:regex_line_prefix"),
            unicode=int(_vim_eval("g:cpsm_unicode")))
    vim.command("let s:results = [%s]" % ",".join(
            map(_escape_and_quote, results)))
    vim.command("let s:regexes = [%s]" % ",".join(
            map(_escape_and_quote, regexes)))

def _escape_and_quote(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'
