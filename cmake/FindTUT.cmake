################################################################################
#
# \file      FindTUT.cmake
# \copyright 2012-2015 J. Bakosi,
#            2016-2018 Los Alamos National Security, LLC.,
#            2019 Triad National Security, LLC.
#            All rights reserved. See the LICENSE file for details.
# \brief     Find the Template Unit Test library headers
#
################################################################################

# TUT: http://mrzechonek.github.io/tut-framework/
#
#  TUT_FOUND        - True if TUT is found
#  TUT_INCLUDE_DIRS - TUT include files directories
#
#  Set TUT_ROOT before calling find_package to a path to add an additional
#  search path, e.g.,
#
#  Usage:
#
#  set(TUT_ROOT "/path/to/custom/tut") # prefer over system
#  find_package(TUT)
#  if(TUT_FOUND)
#    include_directories(${TUT_INCLUDE_DIRS})
#  endif()

find_package(PkgConfig)

# If already in cache, be silent
if (TUT_INCLUDE_DIRS)
  set (TUT_FIND_QUIETLY TRUE)
  pkg_check_modules(PC_TUT QUIET tut)
else()
  pkg_check_modules(PC_TUT tut)
endif()

# Look for the header file
FIND_PATH(TUT_INCLUDE_DIR NAMES tut.h tut.hpp
                          HINTS ${TUT_ROOT}/include
                                $ENV{TUT_ROOT}/include 
                                ${PC_TUT_INCLUDE_DIRS}
                          PATH_SUFFIXES tut)

set(TUT_INCLUDE_DIRS ${TUT_INCLUDE_DIR})

# Handle the QUIETLY and REQUIRED arguments and set TUT_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TUT DEFAULT_MSG TUT_INCLUDE_DIRS)

MARK_AS_ADVANCED(TUT_INCLUDE_DIRS)
