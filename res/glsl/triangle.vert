#version 450

layout (location = 0) out vec3 out_color;

const vec2 positions[] = vec2[]
(
  vec2(-0.5f, -0.5f),
  vec2(0.0f, 0.5f),
  vec2(0.5f, -0.5f)
);

const vec3 colors[] = vec3[]
(
  vec3(1.0f, 0.0f, 0.0f),
  vec3(0.0f, 1.0f, 0.0f),
  vec3(0.0f, 0.0f, 1.0f)
);

void main() 
{
  gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
  gl_Position.y *= -1.0f;
  out_color = colors[gl_VertexIndex];
}
