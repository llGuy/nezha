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

SUBDIRLIST(SUBDIRS ${CMAKE_SOURCE_DIR}/examples)

INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/ext/stb")
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/ext/glm")
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/nezha/include")

SET(CMAKE_CXX_STANDARD 17)

FOREACH(subdir ${SUBDIRS})
  FILE(GLOB_RECURSE sources ${subdir}/*.cpp ${subdir}/*.hpp)
  ADD_EXECUTABLE(${subdir} ${sources})
  TARGET_COMPILE_DEFINITIONS(${subdir} PUBLIC 
    EXAMPLE_ROOT=${CMAKE_SOURCE_DIR}/examples/${subdir}
    PROJECT_ROOT=${CMAKE_SOURCE_DIR})
  TARGET_LINK_LIBRARIES(${subdir} PUBLIC nezha_core)
ENDFOREACH()
