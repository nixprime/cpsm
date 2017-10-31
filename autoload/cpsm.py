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

# Importing neovim succeeds as long as the neovim Python module is installed,
# whether or not we are actually running under Neovim, so trying and checking
# for ImportError isn't sufficient.
in_neovim = int(vim.eval("has('nvim')"))
if in_neovim:
    import neovim

def _vim_eval(expr):
    if in_neovim:
        return neovim.Nvim.eval(vim, expr)
    return vim.eval(expr)

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
            items=vim.eval("a:items"), query=vim.eval("a:str"),
            limit=int(vim.eval("a:limit")), mmode=vim.eval("a:mmode"),
            ispath=int(vim.eval("a:ispath")), crfile=vim.eval("a:crfile"),
            highlight_mode=vim.eval("g:cpsm_highlight_mode"),
            match_crfile=int(vim.eval("s:match_crfile")),
            max_threads=int(vim.eval("g:cpsm_max_threads")),
            query_inverting_delimiter=vim.eval("g:cpsm_query_inverting_delimiter"),
            regex_line_prefix=vim.eval("s:regex_line_prefix"),
            unicode=int(vim.eval("g:cpsm_unicode")))
    vim.command("let s:results = [%s]" % ",".join(
            map(_escape_and_quote, results)))
    vim.command("let s:regexes = [%s]" % ",".join(
            map(_escape_and_quote, regexes)))

def _escape_and_quote(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'
