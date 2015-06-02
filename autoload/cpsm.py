# cpsm - fuzzy path matcher
# Copyright (C) 2015 Jamie Liu
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

import os.path
import sys
import vim

script_dir = vim.eval("s:script_dir")
python_dir = os.path.join(script_dir, "..", "python")
sys.path.append(python_dir)
import cpsm

def ctrlp_match():
    # TODO: a:regex is unimplemented.
    results = cpsm.ctrlp_match(vim.eval("a:items"), vim.eval("a:str"),
                               limit=int(vim.eval("a:limit")),
                               mmode=vim.eval("a:mmode"),
                               ispath=int(vim.eval("a:ispath")),
                               crfile=vim.eval("a:crfile"),
                               max_threads=int(vim.eval("g:cpsm_max_threads")),
                               unicode=int(vim.eval("g:cpsm_unicode")))
    # Escape backslashes and ".
    vim.command("let s:results = [%s]" % ",".join(
            '"%s"' % r.replace("\\", "\\\\").replace('"', '\\"')
            for r in results))
