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
  execute_process(COMMAND ${_Python_config} "--ldflags" "--embed" OUTPUT_VARIABLE PYTHON_LINK_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_VARIABLE PYTHON_EMBED_FAILED)
  if(PYTHON_EMBED_FAILED)
    message(STATUS "PythonConfig could not embed Python. (Fine for < Python 3.8)")
    execute_process(COMMAND ${_Python_config} "--ldflags" OUTPUT_VARIABLE PYTHON_LINK_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif(PYTHON_EMBED_FAILED)
  set(_Python_config_message "${PYTHON_COMPILE_FLAGS}; ${PYTHON_LINK_FLAGS}")
  unset(_Python_config)
else(PYTHONINTERP_FOUND)
  message(SEND_ERROR "Python interpreter not found")
endif(PYTHONINTERP_FOUND)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PythonConfig DEFAULT_MSG _Python_config_message PYTHON_COMPILE_FLAGS PYTHON_LINK_FLAGS)
unset(_Python_config_message)
