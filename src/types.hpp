#pragma once

#include <stdint.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

// Primitive
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using b8 = uint8_t;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

// Math
using v2 = glm::vec2;
using v3 = glm::vec3;
using v4 = glm::vec4;

using iv2 = glm::ivec2;
using iv3 = glm::ivec3;
using iv4 = glm::ivec4;

using q4 = glm::quat;

using m4x4 = glm::mat4;
