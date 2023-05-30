FILE(GLOB_RECURSE NEZHA_SOURCES "*.mm" "*.cpp" "*.hpp")

SET(CMAKE_CXX_STANDARD 17)

INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/ext/stb")
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/ext/glm")
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/nezha/include")

ADD_LIBRARY(nezha_core ${NEZHA_SOURCES})

TARGET_COMPILE_DEFINITIONS(nezha_core PUBLIC GLM_ENABLE_EXPERIMENTAL NEZHA_PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

# IF (APPLE)
#   MESSAGE(STATUS "Linking with MoltenVK")
#   TARGET_INCLUDE_DIRECTORIES(nezha_core PUBLIC ${CMAKE_SOURCE_DIR}/ext/MoltenVK/MoltenVK/include)
#   # LINK_DIRECTORIES(${CMAKE_SOURCE_DIR}/ext/MoltenVK/MoltenVK/dylib/macOS)
#   TARGET_LINK_LIBRARIES(nezha_core PUBLIC "glfw" 
#                         "${CMAKE_SOURCE_DIR}/ext/MoltenVK/MoltenVK/dylib/macOS/libMoltenVK.dylib" 
#                         "objc"
#                         "-framework Foundation"
#                         "-framework QuartzCore")
# ENDIF()

FIND_PACKAGE(Vulkan)

IF (Vulkan_FOUND)
  MESSAGE(STATUS "Found Vulkan package in system ${Vulkan_LIBRARY}")
  TARGET_INCLUDE_DIRECTORIES(nezha_core PUBLIC "${Vulkan_INCLUDE_DIRS}")
  TARGET_LINK_LIBRARIES(nezha_core PUBLIC "${Vulkan_LIBRARY}" "glfw" "/usr/local/lib/libMoltenVK.dylib" "objc"
    "-framework Foundation" "-framework QuartzCore" "-framework MetalPerformanceShaders")
ENDIF()

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -framework Cocoa")
