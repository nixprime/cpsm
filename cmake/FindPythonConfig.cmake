# FindPythonConfig
# ----------------
#
# This module locates Python libraries.
#
# This code sets the following variables:
#
# PYTHONCONFIG_FOUND - have the Python libs been found
# PYTHON_COMPILE_FLAGS - compiler flags required to include Python headers
# PYTHON_LINK_FLAGS - linker flags required to link Python libraries
#
# If calling both `find_package(PythonInterp)` and
# `find_package(PythonConfig)`, call `find_package(PythonInterp)` first.

include(FindPackageHandleStandardArgs)

find_package(PythonInterp)
if(PYTHONINTERP_FOUND)
  set(_Python_config "${PYTHON_EXECUTABLE}-config")
  execute_process(COMMAND ${_Python_config} "--includes" OUTPUT_VARIABLE PYTHON_COMPILE_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
  # "To embed Python into an application, a new --embed option must be passed
  # to python3-config --libs --embed to get -lpython3.8 (link the application
  # to libpython). To support both 3.8 and older, try python3-config --libs
  # --embed first and fallback to python3-config --libs (without --embed) if
  # the previous command fails." -
  # https://docs.python.org/3/whatsnew/3.8.html#debug-build-uses-the-same-abi-as-release-build
  execute_process(COMMAND ${_Python_config} "--ldflags" "--embed" OUTPUT_VARIABLE PYTHON_LINK_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE PYTHON_LINK_FLAGS_EMBED_RESULT)
  if(NOT PYTHON_LINK_FLAGS_EMBED_RESULT EQUAL 0)
    message(STATUS "python-config failed, retrying without --embed")
    execute_process(COMMAND ${_Python_config} "--ldflags" OUTPUT_VARIABLE PYTHON_LINK_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif(NOT PYTHON_LINK_FLAGS_EMBED_RESULT EQUAL 0)
  set(_Python_config_message "${PYTHON_COMPILE_FLAGS}; ${PYTHON_LINK_FLAGS}")
  unset(_Python_config)
else(PYTHONINTERP_FOUND)
  message(SEND_ERROR "Python interpreter not found")
endif(PYTHONINTERP_FOUND)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PythonConfig DEFAULT_MSG _Python_config_message PYTHON_COMPILE_FLAGS PYTHON_LINK_FLAGS)
unset(_Python_config_message)
