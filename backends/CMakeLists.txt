MACRO(SUBDIRLIST result curdir)
  FILE(GLOB children RELATIVE ${curdir} ${curdir}/*)
  SET(dirlist "")
  FOREACH(child ${children})
    IF(IS_DIRECTORY ${curdir}/${child})
      LIST(APPEND dirlist ${child})
    ENDIF()
  ENDFOREACH()
  SET(${result} ${dirlist})
ENDMACRO()

SUBDIRLIST(SUBDIRS ${CMAKE_SOURCE_DIR}/backends)

FOREACH(subdir ${SUBDIRS})
  FILE(GLOB_RECURSE sources ${subdir}/src/*.c ${subdir}/*.h)
  ADD_LIBRARY(${subdir} SHARED
    ${sources})
  TARGET_COMPILE_DEFINITIONS(${subdir} PUBLIC 
    BACKEND_ROOT=${CMAKE_SOURCE_DIR}/backends/${subdir}
    PROJECT_ROOT=${CMAKE_SOURCE_DIR})
  TARGET_LINK_LIBRARIES(${subdir} PUBLIC 
    nezha_core)
ENDFOREACH()

