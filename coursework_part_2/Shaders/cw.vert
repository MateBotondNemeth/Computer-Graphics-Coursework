#version 440

// The model matrix
uniform mat4 M;
// The transformation matrix
uniform mat4 MVP;
// The normal matrix
uniform mat3 N;
// The light transformation matrix
uniform mat4 lightMVP[3];
// Eye position
uniform vec3 eye_pos;

// Incoming position
layout(location = 0) in vec3 position;
// Incoming normal
layout(location = 2) in vec3 normal;
// Incoming texture coordinate
layout(location = 10) in vec2 tex_coord_in;

// Outgoing vertex position
layout(location = 0) out vec3 vertex_position;
// Outgoing transformed normal
layout(location = 1) out vec3 transformed_normal;
// Outgoing texture coordinate
layout(location = 2) out vec2 tex_coord_out;
// Outgoing position in light space
layout (location = 3) out vec4 vertex_light[3];
// Outgoing texture coordinate
layout(location = 6) out vec3 sky_tex_coord_out;

void main() {
  // Set position
  gl_Position = MVP * vec4(position, 1);
  // Output other values to fragment shader
  // Position in world space
  vertex_position = (M * vec4(position, 1)).xyz;
  // Normals in world space
  transformed_normal = N * normal;
  // Sky texture coordinate
  sky_tex_coord_out = normalize(reflect(vertex_position - eye_pos, transformed_normal));
  tex_coord_out = tex_coord_in;
  // Transform position into light space
  for(int i = 0; i < 3; i++)
  {
      vertex_light[i] = (lightMVP[i] * vec4(position, 1.0));
  }
}