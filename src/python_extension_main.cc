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
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

#include <boost/range/adaptor/reversed.hpp>
#include <boost/utility/string_ref.hpp>

#include "api.h"
#include "ctrlp_util.h"
#include "par_util.h"
#include "str_util.h"

namespace {

struct PyObjectDeleter {
  void operator()(PyObject* const p) const { Py_DECREF(p); }
};

// Reference-owning, self-releasing PyObject smart pointer.
typedef std::unique_ptr<PyObject, PyObjectDeleter> PyObjPtr;

// Item type that wraps another, and also includes a borrowed pointer to a
// Python object.
template <typename InnerItem>
struct PyObjItem {
  InnerItem inner;
  PyObject* obj;

  PyObjItem() {}
  explicit PyObjItem(InnerItem inner, PyObject* const obj)
      : inner(std::move(inner)), obj(obj) {}

  boost::string_ref match_key() const { return inner.match_key(); }
  boost::string_ref sort_key() const { return inner.sort_key(); }
};

template <typename MatchMode>
class PyListCtrlPMatchSourceState;

// Thread-safe match source that batches items from a Python list protected by
// a std::mutex, templated on CtrlP match mode.
template <typename MatchMode>
class PyListCtrlPMatchSource {
 public:
  typedef typename PyListCtrlPMatchSourceState<MatchMode>::Item Item;

  explicit PyListCtrlPMatchSource(PyListCtrlPMatchSourceState<MatchMode>& state)
      : state_(state) {}

  bool operator()(std::vector<Item>& items) { return state_(items); }

 private:
  PyListCtrlPMatchSourceState<MatchMode>& state_;
};

template <typename MatchMode>
class PyListCtrlPMatchSourceState {
 public:
  typedef PyObjItem<cpsm::CtrlPItem<cpsm::StringRefItem, MatchMode>> Item;

  static constexpr Py_ssize_t BATCH_SIZE = 512;

  explicit PyListCtrlPMatchSourceState(PyObject* const list) : list_(list) {
    size_ = PyList_Size(list);
    if (size_ < 0) {
      throw cpsm::Error("input is not a list");
    }
  }

  bool operator()(std::vector<Item>& items) {
    std::lock_guard<std::mutex> lock(mu_);
    if (have_python_exception_) {
      return false;
    }
    Py_ssize_t max = i_ + BATCH_SIZE;
    if (max > size_) {
      max = size_;
    }
    items.reserve(max - i_);
    for (; i_ < max; i_++) {
      PyObject* const item_obj = PyList_GetItem(list_, i_);
      if (!item_obj) {
        have_python_exception_ = true;
        return false;
      }
      char* item_data;
      Py_ssize_t item_size;
      if (PyString_AsStringAndSize(item_obj, &item_data, &item_size) < 0) {
        have_python_exception_ = true;
        return false;
      }
      items.emplace_back(
          cpsm::CtrlPItem<cpsm::StringRefItem, MatchMode>(
              (cpsm::StringRefItem(boost::string_ref(item_data, item_size)))),
          item_obj);
    }
    return i_ != size_;
  }

  PyListCtrlPMatchSource<MatchMode> get_functor() {
    return PyListCtrlPMatchSource<MatchMode>(*this);
  }

 private:
  std::mutex mu_;
  PyObject* const list_;
  Py_ssize_t i_ = 0;
  Py_ssize_t size_;
  bool have_python_exception_ = false;
};

unsigned int get_nr_threads(unsigned int const max_threads) {
  std::size_t nr_threads = cpsm::Thread::hardware_concurrency();
  if (!nr_threads) {
    nr_threads = 1;
  }
  if (max_threads && (nr_threads > max_threads)) {
    nr_threads = max_threads;
  }
  return nr_threads;
}

template <typename T>
using CpsmItem = typename PyListCtrlPMatchSourceState<T>::Item;

}  // namespace

extern "C" {

static PyObject* cpsm_ctrlp_match(PyObject* self, PyObject* args,
                                  PyObject* kwargs) {
  static char const* kwlist[] = {"items", "query", "limit", "mmode", "ispath",
                                 "crfile", "highlight_mode", "match_crfile",
                                 "max_threads", "query_inverting_delimiter",
                                 "unicode", nullptr};
  // Required parameters.
  PyObject* items_obj;
  char const* query_data;
  Py_ssize_t query_size;
  // CtrlP-provided options.
  int limit_int = -1;
  char const* mmode_data = nullptr;
  Py_ssize_t mmode_size = 0;
  int is_path = 0;
  char const* crfile_data = nullptr;
  Py_ssize_t crfile_size = 0;
  // cpsm-specific options.
  char const* highlight_mode_data = nullptr;
  Py_ssize_t highlight_mode_size = 0;
  int match_crfile = 0;
  int max_threads_int = 0;
  char const* query_inverting_delimiter_data = nullptr;
  Py_ssize_t query_inverting_delimiter_size = 0;
  int unicode = 0;
  if (!PyArg_ParseTupleAndKeywords(
          args, kwargs, "Os#|is#is#s#iis#i", const_cast<char**>(kwlist),
          &items_obj, &query_data, &query_size, &limit_int, &mmode_data,
          &mmode_size, &is_path, &crfile_data, &crfile_size,
          &highlight_mode_data, &highlight_mode_size, &match_crfile,
          &max_threads_int, &query_inverting_delimiter_data,
          &query_inverting_delimiter_size, &unicode)) {
    return nullptr;
  }

  try {
    std::string query(query_data, query_size);
    boost::string_ref query_inverting_delimiter(query_inverting_delimiter_data,
                                                query_inverting_delimiter_size);
    if (!query_inverting_delimiter.empty()) {
      if (query_inverting_delimiter.size() > 1) {
        throw cpsm::Error(
            "query inverting delimiter must be a single character");
      }
      query = cpsm::str_join(boost::adaptors::reverse(cpsm::str_split(
                                 query, query_inverting_delimiter[0])),
                             "");
    }

    auto const mopts =
        cpsm::Options()
            .set_crfile(boost::string_ref(crfile_data, crfile_size))
            .set_limit((limit_int >= 0) ? std::size_t(limit_int) : 0)
            .set_match_crfile(match_crfile)
            .set_nr_threads(
                 get_nr_threads((max_threads_int >= 0)
                                    ? static_cast<unsigned int>(max_threads_int)
                                    : 0))
            .set_path(is_path)
            .set_unicode(unicode)
            .set_want_match_info(true);
    boost::string_ref const highlight_mode(highlight_mode_data,
                                           highlight_mode_size);
    std::vector<PyObject*> matched_objs;
    std::vector<std::string> highlight_regexes;
    auto const write_match =
        [&](boost::string_ref const item, boost::string_ref const match_key,
            PyObject* const obj, cpsm::MatchInfo* const info) {
          matched_objs.push_back(obj);
          auto match_positions = info->match_positions();
          // Adjust match positions to account for substringing.
          std::size_t const delta = match_key.data() - item.data();
          for (auto& pos : match_positions) {
            pos += delta;
          }
          cpsm::get_highlight_regexes(highlight_mode, item, match_positions,
                                      highlight_regexes);
        };
#define DO_MATCH_WITH_MMODE(MMODE)                                          \
  cpsm::for_each_match<CpsmItem<MMODE>>(                                    \
      query, mopts,                                                         \
      PyListCtrlPMatchSourceState<MMODE>(items_obj).get_functor(),          \
      [&](CpsmItem<MMODE> const& item, cpsm::MatchInfo* const info) {       \
    write_match(item.inner.inner.item(), item.match_key(), item.obj, info); \
      });
    switch (cpsm::parse_ctrlp_match_mode(
        boost::string_ref(mmode_data, mmode_size))) {
      case cpsm::CtrlPMatchMode::FULL_LINE:
        DO_MATCH_WITH_MMODE(cpsm::FullLineMatch);
        break;
      case cpsm::CtrlPMatchMode::FILENAME_ONLY:
        DO_MATCH_WITH_MMODE(cpsm::FilenameOnlyMatch);
        break;
      case cpsm::CtrlPMatchMode::FIRST_NON_TAB:
        DO_MATCH_WITH_MMODE(cpsm::FirstNonTabMatch);
        break;
      case cpsm::CtrlPMatchMode::UNTIL_LAST_TAB:
        DO_MATCH_WITH_MMODE(cpsm::UntilLastTabMatch);
        break;
    }
#undef DO_MATCH_WITH_MMODE

    // Translate matches back to Python.
    PyObjPtr output_tuple(PyTuple_New(2));
    if (!output_tuple) {
      return nullptr;
    }
    PyObjPtr matches_list(PyList_New(0));
    if (!matches_list) {
      return nullptr;
    }
    for (PyObject* const match_obj : matched_objs) {
      if (PyList_Append(matches_list.get(), match_obj) < 0) {
        return nullptr;
      }
    }
    if (PyTuple_SetItem(output_tuple.get(), 0, matches_list.release())) {
      return nullptr;
    }
    PyObjPtr regexes_list(PyList_New(0));
    if (!regexes_list) {
      return nullptr;
    }
    for (auto const& regex : highlight_regexes) {
      PyObjPtr regex_str(
          PyString_FromStringAndSize(regex.data(), regex.size()));
      if (!regex_str) {
        return nullptr;
      }
      if (PyList_Append(regexes_list.get(), regex_str.get()) < 0) {
        return nullptr;
      }
    }
    if (PyTuple_SetItem(output_tuple.get(), 1, regexes_list.release())) {
      return nullptr;
    }
    return output_tuple.release();
  } catch (std::exception const& ex) {
    PyErr_SetString(PyExc_RuntimeError, ex.what());
    return nullptr;
  }
}

static PyMethodDef cpsm_py_methods[] = {
    {"ctrlp_match", reinterpret_cast<PyCFunction>(cpsm_ctrlp_match),
     METH_VARARGS | METH_KEYWORDS,
     "Match strings with a CtrlP-compatible interface"},
    {nullptr, nullptr, 0, nullptr}};

PyMODINIT_FUNC initcpsm_py() { Py_InitModule("cpsm_py", cpsm_py_methods); }

} /* extern "C" */
