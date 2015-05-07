// cpsm - fuzzy path matcher
// Copyright (C) 2015 Jamie Liu
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <algorithm>
#include <memory>
#include <string>

#include <boost/utility/string_ref.hpp>

#include "ctrlp_util.h"
#include "match.h"
#include "matcher.h"

namespace {

struct PyObjectDeleter {
  void operator()(PyObject* const p) const { Py_DECREF(p); }
};

// Reference-owning, self-releasing PyObject smart pointer.
typedef std::unique_ptr<PyObject, PyObjectDeleter> PyObjectPtr;

}  // namespace

extern "C" {

static PyObject* cpsm_ctrlp_match(PyObject* self, PyObject* args,
                                  PyObject* kwargs) {
  static char const* kwlist[] = {"items",  "query",  "limit", "mmode",
                                 "ispath", "crfile", nullptr};
  PyObject* items_obj;
  char const* query_data;
  Py_ssize_t query_size;
  int limit = -1;
  char const* mmode_data = nullptr;
  Py_ssize_t mmode_size = 0;
  int is_path = 0;
  char const* cur_file_data = nullptr;
  Py_ssize_t cur_file_size = 0;
  if (!PyArg_ParseTupleAndKeywords(
          args, kwargs, "Os#|is#is#", const_cast<char**>(kwlist), &items_obj,
          &query_data, &query_size, &limit, &mmode_data, &mmode_size, &is_path,
          &cur_file_data, &cur_file_size)) {
    return nullptr;
  }

  try {
    cpsm::MatcherOpts mopts;
    mopts.cur_file = std::string(cur_file_data, cur_file_size);
    mopts.is_path = is_path;
    mopts.item_substr_fn = cpsm::match_mode_item_substr_fn(
        boost::string_ref(mmode_data, mmode_size));
    cpsm::Matcher matcher(std::string(query_data, query_size),
                          std::move(mopts));
    std::vector<cpsm::Match> matches;
    PyObjectPtr items_iter(PyObject_GetIter(items_obj));
    if (!items_iter) {
      return nullptr;
    }
    PyObjectPtr item_obj(PyIter_Next(items_iter.get()));
    while (item_obj) {
      char* item_data;
      Py_ssize_t item_size;
      if (PyString_AsStringAndSize(item_obj.get(), &item_data, &item_size) <
          0) {
        return nullptr;
      }
      boost::string_ref item(item_data, item_size);
      matcher.append_match(item, matches);
      item_obj.reset(PyIter_Next(items_iter.get()));
    }
    std::sort(matches.begin(), matches.end());
    if (limit >= 0 && matches.size() > std::size_t(limit)) {
      matches.resize(limit);
    }

    PyObjectPtr matches_list(PyList_New(0));
    if (!matches_list) {
      return nullptr;
    }
    for (auto const& match : matches) {
      PyObjectPtr match_str(
          PyString_FromStringAndSize(match.item().data(), match.item().size()));
      if (!match_str) {
        return nullptr;
      }
      if (PyList_Append(matches_list.get(), match_str.get()) < 0) {
        return nullptr;
      }
    }
    return matches_list.release();
  } catch (std::exception const& ex) {
    PyErr_SetString(PyExc_RuntimeError, ex.what());
    return nullptr;
  }
}

static PyMethodDef cpsm_methods[] = {
    {"ctrlp_match", reinterpret_cast<PyCFunction>(cpsm_ctrlp_match),
     METH_VARARGS | METH_KEYWORDS,
     "Match strings with a CtrlP-compatible interface"},
    {nullptr, nullptr, 0, nullptr}};

PyMODINIT_FUNC initcpsm() { Py_InitModule("cpsm", cpsm_methods); }
}
