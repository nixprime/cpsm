" cpsm - fuzzy path matcher
" Copyright (C) 2015 the Authors
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

" Global variables and defaults
if !exists('g:cpsm_highlight_mode')
  let g:cpsm_highlight_mode = 'detailed'
endif
if !exists('g:cpsm_match_empty_query')
  let g:cpsm_match_empty_query = 1
endif
if !exists('g:cpsm_max_threads')
  if has('win32unix')
    " Synchronization primitives are extremely slow on Cygwin:
    " https://cygwin.com/ml/cygwin/2012-08/msg00200.html
    let g:cpsm_max_threads = 1
  else
    let g:cpsm_max_threads = 0
  endif
endif
if !exists('g:cpsm_query_inverting_delimiter')
  let g:cpsm_query_inverting_delimiter = ''
endif
if !exists('g:cpsm_unicode')
  let g:cpsm_unicode = 0
endif

let s:script_dir = escape(expand('<sfile>:p:h'), '\')

let s:cpsm_python3 = 0
if has('python3')
  try
    execute 'py3file ' . s:script_dir . '/cpsm.py'
    let s:cpsm_python3 = 1
  catch
    execute 'pyfile ' . s:script_dir . '/cpsm.py'
  endtry
else
  execute 'pyfile ' . s:script_dir . '/cpsm.py'
endif

function cpsm#CtrlPMatch(items, str, limit, mmode, ispath, crfile, regex)
  if !has('python3') && !has('python')
    return ['ERROR: cpsm requires Vim built with Python or Python3 support']
  endif
  if empty(a:str) && g:cpsm_match_empty_query == 0
    let s:results = a:items[0:(a:limit)]
    let s:regexes = []
  else
    let s:match_crfile = exists('g:ctrlp_match_current_file') ? g:ctrlp_match_current_file : 0
    if has('python3') && s:cpsm_python3
      py3 ctrlp_match()
    else
      py ctrlp_match()
    endif
  endif
  call clearmatches()
  " Apply highlight regexes.
  for r in s:regexes
    call matchadd('CtrlPMatch', r)
  endfor
  " CtrlP does this match to hide the leading > in results.
  call matchadd('CtrlPLinePre', '^>')
  return s:results
endfunction
