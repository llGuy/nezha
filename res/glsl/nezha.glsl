#ifndef _UTIL_GLSL_
#define _UTIL_GLSL_

#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_arithmetic : require

#define BEGIN_SEND_DATA() layout (push_constant) uniform _push_constant_ {
#define END_SEND_DATA(name) } name

#define BEGIN_STORAGE_BUFFER(set_n, binding_n, name) layout (set = set_n, binding = binding_n) buffer name {
#define END_STORAGE_BUFFER(name) } name

#endif
