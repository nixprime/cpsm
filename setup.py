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

from distutils.core import setup, Extension

srcs = [
        "src/ctrlp_util.cc",
        "src/matcher.cc",
        "src/path_util.cc",
        "src/str_util.cc",
        "src/python_extension_main.cc",
]

cpsm = Extension("cpsm", sources=srcs, extra_compile_args=["-std=c++11"],
                 define_macros=[("CPSM_CONFIG_ICU", "1")],
                 libraries=["icudata", "icuuc"])

setup(name="cpsm", version="0.1", description="A path matcher.",
      ext_modules=[cpsm])
