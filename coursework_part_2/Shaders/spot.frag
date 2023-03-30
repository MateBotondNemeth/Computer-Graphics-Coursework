#version 440

// Spot light data
struct spot_light {
  vec4 light_colour;
  vec3 position;
  vec3 direction;
  float constant;
  float linear;
  float quadratic;
  float power;
};

// Material data
struct material {
  vec4 emissive;
  vec4 diffuse_reflection;
  vec4 specular_reflection;
  float shininess;
};

// Spot light being used in the scene
uniform spot_light spot;
// Material of the object being rendered
uniform material mat;
// Position of the eye (camera)
uniform vec3 eye_pos;
// Texture to sample from
uniform sampler2D tex;

// Incoming position
layout(location = 0) in vec3 position;
// Incoming normal
layout(location = 1) in vec3 normal;
// Incoming texture coordinate
layout(location = 2) in vec2 tex_coord;

// Outgoing colour
layout(location = 0) out vec4 colour;

void main() {
  // *********************************
  // Calculate direction to the light
  vec3 direction = normalize(spot.position - position);
  // Calculate distance to light
  float light_distance = distance(spot.position, position);
  // Calculate attenuation value
  float attenuation_factor = spot.constant + spot.linear * light_distance + spot.quadratic * pow(light_distance, 2);
  // Calculate spot light intensity
  float intensity = pow(max(dot(-spot.direction, direction), 0.0), spot.power);
  // Calculate light colour
  vec4 light_colour = (intensity / attenuation_factor) * spot.light_colour;
  // Calculate view direction
  vec3 eye_dir = (eye_pos - position) / length(eye_pos - position);
  // Now use standard phong shading but using calculated light colour and direction
  // - note no ambient
  // Diffuse
  float k = max(dot(normal, direction), 0.0);
  vec4 diffuse = k * (mat.diffuse_reflection * light_colour);
  // Calculate half vector between view_dir and light_dir
  vec3 half_vector = (direction + eye_dir) / length(direction + eye_dir);
  // Calculate k
  k = max(dot(normal, half_vector), 0.0);
  // Calculate specular
  vec4 amb = light_colour * mat.specular_reflection;
  vec4 specular = amb * pow(k, mat.shininess);
  specular.a = 1.0;
  // Sample texture
  vec4 tex_colour = texture(tex, tex_coord);
  // Calculate primary colour component
  vec4 primary = mat.emissive + diffuse;
  // Calculate final colour - remember alpha
  primary.a = 1.0;
  colour = primary * tex_colour + specular;
  // *********************************
}