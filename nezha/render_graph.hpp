#pragma once


/* TODO:
 * - Recycle semaphores/fences/commandbuffers
 * - Make sure that we can add multiple instances of a compute shader. 
 * - Job synchronization
 * - More async stuff
 * - Device memory cleanups! */


#include <set>
#include <array>
#include <vector>
#include <assert.h>
#include "string.hpp"
#include "heap_array.hpp"
#include "dynamic_array.hpp"
#include <vulkan/vulkan.h>

namespace nz
{




}

#define RES(x) (uid_string{     \
  x, sizeof(x),               \
  get_id<crc32<sizeof(x)-2>(x)^0xFFFFFFFF>(nz::render_graph::rdg_name_id_counter)})

#define STG(x) (uid_string{     \
  x, sizeof(x),               \
  get_id<crc32<sizeof(x)-2>(x)^0xFFFFFFFF>(nz::render_graph::stg_name_id_counter)})
