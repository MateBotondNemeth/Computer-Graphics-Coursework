#version 450 core
// This shader requires direction.frag and normal_map.frag

// Directional light structure
#ifndef DIRECTIONAL_LIGHT
#define DIRECTIONAL_LIGHT
struct directional_light {
  vec4 ambient_intensity;
  vec4 light_colour;
  vec3 light_dir;
};
#endif

// A material structure
#ifndef MATERIAL
#define MATERIAL
struct material {
  vec4 emissive;
  vec4 diffuse_reflection;
  vec4 specular_reflection;
  float shininess;
};
#endif


vec3 calc_normal(in vec3 normal, in vec3 tangent, in vec3 binormal, in sampler2D normal_map, in vec2 tex_coord)
{
	// *********************************
	// Ensure normal, tangent and binormal are unit length (normalized)
	tangent = normalize(tangent);
	binormal = normalize(binormal);
	normal = normalize(normal);
	// Sample normal from normal map
	vec3 sampled_normal = texture(normal_map, tex_coord).xyz;
	// *********************************
	// Transform components to range [0, 1]
	sampled_normal = 2.0 * sampled_normal - vec3(1.0, 1.0, 1.0);
	// Generate TBN matrix
	mat3 TBN = mat3(tangent, binormal, normal);
	// Return sampled normal transformed by TBN
	return normalize(TBN * sampled_normal);
}

// Calculates the shadow factor of a vertex
float calculate_shadow(in sampler2D shadow_map, in vec4 light_space_pos) {
  // Get light screen space coordinate
  vec3 proj_coords = light_space_pos.xyz / light_space_pos.w;
  // Use this to calculate texture coordinates for shadow map
  vec2 shadow_tex_coords;
  // This is a bias calculation to convert to texture space
  shadow_tex_coords.x = 0.5 * proj_coords.x + 0.5;
  shadow_tex_coords.y = 0.5 * proj_coords.y + 0.5;
  // Check shadow coord is in range
  if (shadow_tex_coords.x < 0 || shadow_tex_coords.x > 1 || shadow_tex_coords.y < 0 || shadow_tex_coords.y > 1) {
    return 1.0;
  }
  float z = 0.5 * proj_coords.z + 0.5;
  // *********************************
  // Now sample the shadow map, return only first component (.x/.r)
  float depth = texture(shadow_map, shadow_tex_coords).x;
  // *********************************
  // Check if depth is in range.  Add a slight epsilon for precision
  if (depth == 0.0) {
    return 1.0;
  } else if (depth < z + 0.001) {
    return 0.5;
  } else {
    return 1.0;
  }
}


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
// Texture to sample from
uniform sampler2D tex;
// Normal map to sample from
uniform sampler2D normal_map;
// Shadow map to sample from
uniform sampler2D shadow_map[3];
// Cubemap data
uniform samplerCube cubemap;

// Incoming vertex position
layout(location = 0) in vec3 position;
// Incoming texture coordinate
layout(location = 1) in vec2 tex_coord;
// Incoming normal
layout(location = 2) in vec3 normal;
// Incoming tangent
layout(location = 7) in vec3 tangent;
// Incoming binormal
layout(location = 8) in vec3 binormal;
// Incoming light space position
layout(location = 3) in vec4 light_space_pos[3];
// Incoming sky texture coordinate
layout(location = 6) in vec3 sky_tex_coord;

// Outgoing colour
layout(location = 0) out vec4 colour;

void main() {
  // *********************************
  // Sample texture
  vec4 tex_colour = texture(tex, tex_coord);
  // Calculate view direction
  vec3 view_dir = (eye_pos - position) / length(eye_pos - position);
  // Calculate normal from normal map
  vec3 normal1 = calc_normal(normal, tangent, binormal, normal_map, tex_coord);
  
  vec4 sky_tex = texture(cubemap, sky_tex_coord);
  // Ambient light intensity
  vec4 ambient_intensity = vec4(0.12, 0.12, 0.12, 1.0);
  // Calculate directional light
   colour = vec4(0.0f);
   // Spot lights
  for(int i = 0; i < 3; i++){
	colour += calculate_spot_light(spot_lights[i], mat, position, normal1, eye_pos, tex_colour) * calculate_shadow(shadow_map[i], light_space_pos[i]);
  }
  // Ambient light
  colour += ambient_intensity * mat.diffuse_reflection; 
  // Skybox light
  colour += sky_tex * vec4(0.02, 0.02, 0.02, 1.0);
   colour.a = 1.0f;
  // *********************************
}