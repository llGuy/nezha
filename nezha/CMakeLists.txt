ADD_LIBRARY(nezha_core SHARED
  bpool.c
  heap_vector.c
  gpu.c
  graph.c
  graph_job.c
  graph_binding.c
  graph_transfer.c
  graph_compute.c
  graph_pass.c
  util.c
  surface.c
  load_backend.c)

TARGET_INCLUDE_DIRECTORIES(nezha_core PUBLIC
  include)

FIND_PACKAGE(Vulkan)

IF (Vulkan_FOUND)
  MESSAGE(STATUS "Found Vulkan package in system ${Vulkan_LIBRARY}")
  TARGET_INCLUDE_DIRECTORIES(nezha_core PUBLIC "${Vulkan_INCLUDE_DIRS}")
  TARGET_LINK_LIBRARIES(nezha_core PUBLIC "${Vulkan_LIBRARY}" "glfw")
ENDIF()
