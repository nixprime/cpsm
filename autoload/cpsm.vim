" cpsm - fuzzy path matcher
" Copyright (C) 2015 Jamie Liu
"
" Licensed under the Apache License, Version 2.0 (the "License");
" you may not use this file except in compliance with the License.
" You may obtain a copy of the License at
"
"     http://www.apache.org/licenses/LICENSE-2.0
"
" Unless required by applicable law or agreed to in writing, software
" distributed under the License is distributed on an "AS IS" BASIS,
" WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
" See the License for the specific language governing permissions and
" limitations under the License.

let s:script_dir = escape(expand('<sfile>:p:h'), '\')

execute 'pyfile ' . s:script_dir . '/cpsm.py'

function cpsm#CtrlPMatch(items, str, limit, mmode, ispath, crfile, regex)
  py ctrlp_match()
  return s:results
endfunction

" Default settings
let g:cpsm_max_threads = 0
let g:cpsm_unicode = 0
