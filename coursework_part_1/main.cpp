/**
 Computer Graphics coursework part 1
 author: Mate Botond Nemeth
*/

#include <glm\glm.hpp>
#include <graphics_framework.h>

using namespace std;
using namespace std::chrono;
using namespace graphics_framework;
using namespace glm;

map<string, mesh> meshes;   // meshes
array<mesh, 3> boxes;   // rotating boxes
mesh skybox;    // skybox
geometry geom;
effect eff; // main effect
effect shadow_eff;  // shadow effect
effect motion_blur; // motion blur effect
effect tex_eff; // basic texture effect
effect grey_eff;    // greyscale
effect compute_eff; // compute shader effect
effect smoke_eff;   // smoke effect
effect sky_eff; // skybox
cubemap cube_map;   // sky cubemap
frame_buffer frames[2]; // motion blur frame buffers
frame_buffer temp_frame;    // frame buffer for motion blur and greyscale
unsigned int current_frame = 0;
geometry screen_quad;   // screen quad for post processing
free_camera free_cam;   // free camera to roam around
target_camera cam;  // basic camera with 4 positions
arc_ball_camera arc_cam;    // arc ball camera
int active_cam_num = 2; // the index pf the currently active camera (target cam - 2, free cam - 0, arc cam - 1)
array <texture, 5> texs;    // the array of textures
texture smoke;  // smoke texture
array <spot_light, 3> spot_lights;  // spot lights
shadow_map shadows[3];  // shadow maps
double cursor_x = 0.0;
double cursor_y = 0.0;
const unsigned int MAX_PARTICLES = 4096;    // Maximum number of particles
vec4 positions[MAX_PARTICLES];  // Particle positions
vec4 velocitys[MAX_PARTICLES];  // Particle velocities
GLuint G_Position_buffer, G_Velocity_buffer;    // Position and velocity buffers
GLuint vao;

//  Hide the cursor and get its position
bool initialise() {
    // *********************************
    // Set input mode - hide the cursor
    glfwSetInputMode(renderer::get_window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    // Capture initial mouse position
    glfwGetCursorPos(renderer::get_window(), &cursor_x, &cursor_y);
    // *********************************
    return true;
}

// Loading the contents of the scene
bool load_content() {

  // Create box geometry for skybox
  skybox = mesh(geometry_builder::create_box());
  // Scale box by 100 - allows a distance
  skybox.get_transform().scale = vec3(100.0f);
  // Load the cubemap  - create array of six filenames +x, -x, +y, -y, +z, -z
  array<string, 6> filenames = { "res/textures/sahara_ft.jpg", "res/textures/sahara_bk.jpg", "res/textures/sahara_up.jpg",
                                "res/textures/sahara_dn.jpg", "res/textures/sahara_rt.jpg", "res/textures/sahara_lf.jpg" };
  // Create cube_map
  cube_map = cubemap(filenames);

  // Load in skybox effect
  sky_eff.add_shader("res/shaders/skybox.vert", GL_VERTEX_SHADER);
  sky_eff.add_shader("res/shaders/skybox.frag", GL_FRAGMENT_SHADER);
  // Build effect
  sky_eff.build();

  // Random number
  default_random_engine rand(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
  //Distribution
  uniform_real_distribution<float> dist;

  smoke = texture("res/textures/smoke.png");

  // Initilise particles
  for (unsigned int i = 0; i < MAX_PARTICLES; ++i) {
      positions[i] = vec4(((3.0f * dist(rand)) - 1.0f), 5.0 * dist(rand), ((3.0f * dist(rand)) - 1.0f), 0.0f);
      velocitys[i] = vec4(0.0f, 2.0f + dist(rand), 0.0f, 0.0f);
  }
  // Load in smoke shaders
  smoke_eff.add_shader("res/shaders/smoke.vert", GL_VERTEX_SHADER);
  smoke_eff.add_shader("res/shaders/smoke.frag", GL_FRAGMENT_SHADER);
  smoke_eff.add_shader("res/shaders/smoke.geom", GL_GEOMETRY_SHADER);

  smoke_eff.build();

  // Load in particle shaders
  compute_eff.add_shader("res/shaders/particle.comp", GL_COMPUTE_SHADER);
  compute_eff.build();

  // a useless vao, but we need it bound or we get errors.
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  // *********************************
   //Generate Position Data buffer
  glGenBuffers(1, &G_Position_buffer);
  // Bind as GL_SHADER_STORAGE_BUFFER
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, G_Position_buffer);
  // Send Data to GPU, use GL_DYNAMIC_DRAW
  glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * MAX_PARTICLES, positions, GL_DYNAMIC_DRAW);
  // Generate Velocity Data buffer
  glGenBuffers(1, &G_Velocity_buffer);
  // Bind as GL_SHADER_STORAGE_BUFFER
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, G_Velocity_buffer);
  // Send Data to GPU, use GL_DYNAMIC_DRAW
  glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * MAX_PARTICLES, velocitys, GL_DYNAMIC_DRAW);
  // *********************************
   //Unbind
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  // Create 2 frame buffers - use screen width and height
  frames[0] = frame_buffer(renderer::get_screen_width(), renderer::get_screen_height());
  frames[1] = frame_buffer(renderer::get_screen_width(), renderer::get_screen_height());

  // Create a temp framebuffer
  temp_frame = frame_buffer(renderer::get_screen_width(), renderer::get_screen_height());
  // Create screen quad
  vector<vec3> positions{ vec3(-1.0f, -1.0f, 0.0f), vec3(1.0f, -1.0f, 0.0f), vec3(-1.0f, 1.0f, 0.0f),
                             vec3(1.0f, 1.0f, 0.0f) };
  vector<vec2> tex_coords{ vec2(0.0, 0.0), vec2(1.0f, 0.0f), vec2(0.0f, 1.0f), vec2(1.0f, 1.0f) };
  screen_quad.add_buffer(positions, BUFFER_INDEXES::POSITION_BUFFER);
  screen_quad.add_buffer(tex_coords, BUFFER_INDEXES::TEXTURE_COORDS_0);
  screen_quad.set_type(GL_TRIANGLE_STRIP);

  // Create plane mesh
  meshes["plane"] = mesh(geometry_builder::create_plane(25, 25, true)); // Generate with subdivision for the texture
  meshes["plane"].get_transform().scale = vec3(4);
  meshes["plane"].get_transform().translate(vec3(-50, 0, 0));

  shadows[0] = shadow_map(renderer::get_screen_width(), renderer::get_screen_height());
  shadows[1] = shadow_map(renderer::get_screen_width(), renderer::get_screen_height());
  shadows[2] = shadow_map(renderer::get_screen_width(), renderer::get_screen_height());
  
  // Create meshes
  meshes["cylinder"] = mesh(geometry_builder::create_cylinder(20, 20));
  meshes["pyramid"] = mesh(geometry_builder::create_pyramid());
  meshes["torus"] = mesh(geometry_builder::create_torus(20, 20, 1.0f, 5.0f));
  meshes["sphere"] = mesh(geometry_builder::create_sphere(20, 20));
  meshes["campfire"] = mesh(geometry("res/models/campfire2.obj"));
  
  boxes[0] = mesh(geometry_builder::create_box(vec3(5)));
  boxes[1] = mesh(geometry_builder::create_box(vec3(5)));
  boxes[2] = mesh(geometry_builder::create_box(vec3(5)));

  // Translations for boxes, the boxes get smaller so they need to be translated by a smaller amount
  boxes[0].get_transform().translate(vec3(-10.0f, 5.0f, 10.0f));
  boxes[1].get_transform().translate(vec3(0.0f, 0.0f, 3.5f));
  boxes[2].get_transform().translate(vec3(0.0f, 2.25f, 2.25f));

  // Set orientation (0 by default)
  boxes[0].get_transform().orientation = vec3(pi<float>());
  boxes[1].get_transform().orientation = vec3(pi<float>());
  boxes[2].get_transform().orientation = vec3(pi<float>());

  // Each box gets half the size (since their model matrices are multiplied each box gets smaller and smaller)
  boxes[0].get_transform().scale = vec3(0.5);
  boxes[1].get_transform().scale = vec3(0.5);
  boxes[2].get_transform().scale = vec3(0.5);

  // Transformations for the meshes
  meshes["pyramid"].get_transform().scale = vec3(10.0f, 10.0f, 10.0f);
  meshes["pyramid"].get_transform().translate(vec3(-40.0f, 2.5f, -35.0f));
  meshes["cylinder"].get_transform().scale = vec3(5.0f, 5.0f, 5.0f);
  meshes["cylinder"].get_transform().translate(vec3(-25.0f, 2.5f, -5.0f));
  meshes["torus"].get_transform().translate(vec3(-25.0f, 10.0f, -5.0f));
  meshes["torus"].get_transform().rotate(vec3(half_pi<float>(), 0.0f, 0.0f));
  meshes["torus"].get_transform().rotate(vec3(half_pi<float>(), 0.0f, 0.0f));   // Have to rotate two times, or manually set orientation (it is 0 for all meshes at generation)
  meshes["sphere"].get_transform().scale = vec3(3.0);
  meshes["sphere"].get_transform().translate(vec3(10.0, 3.0, -20.0));
  meshes["campfire"].get_transform().translate(vec3(0.6f, 0.0f, 0.6f));
  meshes["campfire"].get_transform().scale = vec3(0.4);

  // Set plane material
  meshes["plane"].get_material().set_emissive(vec4(0.0, 0.0, 0.0, 1.0));
  meshes["plane"].get_material().set_specular(vec4(0.1, 0.1, 0.1, 1.0));
  meshes["plane"].get_material().set_shininess(5);
  meshes["plane"].get_material().set_diffuse(vec4(1.0, 1.0, 1.0, 1.0));

  // Red pyramid
  meshes["pyramid"].get_material().set_emissive(vec4(0.0, 0.0, 0.0, 1.0));
  meshes["pyramid"].get_material().set_specular(vec4(1.0, 1.0, 1.0, 1.0));
  meshes["pyramid"].get_material().set_shininess(25);
  meshes["pyramid"].get_material().set_diffuse(vec4(1.0, 0.0, 0.0, 1.0));

  // Yellow cylinder
  meshes["cylinder"].get_material().set_emissive(vec4(0.0, 0.0, 0.0, 1.0));
  meshes["cylinder"].get_material().set_specular(vec4(1.0, 1.0, 1.0, 1.0));
  meshes["cylinder"].get_material().set_shininess(25);
  meshes["cylinder"].get_material().set_diffuse(vec4(1.0, 1.0, 0.0, 1.0));

  // Blue torus
  meshes["torus"].get_material().set_emissive(vec4(0.0, 0.0, 0.0, 1.0));
  meshes["torus"].get_material().set_specular(vec4(1.0, 1.0, 1.0, 1.0));
  meshes["torus"].get_material().set_shininess(25);
  meshes["torus"].get_material().set_diffuse(vec4(0.0, 0.0, 1.0, 1.0));

  // RGB shpere
  meshes["sphere"].get_material().set_emissive(vec4(0.0, 0.0, 0.0, 1.0));
  meshes["sphere"].get_material().set_specular(vec4(1.0, 1.0, 1.0, 1.0));
  meshes["sphere"].get_material().set_shininess(25);
  meshes["sphere"].get_material().set_diffuse(vec4(1.0, 1.0, 0.0, 1.0));

  // Box materials
  boxes[0].get_material().set_emissive(vec4(0.0, 0.0, 0.0, 1.0));
  boxes[0].get_material().set_specular(vec4(1.0, 1.0, 1.0, 1.0));
  boxes[0].get_material().set_shininess(25);
  boxes[0].get_material().set_diffuse(vec4(1.0, 1.0, 1.0, 1.0));

  boxes[1].get_material().set_emissive(vec4(0.0, 0.0, 0.0, 1.0));
  boxes[1].get_material().set_specular(vec4(1.0, 1.0, 1.0, 1.0));
  boxes[1].get_material().set_shininess(25);
  boxes[1].get_material().set_diffuse(vec4(1.0, 1.0, 1.0, 1.0));

  boxes[2].get_material().set_emissive(vec4(0.0, 0.0, 0.0, 1.0));
  boxes[2].get_material().set_specular(vec4(1.0, 1.0, 1.0, 1.0));
  boxes[2].get_material().set_shininess(25);
  boxes[2].get_material().set_diffuse(vec4(1.0, 1.0, 1.0, 1.0));

  // Load texture
  texs[0] = texture("res/textures/checked.gif");
  texs[1] = texture("res/textures/grass.jpg");
  texs[2] = texture("res/textures/stonygrass.jpg");
  texs[3] = texture("res/textures/blend_map2.jpg");
  texs[4] = texture("res/textures/campfire_diff.png");

  // Spot light 0
  // Position (-30, 25, -30)
  spot_lights[0].set_position(vec3(-30.0, 25.0, -30.0));
  // Light colour blue
  spot_lights[0].set_light_colour(vec4(1.0, 1.0, 1.0, 1.0));
  spot_lights[0].set_direction(vec3(1.0, -0.5, -1.0));
  spot_lights[0].set_range(50.0);
  spot_lights[0].set_power(0.7);

  // Spot light 1
  // Position (-25, 10, 30)
  spot_lights[1].set_position(vec3(-15.0, 20.0, -15.0));
  // Light colour red
  spot_lights[1].set_light_colour(vec4(1.0, 1.0, 1.0, 1.0));
  spot_lights[1].set_direction(vec3(1.0, -0.5, 1.0));
  spot_lights[1].set_range(40.0);
  spot_lights[1].set_power(0.7);

  // Spot light 2
  // Position (25, 10, -20)
  spot_lights[2].set_position(vec3(-45.0, 20.0, -60.0));
  // Light colour red
  spot_lights[2].set_light_colour(vec4(1.0, 1.0, 1.0, 1.0));
  spot_lights[2].set_direction(vec3(-1.0, 0.0, 1.0));
  spot_lights[2].set_range(90.0);
  spot_lights[2].set_power(0.7);

  // Load in shaders
  eff.add_shader("res/shaders/cw.vert", GL_VERTEX_SHADER);
  vector<string> frag_shaders{ "res/shaders/cw.frag", "res/shaders/part_shadow.frag", "res/shaders/part_spot.frag"};
  eff.add_shader(frag_shaders, GL_FRAGMENT_SHADER);
  shadow_eff.add_shader("res/shaders/spot.vert", GL_VERTEX_SHADER);
  shadow_eff.add_shader("res/shaders/spot.frag", GL_FRAGMENT_SHADER);
  motion_blur.add_shader("res/shaders/simple_texture.vert", GL_VERTEX_SHADER);
  motion_blur.add_shader("res/shaders/motion_blur.frag", GL_FRAGMENT_SHADER);
  tex_eff.add_shader("res/shaders/simple_texture.vert", GL_VERTEX_SHADER);
  tex_eff.add_shader("res/shaders/simple_texture.frag", GL_FRAGMENT_SHADER);
  grey_eff.add_shader("res/shaders/simple_texture.vert", GL_VERTEX_SHADER);
  grey_eff.add_shader("res/shaders/greyscale.frag", GL_FRAGMENT_SHADER);
  // Build effects
  eff.build();
  shadow_eff.build();
  motion_blur.build();
  tex_eff.build();
  grey_eff.build();

  // Set camera 2 properties
  cam.set_position(vec3(50.0f, 20.0f, 50.0f));
  cam.set_target(vec3(-5.0f, 0.0f, 0.0f));
  cam.set_projection(quarter_pi<float>(), renderer::get_screen_aspect(), 0.1f, 1000.0f);

  // Set camera 0 properties
  free_cam.set_position(vec3(50.0f, 20.0f, 50.0f));
  free_cam.set_target(vec3(-5.0f, 0.0f, 0.0f));
  free_cam.set_projection(quarter_pi<float>(), renderer::get_screen_aspect(), 0.1f, 1000.0f);

  // Set camera 1 properties
  arc_cam.set_target(meshes["pyramid"].get_transform().position + vec3(0.0, 10.0, 0.0));
  arc_cam.set_projection(quarter_pi<float>(), renderer::get_screen_aspect(), 0.1f, 1000.0f);
  arc_cam.move(30.0f);

  return true;
}


bool update(float delta_time) {

    // Particles
    if (delta_time > 10.0f) {
        delta_time = 10.0f;
    }
    renderer::bind(compute_eff);
    glUniform3fv(compute_eff.get_uniform_location("max_dims"), 1, &(vec3(3.0f, 5.0f, 5.0f))[0]);
    glUniform1f(compute_eff.get_uniform_location("delta_time"), delta_time);

  // Flip frame
  current_frame = (current_frame + 1) % 2;

  // Roll ball
  // Translate mesh
  // Multiply rotation (convert to vec4) by mesh transform matrix to convert model space rotation to world space rotation and apply it to mesh (convert to vec3)
  if (glfwGetKey(renderer::get_window(), 'D')) {
      meshes["sphere"].get_transform().translate(vec3(0.1, 0.0, 0.0));
      meshes["sphere"].get_transform().rotate(vec3(vec4(vec3(0.0, 0.0, -pi<float>() * delta_time), 1.0) * meshes["sphere"].get_transform().get_transform_matrix()));
  }
  if (glfwGetKey(renderer::get_window(), 'A')) {
      meshes["sphere"].get_transform().translate(vec3(-0.1, 0.0, 0.0));
      meshes["sphere"].get_transform().rotate(vec3(vec4(vec3(0.0, 0.0, pi<float>() * delta_time), 1.0) * meshes["sphere"].get_transform().get_transform_matrix()));
  }
  if (glfwGetKey(renderer::get_window(), 'W')) {
      meshes["sphere"].get_transform().translate(vec3(0.0, 0.0, -0.1));
      meshes["sphere"].get_transform().rotate(vec3(vec4(vec3(-pi<float>() * delta_time, 0.0, 0.0), 1.0) * meshes["sphere"].get_transform().get_transform_matrix()));
  }
  if (glfwGetKey(renderer::get_window(), 'S')) {
      meshes["sphere"].get_transform().translate(vec3(0.0, 0.0, 0.1));
      meshes["sphere"].get_transform().rotate(vec3(vec4(vec3(pi<float>() * delta_time, 0.0, 0.0), 1.0) * meshes["sphere"].get_transform().get_transform_matrix()));
  }
  
  // rotate Box Zero on Y axis by delta_time
  boxes[0].get_transform().rotate(vec3(0.0f, pi<float>() / 2 * delta_time, 0.0f));
  // rotate Box One on Z axis by delta_time
  boxes[1].get_transform().rotate(vec3(0.0f, 0.0f, pi<float>() / 2 * delta_time));
  // rotate Box Two on Y axis by delta_time
  boxes[2].get_transform().rotate(vec3(0.0f, pi<float>() / 2 * delta_time, 0.0f));

  // RGB Ball
  // Set each color to the absolute value of sin time + offset, the offset provides a different value for R, G, B
  meshes["sphere"].get_material().set_diffuse(vec4(abs(sin(0.0 + glfwGetTime())), abs(sin(1.0 + glfwGetTime())), abs(sin(2.0 + glfwGetTime())), 1.0));
  
  // Get active camera
  if (glfwGetKey(renderer::get_window(), '3') || glfwGetKey(renderer::get_window(), '4') || glfwGetKey(renderer::get_window(), '5') || glfwGetKey(renderer::get_window(), '6')) {
      active_cam_num = 2;
  }

  if (glfwGetKey(renderer::get_window(), '1')) {
      active_cam_num = 0;
  }

  if (glfwGetKey(renderer::get_window(), '2')) {
      active_cam_num = 1;
  }

  // Target camera
  if (active_cam_num == 2) {
      // Changing camera position for target camera
      if (glfwGetKey(renderer::get_window(), '3')) {
          active_cam_num = 2; // Set active camera
          cam.set_position(vec3(50, 20, 50));
      }
      if (glfwGetKey(renderer::get_window(), '4')) {
          active_cam_num = 2; // Set active camera
          cam.set_position(vec3(-50, 10, 50));
      }
      if (glfwGetKey(renderer::get_window(), '5')) {
          active_cam_num = 2; // Set active camera
          cam.set_position(vec3(-50, 10, -50));
      }
      if (glfwGetKey(renderer::get_window(), '6')) {
          active_cam_num = 2; // Set active camera
          cam.set_position(vec3(50, 10, -50));
      }

      cam.update(delta_time);

      skybox.get_transform().position = cam.get_position();
  }

  // Free camera
  if (active_cam_num == 0) {
      // The ratio of pixels to rotation
      static double ratio_width = quarter_pi<float>() / static_cast<float>(renderer::get_screen_width());
      static double ratio_height =
          (quarter_pi<float>() *
              (static_cast<float>(renderer::get_screen_height()) / static_cast<float>(renderer::get_screen_width()))) /
          static_cast<float>(renderer::get_screen_height());

      double current_x;
      double current_y;
      // Get the current cursor position
      glfwGetCursorPos(renderer::get_window(), &current_x, &current_y);
      // Calculate delta of cursor positions from last frame
      double delta_x = current_x - cursor_x;
      double delta_y = current_y - cursor_y;
      // Multiply deltas by ratios - gets actual change in orientation
      delta_x = delta_x * ratio_width;
      delta_y = delta_y * ratio_height;
      // Rotate cameras by delta
      free_cam.rotate(delta_x, -delta_y);
      // Move camera
      if (glfwGetKey(renderer::get_window(), GLFW_KEY_UP)) {
          free_cam.move(vec3(0.0f, 0.0f, 1.0f));
      }
      if (glfwGetKey(renderer::get_window(), GLFW_KEY_DOWN)) {
          free_cam.move(vec3(0.0f, 0.0f, -1.0f));
      }

      if (glfwGetKey(renderer::get_window(), GLFW_KEY_LEFT)) {
          free_cam.move(vec3(-1.0f, 0.0f, 0.0f));
      }
      if (glfwGetKey(renderer::get_window(), GLFW_KEY_RIGHT)) {
          free_cam.move(vec3(1.0f, 0.0f, 0.0f));
      }

      free_cam.update(delta_time);

      skybox.get_transform().position = cam.get_position();

      // Update cursor pos
      glfwGetCursorPos(renderer::get_window(), &cursor_x, &cursor_y);
  }

  if (active_cam_num == 1) {
      // The target object
      static mesh& target_mesh = meshes["pyramid"];

      // The ratio of pixels to rotation - remember the fov
      static const float sh = static_cast<float>(renderer::get_screen_height());
      static const float sw = static_cast<float>(renderer::get_screen_height());
      static const double ratio_width = quarter_pi<float>() / sw;
      static const double ratio_height = (quarter_pi<float>() * (sh / sw)) / sh;

      double current_x;
      double current_y;
      // Get the current cursor position
      glfwGetCursorPos(renderer::get_window(), &current_x, &current_y);
      // Calculate delta of cursor positions from last frame
      double delta_x = current_x - cursor_x;
      double delta_y = current_y - cursor_y;
      // Multiply deltas by ratios and delta_time - gets actual change in orientation
      delta_x = delta_x * ratio_width;
      delta_y = delta_y * ratio_height;
      // Rotate cameras by delta
      arc_cam.rotate(delta_y, delta_x);

      // UP and DOWN to change camera distance
      if (glfwGetKey(renderer::get_window(), GLFW_KEY_UP)) {
          arc_cam.move(10.0f * delta_time);
      }
      if (glfwGetKey(renderer::get_window(), GLFW_KEY_DOWN)) {
          arc_cam.move(-10.0f * delta_time);
      }

      arc_cam.update(delta_time);

      skybox.get_transform().position = cam.get_position();

      // Update cursor pos
      cursor_x = current_x;
      cursor_y = current_y;
  }

  // Update the shadow map light_position from the spot light
  shadows[0].light_position = spot_lights[0].get_position();
  // do the same for light_dir property
  shadows[0].light_dir = spot_lights[0].get_direction();

  // Update the shadow map light_position from the spot light
  shadows[1].light_position = spot_lights[1].get_position();
  // do the same for light_dir property
  shadows[1].light_dir = spot_lights[1].get_direction();

  // Update the shadow map light_position from the spot light
  shadows[2].light_position = spot_lights[2].get_position();
  // do the same for light_dir property
  shadows[2].light_dir = spot_lights[2].get_direction();

  return true;
}

// Render scene
bool render() {

    // Render shadows
    texture shadow_texs[3];

    for (int i = 0; i < 3; i++)
    {
        // *********************************
        // Set render target to shadow map
        renderer::set_render_target(shadows[i]);
        // Clear depth buffer bit
        glClear(GL_DEPTH_BUFFER_BIT);
        // Set face cull mode to front
        glCullFace(GL_FRONT);
        // *********************************

        // Light projection matrix
        mat4 LightProjectionMat = perspective<float>(90.f, renderer::get_screen_aspect(), 0.1f, 1000.f);

        // Bind shader
        renderer::bind(shadow_eff);
        // Render meshes
        for (auto& e : meshes) {
            auto m = e.second;
            // Create MVP matrix
            auto M = m.get_transform().get_transform_matrix();
            // *********************************
            // View matrix taken from shadow map
            auto V = shadows[i].get_view();
            // *********************************
            auto MVP = LightProjectionMat * V * M;
            // Set MVP matrix uniform
            glUniformMatrix4fv(shadow_eff.get_uniform_location("MVP"), // Location of uniform
                1,                                      // Number of values - 1 mat4
                GL_FALSE,                               // Transpose the matrix?
                value_ptr(MVP));                        // Pointer to matrix data
            // Render mesh
            renderer::render(m);
            shadow_texs[i] = shadows[i].buffer->get_depth();
        }

        for (int e = 0; e < 1; e++) {
            auto m = boxes[e];
            // Create MVP matrix
            auto M = m.get_transform().get_transform_matrix();
            // *********************************
            // View matrix taken from shadow map
            auto V = shadows[i].get_view();
            // *********************************
            auto MVP = LightProjectionMat * V * M;
            // Set MVP matrix uniform
            glUniformMatrix4fv(shadow_eff.get_uniform_location("MVP"), // Location of uniform
                1,                                      // Number of values - 1 mat4
                GL_FALSE,                               // Transpose the matrix?
                value_ptr(MVP));                        // Pointer to matrix data
            // Render mesh
            renderer::render(m);
            shadow_texs[i] = shadows[i].buffer->get_depth();
        }
    }
    // *********************************
    // Set render target back to the screen
    renderer::set_render_target();
    // Set face cull mode to back
    glCullFace(GL_BACK);

    // Motion blur for cam 0 or greyscale for cam 1
    if (active_cam_num == 0 || active_cam_num == 1)
    {
        // !!!!!!!!!!!!!!! FIRST PASS !!!!!!!!!!!!!!!!
        // *********************************
        // Set render target to temp frame
        renderer::set_render_target(temp_frame);
        // Clear frame
        renderer::clear();
    }

    // Render skybox
    // Disable depth test and depth mask
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glCullFace(GL_FRONT);
    // Bind skybox effect
    renderer::bind(sky_eff);
    // Calculate MVP for the skybox
    auto M = skybox.get_transform().get_transform_matrix();
    auto V = cam.get_view();
    auto P = cam.get_projection();
    auto MVP = P * V * M;
    // Set MVP matrix uniform
    glUniformMatrix4fv(sky_eff.get_uniform_location("MVP"),
        1,
        GL_FALSE,
        value_ptr(MVP));
    // Bind cubemap to TU 0
    renderer::bind(cube_map, 0);
    // Set cubemap uniform
    glUniform1i(sky_eff.get_uniform_location("cubemap"), 0);
    // Render skybox
    renderer::render(skybox);
    // Enable depth test and depth mask
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glCullFace(GL_BACK);

    // Render meshes
    for (auto& e : meshes) {
        auto m = e.second;
        // Bind effect
        renderer::bind(eff);
        // Create MVP matrix
        auto M = m.get_transform().get_transform_matrix();
        if (active_cam_num == 0) {
            auto V = free_cam.get_view();
            auto P = free_cam.get_projection();

            auto MVP = P * V * M;
            // Set MVP matrix uniform
            glUniformMatrix4fv(eff.get_uniform_location("MVP"), // Location of uniform
                1,                               // Number of values - 1 mat4
                GL_FALSE,                        // Transpose the matrix?
                value_ptr(MVP));                 // Pointer to matrix data
        }
        if (active_cam_num == 2) {
            auto V = cam.get_view();
            auto P = cam.get_projection();

            auto MVP = P * V * M;
            // Set MVP matrix uniform
            glUniformMatrix4fv(eff.get_uniform_location("MVP"), // Location of uniform
                1,                               // Number of values - 1 mat4
                GL_FALSE,                        // Transpose the matrix?
                value_ptr(MVP));                 // Pointer to matrix data
        }
        if (active_cam_num == 1) {
            auto V = arc_cam.get_view();
            auto P = arc_cam.get_projection();

            auto MVP = P * V * M;
            // Set MVP matrix uniform
            glUniformMatrix4fv(eff.get_uniform_location("MVP"), // Location of uniform
                1,                               // Number of values - 1 mat4
                GL_FALSE,                        // Transpose the matrix?
                value_ptr(MVP));                 // Pointer to matrix data
        }

        // Set M matrix uniform
        glUniformMatrix4fv(eff.get_uniform_location("M"), 1, GL_FALSE, value_ptr(M));
        // Set N matrix uniform - remember - 3x3 matrix
        glUniformMatrix3fv(eff.get_uniform_location("N"), 1, GL_FALSE, value_ptr(mat3(M)));
        // Bind material
        glUniform4fv(eff.get_uniform_location("mat.diffuse_reflection"), 1, value_ptr(m.get_material().get_diffuse()));     // Set diffuse
        glUniform4fv(eff.get_uniform_location("mat.specular_reflection"), 1, value_ptr(m.get_material().get_specular()));   // Set specular
        glUniform4fv(eff.get_uniform_location("mat.emissive"), 1, value_ptr(m.get_material().get_emissive()));              // Set emissive
        glUniform1f(eff.get_uniform_location("mat.shininess"), m.get_material().get_shininess());                           // Set shininess
        // Bind light
        renderer::bind(spot_lights[0], "spot_lights[0]");
        // Bind light
        renderer::bind(spot_lights[1], "spot_lights[1]");
        // Bind light
        renderer::bind(spot_lights[2], "spot_lights[2]");
        // Bind texture
        renderer::bind(texs[0], 0);
        renderer::bind(texs[1], 1);
        renderer::bind(texs[2], 2);
        renderer::bind(texs[3], 3);
        renderer::bind(texs[4], 4);
        if (!e.first.compare("campfire")) {
            glUniform1i(eff.get_uniform_location("tex1"), 4);
            glUniform1i(eff.get_uniform_location("tex2"), 4);
            glUniform1i(eff.get_uniform_location("blend"), 3);
        }
        if (!e.first.compare("plane")) {
            // Set tex uniform
            glUniform1i(eff.get_uniform_location("tex1"), 1);
            glUniform1i(eff.get_uniform_location("tex2"), 2);
            glUniform1i(eff.get_uniform_location("blend"), 3);
        }else
        {
            // Set tex uniform
            glUniform1i(eff.get_uniform_location("tex1"), 0);
            glUniform1i(eff.get_uniform_location("tex2"), 0);
            glUniform1i(eff.get_uniform_location("blend"), 3);
        }
        // Set eye position- Get this from active camera
        if (active_cam_num == 0) {
            glUniform3fv(eff.get_uniform_location("eye_pos"), 1, value_ptr(free_cam.get_position()));
        }
        if (active_cam_num == 1) {
            glUniform3fv(eff.get_uniform_location("eye_pos"), 1, value_ptr(arc_cam.get_position()));
        }
        else {
            glUniform3fv(eff.get_uniform_location("eye_pos"), 1, value_ptr(cam.get_position()));
        }

        // viewmatrix from the shadow map
        auto lightV0 = shadows[0].get_view();
        // Multiply together with LightProjectionMat
        mat4 LightProjectionMat = perspective<float>(90.f, renderer::get_screen_aspect(), 0.1f, 1000.f);
        auto lightMVP0 = LightProjectionMat * lightV0 * M;
        // Set uniform
        glUniformMatrix4fv(eff.get_uniform_location("lightMVP[0]"), 1, GL_FALSE, value_ptr(lightMVP0));
        auto lightV1 = shadows[1].get_view();
        auto lightMVP1 = LightProjectionMat * lightV1 * M;
        glUniformMatrix4fv(eff.get_uniform_location("lightMVP[1]"), 1, GL_FALSE, value_ptr(lightMVP1));
        auto lightV2 = shadows[2].get_view();
        auto lightMVP2 = LightProjectionMat * lightV2 * M;
        glUniformMatrix4fv(eff.get_uniform_location("lightMVP[2]"), 1, GL_FALSE, value_ptr(lightMVP2));

        // Bind shadow map texture - use texture unit 1
        renderer::bind(shadow_texs[0], 5);
        renderer::bind(shadow_texs[1], 6);
        renderer::bind(shadow_texs[2], 7);
        // Set the shadow_map uniform
        glUniform1i(eff.get_uniform_location("shadow_map[0]"), 5);
        glUniform1i(eff.get_uniform_location("shadow_map[1]"), 6);
        glUniform1i(eff.get_uniform_location("shadow_map[2]"), 7);


        // Render mesh
        renderer::render(m);
    }

    // Render boxes
    for (size_t i = 0; i < boxes.size(); i++) {
        auto m = boxes[i];
        // Bind effect
        renderer::bind(eff);
        // Create MVP matrix
        auto M = m.get_transform().get_transform_matrix();
        if (active_cam_num == 0) {
            auto V = free_cam.get_view();
            auto P = free_cam.get_projection();

            for (size_t j = i; j > 0; j--) {
               M = boxes[j - 1].get_transform().get_transform_matrix() * M;
            }

            auto MVP = P * V * M;
            // Set MVP matrix uniform
            glUniformMatrix4fv(eff.get_uniform_location("MVP"), // Location of uniform
                1,                               // Number of values - 1 mat4
                GL_FALSE,                        // Transpose the matrix?
                value_ptr(MVP));                 // Pointer to matrix data
        }
        if (active_cam_num == 2) {
            auto V = cam.get_view();
            auto P = cam.get_projection();

            for (size_t j = i; j > 0; j--) {
                M = boxes[j - 1].get_transform().get_transform_matrix() * M;
            }

            auto MVP = P * V * M;
            // Set MVP matrix uniform
            glUniformMatrix4fv(eff.get_uniform_location("MVP"), // Location of uniform
                1,                               // Number of values - 1 mat4
                GL_FALSE,                        // Transpose the matrix?
                value_ptr(MVP));                 // Pointer to matrix data
        }
        if (active_cam_num == 1) {
            auto V = arc_cam.get_view();
            auto P = arc_cam.get_projection();

            for (size_t j = i; j > 0; j--) {
                M = boxes[j - 1].get_transform().get_transform_matrix() * M;
            }

            auto MVP = P * V * M;
            // Set MVP matrix uniform
            glUniformMatrix4fv(eff.get_uniform_location("MVP"), // Location of uniform
                1,                               // Number of values - 1 mat4
                GL_FALSE,                        // Transpose the matrix?
                value_ptr(MVP));                 // Pointer to matrix data
        }

        // Set M matrix uniform
        glUniformMatrix4fv(eff.get_uniform_location("M"), 1, GL_FALSE, value_ptr(M));
        // Set N matrix uniform - remember - 3x3 matrix
        glUniformMatrix3fv(eff.get_uniform_location("N"), 1, GL_FALSE, value_ptr(mat3(M)));
        // Bind material
        glUniform4fv(eff.get_uniform_location("mat.diffuse_reflection"), 1, value_ptr(m.get_material().get_diffuse()));     // Set diffuse
        glUniform4fv(eff.get_uniform_location("mat.specular_reflection"), 1, value_ptr(m.get_material().get_specular()));   // Set specular
        glUniform4fv(eff.get_uniform_location("mat.emissive"), 1, value_ptr(m.get_material().get_emissive()));              // Set emissive
        glUniform1f(eff.get_uniform_location("mat.shininess"), m.get_material().get_shininess());                           // Set shininess
        // Bind light
        renderer::bind(spot_lights[0], "spot_lights[0]");
        // Bind light
        renderer::bind(spot_lights[1], "spot_lights[1]");
        // Bind light
        renderer::bind(spot_lights[2], "spot_lights[2]");
        // Bind texture
        renderer::bind(texs[0], 0);
        renderer::bind(texs[1], 1);
        renderer::bind(texs[2], 2);
        renderer::bind(texs[3], 3);
        
        // Set tex uniform
        glUniform1i(eff.get_uniform_location("tex1"), 0);
        glUniform1i(eff.get_uniform_location("tex2"), 0);
        glUniform1i(eff.get_uniform_location("blend"), 3);
       
        // Set eye position- Get this from active camera
        if (active_cam_num == 0) {
            glUniform3fv(eff.get_uniform_location("eye_pos"), 1, value_ptr(free_cam.get_position()));
        }
        if (active_cam_num == 1) {
            glUniform3fv(eff.get_uniform_location("eye_pos"), 1, value_ptr(arc_cam.get_position()));
        }
        else {
            glUniform3fv(eff.get_uniform_location("eye_pos"), 1, value_ptr(cam.get_position()));
        }

        // viewmatrix from the shadow map
        auto lightV0 = shadows[0].get_view();
        // Multiply together with LightProjectionMat
        mat4 LightProjectionMat = perspective<float>(90.f, renderer::get_screen_aspect(), 0.1f, 1000.f);
        auto lightMVP0 = LightProjectionMat * lightV0 * M;
        // Set uniform
        glUniformMatrix4fv(eff.get_uniform_location("lightMVP[0]"), 1, GL_FALSE, value_ptr(lightMVP0));
        auto lightV1 = shadows[1].get_view();
        auto lightMVP1 = LightProjectionMat * lightV1 * M;
        glUniformMatrix4fv(eff.get_uniform_location("lightMVP[1]"), 1, GL_FALSE, value_ptr(lightMVP1));
        auto lightV2 = shadows[2].get_view();
        auto lightMVP2 = LightProjectionMat * lightV2 * M;
        glUniformMatrix4fv(eff.get_uniform_location("lightMVP[2]"), 1, GL_FALSE, value_ptr(lightMVP2));

        // Bind shadow map texture - use texture unit 1
        renderer::bind(shadow_texs[0], 4);
        renderer::bind(shadow_texs[1], 5);
        renderer::bind(shadow_texs[2], 6);
        // Set the shadow_map uniform
        glUniform1i(eff.get_uniform_location("shadow_map[0]"), 4);
        glUniform1i(eff.get_uniform_location("shadow_map[1]"), 5);
        glUniform1i(eff.get_uniform_location("shadow_map[2]"), 6);


        // Render mesh
        renderer::render(m);
    }

    // Render smoke
    // Bind Compute Shader
    renderer::bind(compute_eff);
    // Bind data as SSBO
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, G_Position_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, G_Velocity_buffer);
    // Dispatch
    glDispatchCompute(MAX_PARTICLES / 128, 1, 1);
    // Sync, wait for completion
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // *********************************
    // Bind render effect
    renderer::bind(smoke_eff);
    // Create MV matrix
    if (active_cam_num == 0) {
        auto V = free_cam.get_view();
        auto P = free_cam.get_projection();
        // Set MV, and P matrix uniforms seperatly
        glUniformMatrix4fv(smoke_eff.get_uniform_location("MV"), 1, GL_FALSE, value_ptr(V));
        glUniformMatrix4fv(smoke_eff.get_uniform_location("P"), 1, GL_FALSE, value_ptr(P));
    }
    // Create MV matrix
    if (active_cam_num == 1) {
        auto V = arc_cam.get_view();
        auto P = arc_cam.get_projection();
        // Set MV, and P matrix uniforms seperatly
        glUniformMatrix4fv(smoke_eff.get_uniform_location("MV"), 1, GL_FALSE, value_ptr(V));
        glUniformMatrix4fv(smoke_eff.get_uniform_location("P"), 1, GL_FALSE, value_ptr(P));
    }
    // Create MV matrix
    if (active_cam_num == 2) {
        auto V = cam.get_view();
        auto P = cam.get_projection();
        // Set MV, and P matrix uniforms seperatly
        glUniformMatrix4fv(smoke_eff.get_uniform_location("MV"), 1, GL_FALSE, value_ptr(V));
        glUniformMatrix4fv(smoke_eff.get_uniform_location("P"), 1, GL_FALSE, value_ptr(P));
    }
    // Set the colour uniform
    glUniform4fv(smoke_eff.get_uniform_location("colour"), 1, value_ptr(vec4(1.0)));
    // Set point_size size uniform to .1f
    glUniform1f(smoke_eff.get_uniform_location("point_size"), 0.5f);
    // Bind particle texture
    renderer::bind(smoke, 8);
    glUniform1i(smoke_eff.get_uniform_location("tex"), 8);
    // *********************************

    // Bind position buffer as GL_ARRAY_BUFFER
    glBindBuffer(GL_ARRAY_BUFFER, G_Position_buffer);
    // Setup vertex format
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);
    // Enable Blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Disable Depth Mask
    glDepthMask(GL_FALSE);
    // Render
    glDrawArrays(GL_POINTS, 0, MAX_PARTICLES);
    // Tidy up, enable depth mask
    glDepthMask(GL_TRUE);
    // Disable Blend
    glDisable(GL_BLEND);
    // Unbind all arrays
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);

    if (active_cam_num == 0)
    {
        // !!!!!!!!!!!!!!! SECOND PASS !!!!!!!!!!!!!!!!
        // *********************************
        // Set render target to current frame
        renderer::set_render_target(frames[current_frame]);
        // Clear frame
        renderer::clear();
        // Bind motion blur effect
        renderer::bind(motion_blur);
        // MVP is now the identity matrix
        mat4 MVP = mat4(1.0);
        // Set MVP matrix uniform
        glUniformMatrix4fv(motion_blur.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
        // Bind tempframe to TU 0.
        renderer::bind(temp_frame.get_frame(), 0);
        // Bind frames[(current_frame + 1) % 2] to TU 1.
        renderer::bind(frames[(current_frame + 1) % 2].get_frame(), 1);
        // Set tex uniforms
        glUniform1i(motion_blur.get_uniform_location("tex"), 1);
        glUniform1i(motion_blur.get_uniform_location("previous_frame"), 0);
        // Set blend factor (0.9f)
        glUniform1f(motion_blur.get_uniform_location("blend_factor"), 0.2);
        // Render screen quad
        renderer::render(screen_quad);

        // !!!!!!!!!!!!!!! SCREEN PASS !!!!!!!!!!!!!!!!

        // Set render target back to the screen
        renderer::set_render_target();

        // Set MVP matrix uniform
        MVP = mat4(1.0);
        // Bind texture from frame buffer
        renderer::bind(frames[current_frame].get_frame(), 2);
        // Set the uniform
        glUniform1i(motion_blur.get_uniform_location("tex"), 2);
        // Render the screen quad
        renderer::render(screen_quad);
    }

    if (active_cam_num == 1)
    {
        // Set render target back to the screen
        renderer::set_render_target();
        // Bind Tex effect
        renderer::bind(grey_eff);
        // MVP is now the identity matrix
        auto MVP = mat4(1.0f);
        // Set MVP matrix uniform
        glUniformMatrix4fv(grey_eff.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
        // Bind texture from frame buffer
        renderer::bind(temp_frame.get_frame(), 3);
        // Set the tex uniform
        glUniform1i(grey_eff.get_uniform_location("tex"), 3);
        // Render the screen quad
        renderer::render(screen_quad);
    }

  return true;
}

void main() {
  // Create application
  app application("Graphics Coursework");
  // Set load content, update and render methods
  application.set_load_content(load_content);
  application.set_initialise(initialise);
  application.set_update(update);
  application.set_render(render);
  // Run application
  application.run();
}