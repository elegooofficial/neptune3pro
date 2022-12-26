#pragma once

#include <vector>
#include <array>

#include "hardware/print_bed.h"
#include "hardware/bed_probe.h"

#include <gl.h>
#include <SDL2/SDL.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "window.h"
#include "user_interface.h"

constexpr glm::ivec2 build_plate_dimension{X_BED_SIZE, Y_BED_SIZE};
constexpr glm::ivec2 build_plate_offset{X_MIN_POS, Y_MIN_POS};

using millisec = std::chrono::duration<float, std::milli>;

typedef enum t_attrib_id
{
    attrib_position,
    attrib_normal,
    attrib_color
} t_attrib_id;

struct cp_vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec4 color;
};

class PerspectiveCamera {
public:
  PerspectiveCamera() = default;
  PerspectiveCamera(glm::vec3 position,
                    glm::vec3 rotation,
                    glm::vec3 world_up,
                    float aspect_ratio,
                    float fov,
                    float clip_near,
                    float clip_far
                    ):position{position},
                    rotation{rotation},
                    world_up{world_up},
                    aspect_ratio{aspect_ratio},
                    fov{fov},
                    clip_near{clip_near},
                    clip_far{clip_far},
                    speed{50.0f},
                    front{0.0f, 0.0f, -1.0f} {
    generate();
  }

  void generate() {
    proj = glm::perspective(fov, aspect_ratio, clip_near, clip_far);
    update_view();
  }

  void update_view() {
    direction = glm::vec3(  cos(glm::radians(rotation.y)) * sin(glm::radians(rotation.x)),
                            sin(glm::radians(rotation.y)),
                            cos(glm::radians(rotation.y)) * cos(glm::radians(rotation.x)));

    right = glm::vec3(sin(glm::radians(rotation.x) - (glm::pi<float>() / 2.0)), 0, cos(glm::radians(rotation.x) - (glm::pi<float>() / 2.0)));
    up = glm::cross(right, direction);
    view = glm::lookAt(position, position + direction, up);
  }

  void update_aspect_ratio(float ar) {
    aspect_ratio = ar;
    proj = glm::perspective(fov, aspect_ratio, clip_near, clip_far);
  }

  void update_direction() {
  }

  float* proj_ptr() { return glm::value_ptr(proj); }
  float* view_ptr() { return glm::value_ptr(view); }

  // view
  glm::vec3 position;
  glm::vec3 rotation;
  glm::vec3 focal_point;
  glm::vec3 world_up;
  glm::vec3 right;
  glm::vec3 direction;
  glm::vec3 up;

  // projection
  float aspect_ratio;
  float fov;
  float clip_near;
  float clip_far;
  glm::mat4 view;
  glm::mat4 proj;
  float speed;
  glm::vec3 front;
};

class ShaderProgram {
public:
  static GLuint loadProgram(const char* vertex_string, const char* fragment_string, const char* geometry_string = nullptr) {
    GLuint vertex_shader = 0, fragment_shader = 0, geometry_shader = 0;
    if (vertex_string != nullptr) {
      vertex_shader = loadShader(GL_VERTEX_SHADER, vertex_string);
    }
    if (fragment_string != nullptr) {
      fragment_shader = loadShader(GL_FRAGMENT_SHADER, fragment_string);
    }
    if (geometry_string != nullptr) {
      geometry_shader = loadShader(GL_GEOMETRY_SHADER, geometry_string);
    }

    GLuint shader_program = glCreateProgram();
    glAttachShader( shader_program, vertex_shader );
    glAttachShader( shader_program, fragment_shader );
    if (geometry_shader) glAttachShader( shader_program, geometry_shader );

    glBindAttribLocation(shader_program, attrib_position, "i_position");
    glBindAttribLocation(shader_program, attrib_color, "i_color");
    glLinkProgram(shader_program );
    glUseProgram(shader_program );

    if (vertex_shader) glDeleteShader(vertex_shader);
    if (fragment_shader) glDeleteShader(fragment_shader);
    if (geometry_shader) glDeleteShader(geometry_shader);

    return shader_program;
  }
  static GLuint loadShader(GLuint shader_type, const char* shader_string) {
    GLuint shader_id = glCreateShader(shader_type);;
    int length = strlen(shader_string);
    glShaderSource(shader_id, 1, ( const GLchar ** )&shader_string, &length);
    glCompileShader(shader_id );

    GLint status;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
      GLint maxLength = 0;
      glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &maxLength);
      std::vector<GLchar> errorLog(maxLength);
      glGetShaderInfoLog(shader_id, maxLength, &maxLength, &errorLog[0]);
      for (auto c : errorLog) fputc(c, stderr);
      glDeleteShader(shader_id);
      return 0;
    }
    return shader_id;
  }
};

class Visualisation {
public:
  Visualisation(VirtualPrinter& virtual_printer);
  ~Visualisation();
  void create();
  void process_event(SDL_Event& e);

  VirtualPrinter& virtual_printer;

  void gpio_event_handler(GpioEvent& event);
  void update();
  void destroy();
  void on_position_update();

  void ui_viewport_callback(UiWindow*);
  void ui_info_callback(UiWindow*);

  glm::vec4 last_position = {};
  glm::vec4 last_extrusion_check = {};
  bool extruding = false;
  bool last_extruding  = false;
  const float filiment_diameter = 1.75;
  void set_head_position(glm::vec4 position);
  bool points_are_collinear(glm::vec3 a, glm::vec3 b, glm::vec3 c);

  uint8_t follow_mode = 0;
  bool render_full_path = true;
  bool render_path_line = false;
  glm::vec3 follow_offset = {0.0f, 0.0f, 0.0f};
  std::chrono::steady_clock clock;
  std::chrono::steady_clock::time_point last_update;
  glm::vec4 effector_pos = {};
  glm::vec3 effector_scale = {3.0f ,10.0f, 3.0f};

  PerspectiveCamera camera;
  opengl_util::FrameBuffer* framebuffer = nullptr;
  std::vector<cp_vertex>* active_path_block = nullptr;
  std::vector<std::vector<cp_vertex>> full_path;

  GLuint program, path_program;
  GLuint vao, vbo;
  bool mouse_captured = false;
  bool input_state[6] = {};
  glm::vec<2, int> mouse_lock_pos;

  std::array<GLfloat, 24 * 10> g_vertex_buffer_data{
      //end effector
      0, 0, 0, 0.0, 0.0, 0.0, 1, 0, 0, 1,
      -0.5, 0.5, 0.5, 0.0, 0.0, 0.0, 0, 1, 0, 1,
      -0.5, 0.5, -0.5, 0.0, 0.0, 0.0, 0, 0, 1, 1,

      0, 0, 0, 0.0, 0.0, 0.0, 1, 0, 0, 1,
      -0.5, 0.5, -0.5, 0.0, 0.0, 0.0, 0, 0, 1, 1,
       0.5, 0.5, -0.5, 0.0, 0.0, 0.0, 0, 1, 0, 1,

      0, 0, 0, 0.0, 0.0, 0.0, 1, 0, 0, 1,
       0.5, 0.5, -0.5, 0.0, 0.0, 0.0, 0, 1, 0, 1,
       0.5, 0.5, 0.5, 0.0, 0.0, 0.0, 0, 0, 1, 1,

      0, 0, 0, 0.0, 0.0, 0.0, 1, 0, 0, 1,
       0.5, 0.5, 0.5, 0.0, 0.0, 0.0, 0, 0, 1, 1,
      -0.5, 0.5, 0.5, 0.0, 0.0, 0.0, 0, 1, 0, 1,

       0.5, 0.5, -0.5, 0.0, 0.0, 0.0, 0, 1, 0, 1,
      -0.5, 0.5, -0.5, 0.0, 0.0, 0.0, 0, 0, 1, 1,
      -0.5, 0.5, 0.5, 0.0, 0.0, 0.0, 0, 1, 0, 1,

       0.5, 0.5, -0.5, 0.0, 0.0, 0.0, 0, 1, 0, 1,
      -0.5, 0.5, 0.5, 0.0, 0.0, 0.0, 0, 1, 0, 1,
       0.5, 0.5, 0.5, 0.0, 0.0, 0.0, 0, 0, 1, 1,

      // bed
      build_plate_dimension.x, 0, -build_plate_dimension.y, 0.0, 1.0, 0.0,  0.5, 0.5, 0.5, 1,
      0, 0, -build_plate_dimension.y, 0.0, 1.0, 0.0,  0.5, 0.5, 0.5, 1,
      0, 0, 0, 0.0, 1.0, 0.0,  0.5, 0.5, 0.5, 1,

      build_plate_dimension.x, 0, -build_plate_dimension.y, 0.0, 1.0, 0.0, 0.5, 0.5, 0.5, 1,
      0, 0, 0, 0.0, 1.0, 0.0, 0.5, 0.5, 0.5, 1,
      build_plate_dimension.x, 0, 0, 0.0, 1.0, 0.0, 0.5, 0.5, 0.5, 1,
  };

  float extrude_width = 0.4;
  float extrude_thickness = 0.3;

};

struct Viewport : public UiWindow {
  bool hovered = false;
  bool focused = false;
  ImVec2 viewport_size;
  GLuint texture_id = 0;
  bool dirty = false;

  template<class... Args>
  Viewport(std::string name, Args... args) : UiWindow(name, args...) {}
  void show() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
    ImGui::Begin((char *)name.c_str(), nullptr);
    auto size = ImGui::GetContentRegionAvail();
    if (viewport_size.x != size.x || viewport_size.y != size.y) {
      viewport_size = size;
      dirty = true;
    }
    ImGui::Image((ImTextureID)(intptr_t)texture_id, viewport_size, ImVec2(0,1), ImVec2(1,0));
    hovered = ImGui::IsItemHovered();
    focused = ImGui::IsWindowFocused();

    if (show_callback) show_callback(this);

    ImGui::End();
    ImGui::PopStyleVar();
  }
};
