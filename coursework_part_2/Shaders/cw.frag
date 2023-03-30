#version 440

#ifndef SPOT_LIGHT
#define SPOT_LIGHT
struct spot_light
{
	vec4 light_colour;
	vec3 position;
	vec3 direction;
	float constant;
	float linear;
	float quadratic;
	float power;
};
#endif

// Material data
#ifndef MATERIAL
#define MATERIAL
struct material
{
	vec4 emissive;
	vec4 diffuse_reflection;
	vec4 specular_reflection;
	float shininess;
};
#endif

float calculate_shadow(in sampler2D shadow_map, in vec4 light_space_pos);
vec4 calculate_spot(in spot_light spot, in material mat, in vec3 position, in vec3 normal, in vec3 view_dir,
                    in vec4 tex_colour);

// Calculate point light
vec4 calculate_spot_light(spot_light spot, material mat, vec3 position, vec3 normal, vec3 eye_pos, vec4 tex_colour){
	vec4 final_colour = vec4(0.0, 0.0, 0.0, 1.0);

	normal = normalize(normal);
	// Direction to the light
	vec3 direction = normalize(spot.position - position);
	// Distance to light
	float light_distance = distance(spot.position, position);
	// Attenuation value
	float attenuation_factor = spot.constant + spot.linear * light_distance + spot.quadratic * pow(light_distance, 2);
	// Spot light intensity
	float intensity = pow(max(dot(-spot.direction, direction), 0.0), spot.power);
	// Light colour
	vec4 light_colour = (intensity / attenuation_factor) * spot.light_colour;
	// View direction
	vec3 eye_dir = (eye_pos - position) / length(eye_pos - position);
	// Diffuse
	float k = max(dot(normal, direction), 0.0);
	vec4 diffuse = k * (mat.diffuse_reflection * light_colour);
	// Half vector between view_dir and light_dir
	vec3 half_vector = (direction + eye_dir) / length(direction + eye_dir);
	// K
	k = max(dot(normal, half_vector), 0.0);
	// Specular
	vec4 amb = light_colour * mat.specular_reflection;
	vec4 specular = amb * pow(k, mat.shininess);
	specular.a = 1.0;
	
	// Primary colour component
	vec4 primary = mat.emissive + diffuse;
	// Final colour - remember alpha
	primary.a = 1.0;
	final_colour = primary * tex_colour + specular;

	return final_colour;
}

// Spot lights for the scene
uniform spot_light spot_lights[3];
// Material for the object
uniform material mat;
// Eye position
uniform vec3 eye_pos;
// Textures
uniform sampler2D tex1;
uniform sampler2D tex2;
uniform sampler2D blend;
// Shadow map to sample from
uniform sampler2D shadow_map[3];
// Cubemap data
uniform samplerCube cubemap;

// Incoming position
layout(location = 0) in vec3 position;
// Incoming normal
layout(location = 1) in vec3 normal;
// Incoming texture coordinate
layout(location = 2) in vec2 tex_coord;
// Incoming light space position
layout(location = 3) in vec4 light_space_pos[3];
// Incoming sky texture coordinate
layout(location = 6) in vec3 sky_tex_coord;

// Outgoing colour
layout(location = 0) out vec4 colour;

void main() {
  // Sample texture
  vec4 col1 = texture(tex1, tex_coord);
  vec4 col2 = texture(tex2, tex_coord);
  vec4 blend_map = texture(blend, tex_coord);
  vec4 tex_colour = mix(col1, col2, blend_map[0]);

  vec4 sky_tex = texture(cubemap, sky_tex_coord);

  colour = vec4(0.0, 0.0, 0.0, 0.0);

  // Ambient light intensity
  vec4 ambient_intensity = vec4(0.08, 0.08, 0.08, 1.0);
  
    // Calculate view direction, normalize it
  vec3 view_dir = (eye_pos - position) / length(eye_pos - position);

  // Calculate shade factor
  float shade_factor[3];
  for(int i = 0; i < 3; i++)
  {
    shade_factor[i] = calculate_shadow(shadow_map[i], light_space_pos[i]);
  }
  
  // Spot lights
  for(int i = 0; i < 3; i++){
	colour += calculate_spot_light(spot_lights[i], mat, position, normal, eye_pos, tex_colour) * shade_factor[i];
  }

  // Ambient light
  colour += ambient_intensity * tex_colour * mat.diffuse_reflection; 
  // Skybox light
  colour += sky_tex * vec4(0.05, 0.05, 0.05, 1.0) * tex_colour * mat.diffuse_reflection;
}