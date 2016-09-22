cpsm
====

[![Build Status](https://travis-ci.org/nixprime/cpsm.svg?branch=master)](https://travis-ci.org/nixprime/cpsm)

cpsm is a matcher for [CtrlP][]. Although it supports all kinds of queries, it
is highly optimized for file paths (and, to a lesser extent, similar strings
like identifiers in source code).

Motivation
----------

There are a lot of different way to manage multiple files in Vim. The goal of
cpsm is to enable a particular one based on CtrlP:

1. Completely forget about the current set of open buffers.

2. When you want to open a file, invoke CtrlP and type - at most - a handful of
   immediately obvious letters in the file's name or path, like the beginning
   of its filename.

3. Get immediate visual feedback from CtrlP as to whether or not it has
   correctly determined what file you want.

4. Hit Enter to open the file you wanted in the current window.

To achieve this, cpsm needs to deliver:

- high quality search results (at sufficiently high levels of quality, it's
  possible to enter a short query, hit Enter without needing to look at and
  mentally parse the top match, and have a reasonable amount of confidence that
  CtrlP/cpsm got your file right anyway)

- with as little user input as possible (every keystroke matters because of how
  common switching between files is)

- with as little latency as possible (to support scaling to very large, and
  especially very deeply nested, code bases with very long pathnames)

See the "Performance" section below for both search quality and time
comparisons to other matchers.

Requirements
------------

- Vim 7.4, compiled with the `+python` flag.

- A C++ compiler supporting C++11.

- Boost (Ubuntu: package `libboost-all-dev`).

- CMake (Ubuntu: package `cmake`).

- Python headers (Ubuntu: package `python-dev`).

- Optional, required for Unicode support: ICU (Ubuntu: package `libicu-dev`).

Pyenv users on Mac OS X: cpsm requires dynamic Python libraries, which pyenv
does not build by default. If CMake indicates that it's using static libraries,
e.g.:

```
-- Found PythonLibs: /Users/<username>/.pyenv/versions/3.5.2/lib/python3.5/config-3.5m/libpython3.5m.a
```

Rebuild with dynamic library support by running `env
PYTHON_CONFIGURE_OPTS="--enable-framework" pyenv install <version>`.

Python 3 Support
----------------

If your Vim is compiled against Python 3 instead (`+python3` flag) the install
script should detect this in most cases and everything should just work. You may
need different python headers installed (e.g. `python3-dev` on Ubuntu).

If the detection does not work for any reason you can set `PY3=ON` or `PY3=OFF`
as appropriate when running `./install.sh` to override it.

Installation
------------

1. Install cpsm using your favorite Vim package manager. For example, with
   [Vundle](http://github.com/gmarik/Vundle.vim), this consists of adding:

        Vundle 'nixprime/cpsm'

   to your `vimrc` and then running `:PluginInstall` from Vim.

2. Build the Python module. On Linux, `cd` into `~/.vim/bundle/cpsm` and run
   `./install.sh`. Otherwise, peek inside `install.sh` and see what it does.

3. Add:

        let g:ctrlp_match_func = {'match': 'cpsm#CtrlPMatch'}

   to your `vimrc`.

Options
-------

All of the following options are set by adding

    let (option name) = (option value)

to your .vimrc.

- As shown below, cpsm will still attempt to perform matching on an empty query
  based on the open file in the current buffer. This interacts badly with e.g.
  CtrlPMRU. To disable all matching on empty queries, set
  `g:cpsm_match_empty_query` to 0. If you want empty query matching to only be
  disabled for MRU mode, it's recommended that you configure this in your
  bindings, e.g.:

        nnoremap <silent> <C-o> :let g:cpsm_match_empty_query = 0<CR>:CtrlPMRU<CR>
        nnoremap <silent> <C-p> :let g:cpsm_match_empty_query = 1<CR>:CtrlP<CR>

  Note that if you do the above, you may need to prevent CtrlP from overriding
  your binding by setting `let g:ctrlp_map = ''`.

- `g:cpsm_highlight_mode` controls how matches are highlighted. Valid highlight
  modes are:

  - "none": Do not highlight any match characters.

  - "basic": Highlight the entire region between the leftmost and rightmost
    matched characters.

  - "detailed": Highlight each matched character.

  The default is "detailed". The highlight group used to highlight matched
  characters is "CtrlPMatch" (the same as for CtrlP's default matcher).

- By default, cpsm will automatically detect the number of matcher threads
  based on the available hardware concurrency. To limit the number of threads
  that cpsm can use, set `g:cpsm_max_threads`.

- When `g:cpsm_query_inverting_delimiter` is set to a single character, it can
  be used in a query to move the part of the query after it to the part of the
  query before it. For example, if `g:cpsm_query_inverting_delimiter` is a
  space, then:

  - A query of "foo" is matched normally.

  - A query of "foo bar" is matched as if it were "barfoo".

  - A query of "foo bar qux" is matched as if it were "quxbarfoo".

  If `g:cpsm_query_inverting_delimiter` is unset or empty, this feature is
  disabled.

- To enable Unicode support, set `g:cpsm_unicode` to 1. Unicode support is
  currently very limited, and consists mostly of parsing input strings as UTF-8
  and handling the case of non-ASCII letters correctly.

In addition, cpsm respects the value set for `g:ctrlp_match_current_file`.

Performance
-----------

- The matchers in this comparison:

  - cpsm: cpsm in its default configuration, as accessed through the
    cpsm_py Python extension (the same way the Vim plugin works)

  - ctrlp-cmatcher: https://github.com/JazzCore/ctrlp-cmatcher/

  - ctrlp-py-matcher: https://github.com/FelikZ/ctrlp-py-matcher

  - ctrlp: the default CtrlP matcher

  - fzf: https://github.com/junegunn/fzf

- All data is measured on Ubuntu 14.04, running in a VirtualBox VM in a Windows
  7 host, on an Intel i5-4670K, with all 4 CPUs visible to the VM. Both the
  host and the guest are relatively quiescent while benchmarking.

- The search corpus consists of the 48728 files in a clean Linux kernel source
  repository checked out at the v4.0 tag, as collected by running `ag "" -i
  --nocolor --nogroup --hidden --ignore .git -g ""`.

- For all CtrlP-based matchers, the match mode is "full-line" (the default) and
  the limit is 10 (also the default). ctrlp-cmatcher only uses the current
  filename to remove it from the list of candidate items; ctrlp-py-matcher
  doesn't use it at all; there doesn't seem to be a way to pass this
  information to fzf.

- All times are averages over 100 runs. No timing information is available for
  the default CtrlP matcher or fzf because I can't figure out how to run either
  in a single-shot standalone configuration. (A quick search finds claims that
  ctrlp-cmatcher and ctrlp-py-matcher are both about an order of magnitude
  faster than the default matcher. YMMV.) cpsm times include both the default
  configuration (automatic selection of number of matcher threads) and with
  `max_threads` set to 1.

- Results (given as the best match and the average time to return matches):

  - Query "", current file "":

    - cpsm: "Kbuild"; 2.863ms (9.946ms with 1 thread)

    - ctrlp: "security/keys/encrypted-keys/Makefile"

    - fzf: "COPYING"

    - All others: "security/capability.c" in roughly zero time

    - Only cpsm and fzf do any ranking; cpsm is falling back on the shortest
      filename in the closest directory to the current file (which is the
      repository's root), while fzf picks the lexicographically lowest filename
      in the root directory.

    - I think the default CtrlP matcher is returning a different result simply
      because it gets filenames in a slightly different order from ag (results
      for the default matcher are collected by actually running Vim, while the
      others use a precomputed list of items; "security/capability.c" is the
      first file ag returned in the precomputed list.)

  - Query "", current file "mm/memcontrol.c":

    - cpsm: "include/linux/memcontrol.h"; 2.953ms (10.490ms with 1 thread)

    - All others: same as above

    - "memcontrol" is a sufficiently unique prefix that cpsm returns (IMO) the
      best possible default result, "mm/memcontrol.c"'s corresponding header
      file, with no query entered whatsoever and with no special knowledge of
      the kernel's source layout.

    - It looks like the default CtrlP matcher doesn't use information about the
      currently open file either.

  - Query "", current file "kernel/signal.c":

    - cpsm: "include/linux/signal.h"; 2.838ms (10.242ms with 1 thread)

    - All others: same as above

    - "signal" is a significantly more common prefix, but cpsm gets probably
      the best match (although this is largely luck; "include/linux" is shorter
      than the other likeliest possibility, "include/asm-generic", and all else
      being equal cpsm prefers shorter matches on the grounds that it's easier
      to add more characters to select a longer match.)

  - Query "x86/", current file "kernel/signal.c":

    - cpsm: "arch/x86/um/signal.c"; 1.932ms (6.693ms with 1 thread)

    - ctrlp-cmatcher: "arch/x86/Kbuild"; 25.034ms

    - ctrlp-py-matcher: "arch/x86/Kbuild"; 27.298ms

    - ctrlp: "tools/perf/arch/x86/util/tsc.h"

    - fzf: "Documentation/x86/early-microcode.txt"

    - Without using the current filename, there is nothing the other matchers
      can do to disambiguate the query.

  - The next set of cases simulate a user typing progressively more letters in
    a desired file's name ("include/linux/rcupdate.h"), when they happen to be
    in a different unrelated file.

  - Query "r", current file "kernel/signal.c":

    - cpsm: "kernel/range.c"; 3.980ms (14.070ms with 1 thread)

    - ctrlp-cmatcher: "README"; 19.825ms

    - ctrlp-py-matcher: "README"; 34.215ms

    - ctrlp: "security/keys/encrypted-keys/Makefile"

    - fzf: "CREDITS"

    - cpsm is much faster than either of the other two benchmarkable matchers
      with multithreading enabled, and competitive with ctrlp-cmatcher when
      locked to a single thread.

  - Query "rc", current file "kernel/signal.c":

    - cpsm: "kernel/rcu/rcu.h"; 3.891ms (13.827ms with 1 thread)

    - ctrlp-cmatcher: "arch/Kconfig"; 24.391ms

    - ctrlp-py-matcher: "fs/dlm/rcom.h"; 39.328ms

    - ctrlp: "security/capability.c"

    - fzf: "Documentation/circular-buffers.txt"

  - Query "rcu", current file "kernel/signal.c":

    - cpsm: "kernel/rcu/rcu.h"; 2.534ms (9.009ms with 1 thread)

    - ctrlp-cmatcher: "arch/um/Makefile"; 29.619ms

    - ctrlp-py-matcher: "kernel/rcu/rcu.h"; 37.312ms

    - ctrlp: "security/security.c"

    - fzf: "Documentation/circular-buffers.txt"

  - Query "rcup", current file "kernel/signal.c":

    - cpsm: "include/linux/rcupdate.h"; 2.210ms (7.810ms with 1 thread)

    - ctrlp-cmatcher: "kernel/rcu/update.c"; 31.301ms

    - ctrlp-py-matcher: "include/linux/rcupdate.h"; 37.560ms

    - ctrlp: "security/apparmor/include/path.h"

    - fzf: "Documentation/power/suspend-and-cpuhotplug.txt"

  - Skipping the rest of the letter-by-letter results, since cpsm and
    ctrlp-py-matcher have already "won":

    - ctrlp-cmatcher stays with "kernel/rcu/update.c" as its best match until
      the entire string "rcupdate.h" is used as the query.

    - ctrlp continues to return completely unrelated results for all of the top
      10 until the query "rcupdate", when it suddenly gets the correct best
      match.

    - fzf switches to the correct best match after one more letter (query
      "rcupd").

License
-------

This software is licensed under the [Apache License, Version 2.0][LICENSE].

[CtrlP]: http://github.com/kien/ctrlp.vim
[LICENSE]: http://www.apache.org/licenses/LICENSE-2.0
