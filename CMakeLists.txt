cmake_minimum_required(VERSION 2.8.3)

project(cpsm)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include_directories(${PROJECT_SOURCE_DIR}/src)
set(CMAKE_BUILD_TYPE Release)
if("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU|Clang|Intel")
  list(APPEND CMAKE_CXX_FLAGS "-std=c++11 -Wall")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  # Versions of MSVC that support C++11 at all support it by default, so hope
  # for the best...
else()
  message(FATAL_ERROR "Unknown compiler - don't know how to turn on C++11.")
endif()

set(Boost_USE_MULTITHREADED ON)
find_package(Boost REQUIRED COMPONENTS program_options)
include_directories(${Boost_INCLUDE_DIRS})

find_package(GooglePerfTools)
if(GOOGLE_PERFTOOLS_FOUND)
  include_directories(${GOOGLE_PERFTOOLS_INCLUDE_DIR})
  add_definitions(-DCPSM_CONFIG_HAVE_GOOGLE_PERFTOOLS=1)
endif()

find_package(ICU)
if(ICU_FOUND)
  include_directories(${ICU_INCLUDE_DIRS})
  add_definitions(-DCPSM_CONFIG_ICU=1)
endif()

set(Python_ADDITIONAL_VERSIONS 2.7 2.6)
find_package(PythonLibrary REQUIRED)
include_directories(${PYTHON_INCLUDE_PATH})

find_package(Threads REQUIRED)

add_library(cpsm_core src/matcher.cc src/path_util.cc src/str_util.cc)
target_link_libraries(cpsm_core ${Boost_LIBRARIES})
if(ICU_FOUND)
  target_link_libraries(cpsm_core ${ICU_LIBRARIES})
endif()
target_link_libraries(cpsm_core ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(cpsm_core PROPERTIES COMPILE_FLAGS "-fPIC")

add_library(cpsm_py SHARED src/ctrlp_util.cc src/python_extension_main.cc)
target_link_libraries(cpsm_py cpsm_core ${PYTHON_LIBRARIES})
set_target_properties(cpsm_py PROPERTIES PREFIX "" SUFFIX ".so")
install(TARGETS cpsm_py DESTINATION ${PROJECT_SOURCE_DIR}/autoload)
install(TARGETS cpsm_py DESTINATION ${PROJECT_SOURCE_DIR}/test)

add_executable(cpsm_cli src/cpsm_cli_main.cc)
target_link_libraries(cpsm_cli cpsm_core ${Boost_LIBRARIES})
if(GOOGLE_PERFTOOLS_FOUND)
  target_link_libraries(cpsm_cli profiler)
endif()
set_target_properties(cpsm_cli PROPERTIES PREFIX "" SUFFIX ".bin")
install(TARGETS cpsm_cli DESTINATION ${PROJECT_SOURCE_DIR}/test)

add_executable(matcher_test src/matcher_test.cc)
target_link_libraries(matcher_test cpsm_core)
set_target_properties(matcher_test PROPERTIES PREFIX "" SUFFIX ".bin")
install(TARGETS matcher_test DESTINATION ${PROJECT_SOURCE_DIR}/test)
