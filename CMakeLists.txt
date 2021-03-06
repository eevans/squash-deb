cmake_minimum_required (VERSION 2.8.7)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

project (squash)
enable_testing ()

include (GNUInstallDirs)
include (ExtraWarningFlags)
include (CheckFunctionExists)
include (CheckIncludeFile)

set (PACKAGE_NAME Squash)
set (SQUASH_TOP_SRCDIR "${PROJECT_SOURCE_DIR}")
set (SQUASH_TOP_BUILDDIR "${PROJECT_BINARY_DIR}")

set (SQUASH_VERSION_MAJOR 0)
set (SQUASH_VERSION_MINOR 7)
set (SQUASH_VERSION_REVISION 0)
set (SQUASH_VERSION_API "0.7")
set (PACKAGE_VERSION ${SQUASH_VERSION_MAJOR}.${SQUASH_VERSION_MINOR}.${SQUASH_VERSION_REVISION})

if (NOT EXISTS "${PROJECT_SOURCE_DIR}/squash/tinycthread/source/tinycthread.c")
  message (FATAL_ERROR "It looks like you don't have submodules checked out.
If you are compiling from git, make sure to run `git submodule update --init --recursive' or call autogen.sh, which will invoke that command for you.
If you are compiling from a tarball, make sure you don't use the ones automatically generated by GitHub—they don't include submodules and are, therefore, completely broken.  Official releases are on https://github.com/quixdb/squash/releases and are the ones labeled \"squash-x.y.z.tar.bz2\", not \"Source code\".  GitHub is aware of the issue.")
endif ()

set(CPACK_PACKAGE_VERSION_MAJOR ${SQUASH_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${SQUASH_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${SQUASH_VERSION_REVISION})
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
  "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_IGNORE_FILES
  "/autogen.sh;/build/;.git;.gitmodules;.gitignore;~$;${CPACK_SOURCE_IGNORE_FILES}")
include (CPack)

add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)

include_directories ("${PROJECT_BINARY_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}")

set (SQUASH_PLUGIN_DIR ${CMAKE_INSTALL_FULL_LIBDIR}/squash/${SQUASH_VERSION_API}/plugins)

CHECK_FUNCTION_EXISTS(fdopen            HAVE_FDOPEN)
CHECK_FUNCTION_EXISTS(mmap              HAVE_MMAP)
CHECK_FUNCTION_EXISTS(mremap            HAVE_MREMAP)

configure_file (
    "${PROJECT_SOURCE_DIR}/squash/config.h.in"
    "${PROJECT_BINARY_DIR}/squash/config.h")

configure_file (
    "${PROJECT_SOURCE_DIR}/squash/version.h.in"
    "${PROJECT_BINARY_DIR}/squash/version.h")

configure_file (
    "${PROJECT_SOURCE_DIR}/squash.pc.in"
    "${PROJECT_BINARY_DIR}/squash-${SQUASH_VERSION_API}.pc"
    @ONLY)

set (squash_enabled_plugins "" CACHE INTERNAL "enabled plugins" FORCE)

check_c_compiler_flag (-fvisibility=hidden CFLAG__fvisibility_hidden)
check_cxx_compiler_flag (-fvisibility=hidden CXXFLAG__fvisibility_hidden)
check_c_compiler_flag (-fvisibility-inlines-hidden CFLAG__fvisibility_inlines_hidden)
check_cxx_compiler_flag (-fvisibility-inlines-hidden CXXFLAG__fvisibility_inlines_hidden)

include (CheckCCompilerFlag)
set (OPTIMIZE_CFLAGS "")

if (CMAKE_BUILD_TYPE MATCHES "Release")
  foreach (flag
      -O3
      -flto
      -DNDEBUG)
    string (REGEX REPLACE "[-=]+" "_" test_name "CFLAG_${flag}")
    CHECK_C_COMPILER_FLAG (${flag} ${test_name})
    if (${test_name})
      set (OPTIMIZE_CFLAGS "${OPTIMIZE_CFLAGS} ${flag}")
    endif ()
  endforeach ()
endif ()

####
## Compatibility with older CMake
##
## This will probably be removed when Ubuntu bumps their LTS server
## release (and Travis upgrades).
####

if (${CMAKE_VERSION} VERSION_LESS 2.8.11)
  function (target_include_directories target scope items)
    foreach (argn RANGE 2 ${ARGC})
      if (${argn} LESS ${ARGC})
        include_directories (${ARGV${argn}})
      endif ()
    endforeach ()
  endfunction ()
endif ()

####
## Helper functions
####

function (squash_plugin_add name sources)
  add_library(squash${SQUASH_VERSION_API}-plugin-${name} SHARED ${${sources}})
  target_link_libraries (squash${SQUASH_VERSION_API}-plugin-${name} squash${SQUASH_VERSION_API})

  # Mostly so we can use the plugins uninstalled
  configure_file (squash.ini squash.ini)

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/squash.ini
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/squash/${SQUASH_VERSION_API}/plugins/${name})

  install(TARGETS squash${SQUASH_VERSION_API}-plugin-${name}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/squash/${SQUASH_VERSION_API}/plugins/${name}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/squash/${SQUASH_VERSION_API}/plugins/${name})

  set (squash_enabled_plugins "${squash_enabled_plugins};${name}" CACHE INTERNAL "enabled plugins" FORCE)

  if (CFLAG__fvisibility_hidden)
    set_property (TARGET squash${SQUASH_VERSION_API}-plugin-${name} APPEND PROPERTY COMPILE_FLAGS -fvisibility=hidden)
    set_property (TARGET squash${SQUASH_VERSION_API}-plugin-${name} APPEND PROPERTY LINK_FLAGS -fvisibility=hidden)
  endif ()

  if (NOT CMAKE_BUILD_TYPE MATCHES "Release")
    foreach (file ${${sources}})
      if ("${file}" MATCHES "^squash-${name}.c$")
        add_extra_warning_cflags (SOURCE "${file}")
      elseif ("${file}" MATCHES "^squash-${name}.(cpp|cc|cxx)$")
        add_extra_warning_cxxflags (SOURCE "${file}")
      endif ()

      squash_source_file_add_compile_flag (${file} "${OPTIMIZE_CFLAGS}")
    endforeach ()
  endif ()
endfunction (squash_plugin_add)

macro (squash_plugin_add_link_libraries plugin libs)
  target_link_libraries (squash${SQUASH_VERSION_API}-plugin-${plugin} ${libs})
endmacro (squash_plugin_add_link_libraries)

macro (squash_plugin_add_include_directories plugin)
  set (dirs ${ARGV})
  list (REMOVE_AT dirs 0)
  target_include_directories (squash${SQUASH_VERSION_API}-plugin-${plugin} PRIVATE ${dirs})
endmacro (squash_plugin_add_include_directories)

macro (squash_plugin_add_for_package plugin package sources)
  find_package (${package})
  string (TOUPPER ${package} package_uc)

  if (${package_uc}_FOUND)
    squash_plugin_add (${plugin} ${sources})

    if (${package_uc}_LIBRARIES)
      squash_plugin_add_link_libraries (${plugin} ${${package_uc}_LIBRARIES})
    endif ()

    if (${package_uc}_INCLUDE_DIRS)
      squash_plugin_add_include_directories (${plugin} ${${package_uc}_INCLUDE_DIRS})
    endif ()
  endif (${package_uc}_FOUND)
endmacro (squash_plugin_add_for_package)

function (squash_target_add_cflag target flag)
  get_target_property (existing ${target} COMPILE_FLAGS)
  if (${existing} STREQUAL "existing-NOTFOUND")
    set_target_properties (${target}
      PROPERTIES COMPILE_FLAGS "${flag}")
  else ()
    set_target_properties (${target}
      PROPERTIES COMPILE_FLAGS "${existing} ${flag}")
  endif ()
endfunction ()

function (squash_target_add_ldflag target flag)
  get_target_property (existing ${target} LINK_FLAGS)
  if (${existing} STREQUAL "existing-NOTFOUND")
    set_target_properties (${target}
      PROPERTIES LINK_FLAGS "${flag}")
  else ()
    set_target_properties (${target}
      PROPERTIES LINK_FLAGS "${existing} ${flag}")
  endif ()
endfunction ()

function (squash_target_add_coverage target)
  if (CMAKE_BUILD_TYPE MATCHES "Coverage")
    foreach (cflag -g -O0 -fprofile-arcs -ftest-coverage)
      squash_target_add_cflag (${target} ${cflag})
    endforeach ()

    foreach (cflag -fprofile-arcs -ftest-coverage)
      squash_target_add_ldflag (${target} ${cflag})
    endforeach ()
  endif ()
endfunction ()

function (squash_source_file_add_compile_flag_ex file flag)
  get_source_file_property (existing ${file} COMPILE_FLAGS)
  if ("${existing}" STREQUAL "NOTFOUND")
    set_source_files_properties (${file}
      PROPERTIES COMPILE_FLAGS "${flag}")
  else ()
    set_source_files_properties (${file}
      PROPERTIES COMPILE_FLAGS "${existing} ${flag}")
  endif ()
endfunction ()

function (squash_source_file_add_compile_flag file flag)
  if ("${flag}" STREQUAL "")
    return ()
  endif ()

  if ("${file}" MATCHES ".c$")
    string (REGEX REPLACE "[-=]+" "_" test_name "CFLAG_${flag}")
    CHECK_C_COMPILER_FLAG (${flag} ${test_name})
  elseif ("${file}" MATCHES ".(cpp|cc|cxx)$")
    string (REGEX REPLACE "[-=]+" "_" test_name "CXXFLAG_${flag}")
    CHECK_CXX_COMPILER_FLAG (${flag} ${test_name})
  else ()
    return ()
  endif ()

  if (NOT ${test_name})
    return ()
  endif ()

  squash_source_file_add_compile_flag_ex ("${file}" "${flag}")
endfunction ()

function (squash_source_file_set_c99 files)
  foreach (file ${${files}})
    squash_source_file_add_compile_flag ("${file}" "-std=c99")
  endforeach (file)
endfunction (squash_source_file_set_c99)

add_subdirectory (squash)
add_subdirectory (plugins)
add_subdirectory (utils)
add_subdirectory (docs)
add_subdirectory (examples)
add_subdirectory (bindings)
add_subdirectory (tests)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/squash-${SQUASH_VERSION_API}.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
