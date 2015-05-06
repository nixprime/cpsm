// cpsm - fuzzy path matcher
// Copyright (C) 2015 Jamie Liu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <algorithm>
#include <string>

#include <boost/utility/string_ref.hpp>

#include "ctrlp_util.h"
#include "defer.h"
#include "match.h"
#include "matcher.h"

using cpsm::defer;

extern "C" {

static PyObject* cpsm_ctrlp_match(PyObject* self, PyObject* args,
                                  PyObject* kwargs) {
  static char const* kwlist[] = {"items", "query", "limit", "mmode", "ispath",
                                 "crfile", nullptr};
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
    PyObject* items_iter = PyObject_GetIter(items_obj);
    if (!items_iter) {
      return nullptr;
    }
    auto items_iter_decref = defer([=]() { Py_DECREF(items_iter); });
    PyObject* item_obj;
    while ((item_obj = PyIter_Next(items_iter))) {
      auto item_obj_decref = defer([=]() { Py_DECREF(item_obj); });
      char* item_data;
      Py_ssize_t item_size;
      if (PyString_AsStringAndSize(item_obj, &item_data, &item_size) < 0) {
        return nullptr;
      }
      boost::string_ref item(item_data, item_size);
      matcher.append_match(item, matches);
    }
    std::sort(matches.begin(), matches.end());
    if (limit >= 0 && matches.size() > std::size_t(limit)) {
      matches.resize(limit);
    }

    PyObject* matches_list = PyList_New(0);
    if (!matches_list) {
      return nullptr;
    }
    auto matches_list_decref = defer([=]() { Py_DECREF(matches_list); });
    for (auto const& match : matches) {
      PyObject* match_str =
          PyString_FromStringAndSize(match.item().data(), match.item().size());
      if (!match_str) {
        return nullptr;
      }
      auto match_str_decref = defer([=]() { Py_DECREF(match_str); });
      if (PyList_Append(matches_list, match_str) < 0) {
        return nullptr;
      }
    }
    matches_list_decref.cancel();
    return matches_list;
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

PyMODINIT_FUNC initcpsm() {
  Py_InitModule("cpsm", cpsm_methods);
}

}
