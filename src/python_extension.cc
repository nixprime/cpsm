// cpsm - fuzzy path matcher
// Copyright (C) 2015 the Authors
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

// Wrappers around Python 2/3 string type distinctions.

inline bool PyVimString_AsStringAndSize(PyObject* obj, char** data,
                                        Py_ssize_t* size) {
#if PY_MAJOR_VERSION >= 3
  *data = PyUnicode_AsUTF8AndSize(obj, size);
  return *data != nullptr;
#else
  return PyString_AsStringAndSize(obj, data, size) >= 0;
#endif
}

inline PyObject* PyVimString_FromStringAndSize(char const* data,
                                               Py_ssize_t size) {
#if PY_MAJOR_VERSION >= 3
  return PyUnicode_FromStringAndSize(data, size);
#else
  return PyString_FromStringAndSize(data, size);
#endif
}

// Item type that wraps another, and also includes a pointer to a Python
// object.
template <typename InnerItem, bool IsOwned>
struct PyObjItem {
  using Obj = typename std::conditional<IsOwned, PyObjPtr, PyObject*>::type;

  InnerItem inner;
  Obj obj;

  PyObjItem() {}
  explicit PyObjItem(InnerItem inner, Obj obj)
      : inner(std::move(inner)), obj(std::move(obj)) {}

  boost::string_ref match_key() const { return inner.match_key(); }
  boost::string_ref sort_key() const { return inner.sort_key(); }
};

// Iterators do not necessarily hold a reference on iterated values, so we must
// do so.
template <typename MatchMode>
using PyIterCtrlPItem =
    PyObjItem<cpsm::CtrlPItem<cpsm::StringRefItem, MatchMode>,
              /* IsOwned = */ true>;

// Thread-safe item source that batches items from a Python iterator.
template <typename MatchMode>
class PyIterCtrlPMatchSource {
 public:
  using Item = PyIterCtrlPItem<MatchMode>;

  explicit PyIterCtrlPMatchSource(PyObject* const iter) : iter_(iter) {
    if (!PyIter_Check(iter)) {
      throw cpsm::Error("input is not iterable");
    }
  }

  bool fill(std::vector<Item>& items) {
    std::lock_guard<std::mutex> lock(mu_);
    if (done_) {
      return false;
    }
    auto const add_item = [&](PyObjPtr item_obj) {
      if (item_obj == nullptr) {
        return false;
      }
      char* item_data;
      Py_ssize_t item_size;
      if (!PyVimString_AsStringAndSize(item_obj.get(), &item_data,
                                       &item_size)) {
        return false;
      }
      items.emplace_back(
          cpsm::CtrlPItem<cpsm::StringRefItem, MatchMode>(
              (cpsm::StringRefItem(boost::string_ref(item_data, item_size)))),
          std::move(item_obj));
      return true;
    };
    for (Py_ssize_t i = 0; i < batch_size(); i++) {
      if (!add_item(PyObjPtr(PyIter_Next(iter_)))) {
        done_ = true;
        return false;
      }
    }
    return true;
  }

  static constexpr Py_ssize_t batch_size() { return 512; }

 private:
  std::mutex mu_;
  PyObject* const iter_;
  bool done_ = false;
};

// Lists hold references on their elements, so we can use borrowed references.
template <typename MatchMode>
using PyListCtrlPItem =
    PyObjItem<cpsm::CtrlPItem<cpsm::StringRefItem, MatchMode>,
              /* IsOwned = */ false>;

// Thread-safe item source that batches items from a Python list.
template <typename MatchMode>
class PyListCtrlPMatchSource {
 public:
  using Item = PyListCtrlPItem<MatchMode>;

  explicit PyListCtrlPMatchSource(PyObject* const list) : list_(list) {
    size_ = PyList_Size(list);
    if (size_ < 0) {
      throw cpsm::Error("input is not a list");
    }
  }

  bool fill(std::vector<Item>& items) {
    std::lock_guard<std::mutex> lock(mu_);
    if (done_) {
      return false;
    }
    auto const add_item = [&](PyObject* item_obj) {
      if (item_obj == nullptr) {
        return false;
      }
      char* item_data;
      Py_ssize_t item_size;
      if (!PyVimString_AsStringAndSize(item_obj, &item_data, &item_size)) {
        return false;
      }
      items.emplace_back(
          cpsm::CtrlPItem<cpsm::StringRefItem, MatchMode>(
              (cpsm::StringRefItem(boost::string_ref(item_data, item_size)))),
          item_obj);
      return true;
    };
    Py_ssize_t const max = std::min(i_ + batch_size(), size_);
    for (; i_ < max; i_++) {
      if (!add_item(PyList_GetItem(list_, i_))) {
        done_ = true;
        return false;
      }
    }
    return i_ != size_;
  }

  static constexpr Py_ssize_t batch_size() { return 512; }

 private:
  std::mutex mu_;
  PyObject* const list_;
  Py_ssize_t i_ = 0;
  Py_ssize_t size_ = 0;
  bool done_ = false;
};

// `dst` must be a functor compatible with signature `void(boost::string_ref
// item, boost::string_ref match_key, PyObject* obj, cpsm::MatchInfo* info)`.
template <typename Sink>
void for_each_pyctrlp_match(boost::string_ref const query,
                            cpsm::Options const& opts,
                            cpsm::CtrlPMatchMode const match_mode,
                            PyObject* const items_iter, Sink&& dst) {
  bool const is_list = PyList_Check(items_iter);
#define DO_MATCH_WITH(MMODE)                                                   \
  if (is_list) {                                                               \
    cpsm::for_each_match<PyListCtrlPItem<MMODE>>(                              \
        query, opts, PyListCtrlPMatchSource<MMODE>(items_iter),                \
        [&](PyListCtrlPItem<MMODE> const& item, cpsm::MatchInfo* const info) { \
          dst(item.inner.inner.item(), item.match_key(), item.obj, info);      \
        });                                                                    \
  } else {                                                                     \
    cpsm::for_each_match<PyIterCtrlPItem<MMODE>>(                              \
        query, opts, PyIterCtrlPMatchSource<MMODE>(items_iter),                \
        [&](PyIterCtrlPItem<MMODE> const& item, cpsm::MatchInfo* const info) { \
          dst(item.inner.inner.item(), item.match_key(), item.obj.get(),       \
              info);                                                           \
        });                                                                    \
  }
  switch (match_mode) {
    case cpsm::CtrlPMatchMode::FULL_LINE:
      DO_MATCH_WITH(cpsm::FullLineMatch);
      break;
    case cpsm::CtrlPMatchMode::FILENAME_ONLY:
      DO_MATCH_WITH(cpsm::FilenameOnlyMatch);
      break;
    case cpsm::CtrlPMatchMode::FIRST_NON_TAB:
      DO_MATCH_WITH(cpsm::FirstNonTabMatch);
      break;
    case cpsm::CtrlPMatchMode::UNTIL_LAST_TAB:
      DO_MATCH_WITH(cpsm::UntilLastTabMatch);
      break;
  }
#undef DO_MATCH_WITH
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

}  // namespace

extern "C" {

constexpr char CTRLP_MATCH_DOC[] =
"ctrlp_match(\n"
"    items, query, limit=-1, mmode=None, ispath=False, crfile=None,\n"
"    highlight_mode=None, match_crfile=False, max_threads=0,\n"
"    query_inverting_delimiter=None, unicode=False)\n"
"\n"
"Returns a tuple `(results, regexes)` containing information about the items\n"
"in `items` that match `query`, in order of descending match quality.\n"
"\n"
"Options:\n"
"limit -- if positive, the maximum number of results to return\n"
"mmode -- CtrlP match mode (default 'full-line', i.e. full path mode)\n"
"ispath -- if true, all items are paths\n"
"crfile -- if set, the currently open file\n"
"highlight_mode -- controls `regexes`, see README\n"
"match_crfile -- if false, never match `crfile`\n"
"max_threads -- if positive, limit on the number of matcher threads\n"
"query_inverting_delimiter -- see README\n"
"regex_line_prefix -- prefix for each regex in `regexes`\n"
"unicode -- if true, all items are UTF-8-encoded";

static PyObject* cpsm_ctrlp_match(PyObject* self, PyObject* args,
                                  PyObject* kwargs) {
  static char const* kwlist[] = {"items", "query", "limit", "mmode", "ispath",
                                 "crfile", "highlight_mode", "match_crfile",
                                 "max_threads", "query_inverting_delimiter",
                                 "regex_line_prefix", "unicode", nullptr};
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
  char const* regex_line_prefix_data = nullptr;
  Py_ssize_t regex_line_prefix_size = 0;
  int unicode = 0;
  if (!PyArg_ParseTupleAndKeywords(
          args, kwargs, "Os#|iz#iz#z#iiz#z#i", const_cast<char**>(kwlist),
          &items_obj, &query_data, &query_size, &limit_int, &mmode_data,
          &mmode_size, &is_path, &crfile_data, &crfile_size,
          &highlight_mode_data, &highlight_mode_size, &match_crfile,
          &max_threads_int, &query_inverting_delimiter_data,
          &query_inverting_delimiter_size, &regex_line_prefix_data,
          &regex_line_prefix_size, &unicode)) {
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

    PyObjPtr output_tuple(PyTuple_New(2));
    if (!output_tuple) {
      return nullptr;
    }
    PyObjPtr matches_list(PyList_New(0));
    if (!matches_list) {
      return nullptr;
    }
    std::vector<std::string> highlight_regexes;
    for_each_pyctrlp_match(
        query, mopts,
        cpsm::parse_ctrlp_match_mode(boost::string_ref(mmode_data, mmode_size)),
        items_obj,
        [&](boost::string_ref const item, boost::string_ref const match_key,
            PyObject* const obj, cpsm::MatchInfo* const info) {
          if (PyList_Append(matches_list.get(), obj) < 0) {
            throw cpsm::Error("match appending failed");
          }
          auto match_positions = info->match_positions();
          // Adjust match positions to account for substringing.
          std::size_t const delta = match_key.data() - item.data();
          for (auto& pos : match_positions) {
            pos += delta;
          }
          cpsm::get_highlight_regexes(
              highlight_mode, item, match_positions, highlight_regexes,
              boost::string_ref(regex_line_prefix_data,
                                regex_line_prefix_size));
        });
    if (PyTuple_SetItem(output_tuple.get(), 0, matches_list.release())) {
      return nullptr;
    }
    PyObjPtr regexes_list(PyList_New(0));
    if (!regexes_list) {
      return nullptr;
    }
    for (auto const& regex : highlight_regexes) {
      PyObjPtr regex_str(
          PyVimString_FromStringAndSize(regex.data(), regex.size()));
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
     METH_VARARGS | METH_KEYWORDS, CTRLP_MATCH_DOC},
    {nullptr, nullptr, 0, nullptr}};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "cpsm_py",
    NULL,
    -1,
    cpsm_py_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC PyInit_cpsm_py() { return PyModule_Create(&moduledef); }
#else
PyMODINIT_FUNC initcpsm_py() { Py_InitModule("cpsm_py", cpsm_py_methods); }
#endif

} /* extern "C" */
