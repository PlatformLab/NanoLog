# Try to find real time libraries
# Once done, this will define
#
# RT_FOUND     - system has rt library
# RT_LIBRARIES - rt libraries directory

include(FindPackageHandleStandardArgs)

if(RT_LIBRARIES)
  set(RT_FIND_QUIETLY TRUE)
else()
 find_library(
   RT_LIBRARY
   NAMES rt
   HINTS ${RT_ROOT_DIR}
   PATH_SUFFIXES ${LIBRARY_PATH_PREFIX})

  set(RT_LIBRARIES ${RT_LIBRARY})
  find_package_handle_standard_args(rt DEFAULT_MSG RT_LIBRARY)
  mark_as_advanced(RT_LIBRARY)
endif()
