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

#include <boost/utility/string_ref.hpp>

#include "ctrlp_util.h"
#include "match.h"
#include "matcher.h"
#include "par_util.h"
#include "str_util.h"

namespace {

template <typename F>
class Deferred {
  public:
    explicit Deferred(F f) : f_(std::move(f)), enabled_(true) {}
    ~Deferred() { if (enabled_) f_(); }

    void cancel() { enabled_ = false; }

  private:
    F f_;
    bool enabled_;
};

template <typename F>
Deferred<F> defer(F f) {
  return Deferred<F>(f);
}

struct PyObjectDeleter {
  void operator()(PyObject* const p) const { Py_DECREF(p); }
};

// Reference-owning, self-releasing PyObject smart pointer.
typedef std::unique_ptr<PyObject, PyObjectDeleter> PyObjectPtr;

void get_concurrency_params(unsigned int const max_threads,
                            unsigned int& nr_threads, std::size_t& batch_size) {
  nr_threads = cpsm::Thread::hardware_concurrency();
  if (!nr_threads) {
    nr_threads = 1;
  }
  if (max_threads && (nr_threads > max_threads)) {
    nr_threads = max_threads;
  }
  // Batch size needs to be large enough that processing one batch is at least
  // long enough for all other threads to collect one batch (to minimize
  // contention on the lock that guards the Python API), so it should scale
  // linearly with the number of other threads. The coefficient is determined
  // empirically.
  constexpr std::size_t BATCH_SIZE_PER_THREAD = 64;
  batch_size = BATCH_SIZE_PER_THREAD * (nr_threads - 1);
  if (!batch_size) {
    // nr_threads == 1, so batch_size is fairly arbitrary.
    batch_size = BATCH_SIZE_PER_THREAD;
  }
}

}  // namespace

extern "C" {

static PyObject* cpsm_ctrlp_match(PyObject* self, PyObject* args,
                                  PyObject* kwargs) {
  static char const* kwlist[] = {"items",  "query",  "limit", "mmode",
                                 "ispath", "crfile", "max_threads", "unicode",
                                 nullptr};
  PyObject* items_obj;
  char const* query_data;
  Py_ssize_t query_size;
  int limit_int = -1;
  char const* mmode_data = nullptr;
  Py_ssize_t mmode_size = 0;
  int is_path = 0;
  char const* cur_file_data = nullptr;
  Py_ssize_t cur_file_size = 0;
  int max_threads_int = 0;
  int unicode = 0;
  if (!PyArg_ParseTupleAndKeywords(
          args, kwargs, "Os#|is#is#ii", const_cast<char**>(kwlist), &items_obj,
          &query_data, &query_size, &limit_int, &mmode_data, &mmode_size,
          &is_path, &cur_file_data, &cur_file_size, &max_threads_int,
          &unicode)) {
    return nullptr;
  }

  // Each match needs to be associated with both a boost::string_ref (for
  // correct sorting) and the PyObject (so it can be returned).
  typedef std::pair<boost::string_ref, PyObjectPtr> Item;

  try {
    std::string query(query_data, query_size);
    cpsm::MatcherOpts mopts;
    mopts.cur_file = std::string(cur_file_data, cur_file_size);
    mopts.is_path = is_path;
    cpsm::StringHandlerOpts sopts;
    sopts.unicode = unicode;
    cpsm::Matcher matcher(std::move(query), std::move(mopts),
                          cpsm::StringHandler(sopts));
    auto item_substr_fn = cpsm::match_mode_item_substr_fn(
        boost::string_ref(mmode_data, mmode_size));
    std::size_t const limit = (limit_int >= 0) ? std::size_t(limit_int) : 0;
    unsigned int const max_threads =
        (max_threads_int >= 0) ? static_cast<unsigned int>(max_threads_int) : 0;

    unsigned int nr_threads;
    std::size_t items_per_batch;
    get_concurrency_params(max_threads, nr_threads, items_per_batch);

    PyObjectPtr items_iter(PyObject_GetIter(items_obj));
    if (!items_iter) {
      return nullptr;
    }
    std::mutex items_mu;
    bool end_of_python_iter = false;
    bool have_python_ex = false;

    // Do matching in parallel.
    std::vector<std::vector<cpsm::Match<Item>>> thread_matches(nr_threads);
    std::vector<cpsm::Thread> threads;
    for (unsigned int i = 0; i < nr_threads; i++) {
      auto& matches = thread_matches[i];
      threads.emplace_back(
          [&matcher, item_substr_fn, limit, items_per_batch, &items_iter,
           &items_mu, &end_of_python_iter, &have_python_ex, &matches]() {
            std::vector<Item> items;
            items.reserve(items_per_batch);
            std::vector<PyObjectPtr> unmatched_objs;
            unmatched_objs.reserve(items_per_batch);
            // Ensure that unmatched PyObjects are released with items_mu held,
            // even if an exception is thrown.
            auto release_unmatched_objs =
                defer([&items, &unmatched_objs, &items_mu]() {
                  std::lock_guard<std::mutex> lock(items_mu);
                  items.clear();
                  unmatched_objs.clear();
                });

            // If a limit exists, each thread should only keep that many
            // matches.
            if (limit) {
              matches.reserve(limit + 1);
            }

            std::vector<char32_t> buf, buf2;
            while (true) {
              {
                // Collect a batch (with items_mu held to guard access to the
                // Python API).
                std::lock_guard<std::mutex> lock(items_mu);
                // Drop references on unmatched PyObjects.
                unmatched_objs.clear();
                if (end_of_python_iter || have_python_ex) {
                  return;
                }
                for (std::size_t i = 0; i < items_per_batch; i++) {
                  PyObjectPtr item_obj(PyIter_Next(items_iter.get()));
                  if (!item_obj) {
                    end_of_python_iter = true;
                    break;
                  }
                  char* item_data;
                  Py_ssize_t item_size;
                  if (PyString_AsStringAndSize(item_obj.get(), &item_data,
                                               &item_size) < 0) {
                    have_python_ex = true;
                    return;
                  }
                  items.emplace_back(boost::string_ref(item_data, item_size),
                                     std::move(item_obj));
                }
              }
              if (items.empty()) {
                return;
              }
              for (auto& item : items) {
                boost::string_ref item_str(item.first);
                if (item_substr_fn) {
                  item_str = item_substr_fn(item_str);
                }
                cpsm::Match<Item> m(std::move(item));
                if (matcher.match(item_str, m, &buf, &buf2)) {
                  matches.emplace_back(std::move(m));
                  if (limit) {
                    std::push_heap(matches.begin(), matches.end());
                    if (matches.size() > limit) {
                      std::pop_heap(matches.begin(), matches.end());
                      unmatched_objs.emplace_back(
                          std::move(matches.back().item.second));
                      matches.pop_back();
                    }
                  }
                } else {
                  unmatched_objs.emplace_back(std::move(m.item.second));
                }
              }
              items.clear();
            }
          });
    }
    std::size_t nr_matches = 0;
    for (unsigned int i = 0; i < nr_threads; i++) {
      threads[i].join();
      if (threads[i].has_exception()) {
        throw cpsm::Error(threads[i].exception_msg());
      }
      nr_matches += thread_matches[i].size();
    }
    if (have_python_ex) {
      return nullptr;
    }

    // Combine per-thread match lists.
    std::vector<cpsm::Match<Item>> all_matches;
    all_matches.reserve(nr_matches);
    for (unsigned int i = 0; i < nr_threads; i++) {
      auto& matches = thread_matches[i];
      std::move(matches.begin(), matches.end(),
                std::back_inserter(all_matches));
      matches.shrink_to_fit();
    }
    sort_limit(all_matches, limit);

    // Translate matches back to Python.
    PyObjectPtr matches_list(PyList_New(0));
    if (!matches_list) {
      return nullptr;
    }
    for (auto const& match : all_matches) {
      if (PyList_Append(matches_list.get(), match.item.second.get()) < 0) {
        return nullptr;
      }
    }
    return matches_list.release();
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
}
