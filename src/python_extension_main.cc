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

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <boost/utility/string_ref.hpp>

#include "ctrlp_util.h"
#include "cpsm.h"

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
    std::string query(query_data, query_size);

    auto item_substr_fn = cpsm::match_mode_item_substr_fn(
        boost::string_ref(mmode_data, mmode_size));
    auto item_str_fn =
        [&item_substr_fn](std::pair<boost::string_ref, PyObjectPtr> const& p)
            -> boost::string_ref {
              boost::string_ref str = p.first;
              if (item_substr_fn) {
                return item_substr_fn(str);
              }
              return str;
            };

    // Read items sequentially, since the Python API is probably not
    // thread-safe.
    std::vector<std::pair<boost::string_ref, PyObjectPtr>> items;
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
      items.emplace_back(boost::string_ref(item_data, item_size),
                         std::move(item_obj));
      item_obj.reset(PyIter_Next(items_iter.get()));
    }

    // Do matching.
    std::vector<std::pair<boost::string_ref, PyObjectPtr>*> matches =
        cpsm::match(query, items, item_str_fn, mopts, limit);

    // Translate matches back to Python.
    PyObjectPtr matches_list(PyList_New(0));
    if (!matches_list) {
      return nullptr;
    }
    for (auto const& match : matches) {
      if (PyList_Append(matches_list.get(), match->second.get()) < 0) {
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
