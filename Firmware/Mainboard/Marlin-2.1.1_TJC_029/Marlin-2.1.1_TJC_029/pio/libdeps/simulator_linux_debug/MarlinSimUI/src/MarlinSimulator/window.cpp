#include <SDL2/SDL.h>
#include <glad/glad.h>

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_opengl3.h>

#include "window.h"

namespace window_impl {

SDL_Window* window = nullptr;
SDL_GLContext gl_context = nullptr;
bool window_valid = false;
bool imgui_initialised = false;

WindowReturnCode sdl_window_create(WindowConfig config = {}) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("SDL_Init Error: %s\n", SDL_GetError());
    return WindowReturnCode::SDL_INIT_FAILED;
  }

  #if __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
  #else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  #endif

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, config.gl_profile);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, config.gl_version_major);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, config.gl_version_minor);

  SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1 );
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8 );
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8 );
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8 );
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8 );
  if (config.multisamples) {
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, config.multisamples);
  }

  // Create window with graphics context
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  window = SDL_CreateWindow((char *)config.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
  if (window == nullptr) {
    fprintf(stderr, "SDL_CreateWindow: Failed to Create Window (%s), retrying without multisampling (fallback)\n", SDL_GetError());
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    window = SDL_CreateWindow((char *)config.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
  }
  if (window == nullptr) {
    fprintf(stderr, "SDL_CreateWindow: Failed to Create Window (%s)\n", SDL_GetError());
    return WindowReturnCode::SDL_CREATE_WINDOW_FAILED;
  }
  gl_context = SDL_GL_CreateContext(window);
  if (gl_context == nullptr) {
    fprintf(stderr, "SDL_GL_CreateContext: Failed to initialize OpenGL context (%s)\n", SDL_GetError());
    return WindowReturnCode::SDL_GL_CREATECONTEXT_FAILED;
  }

  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(config.vsync);

  if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
    fprintf(stderr, "gladLoadGLLoader: Failed to initialize OpenGL loader!\n");
    return WindowReturnCode::GLEW_INIT_FAILED;
  }

  if (config.multisamples) glEnable(GL_MULTISAMPLE);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  return WindowReturnCode::WINDOW_OK;
}

void sdl_window_destroy() {
  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

WindowReturnCode imgui_create() {
  if (window == nullptr) {
    return WindowReturnCode::IMGUI_NULL_WINDOW;
  }
  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsClassic();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
  //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

  ImGuiStyle& style = ImGui::GetStyle();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      style.WindowRounding = 0.0f;
      style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }

  // Setup Platform/Renderer bindings
  if (!ImGui_ImplSDL2_InitForOpenGL(window, gl_context)) {
    fprintf(stderr, "ImGui_ImplSDL2_InitForOpenGL: Failed to init Imgui SDL2 support!\n");
    return WindowReturnCode::IMGUI_SDLINIT_FAIL;
  }
  if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
    fprintf(stderr, "ImGui_ImplOpenGL3_Init:Failed to init Imgui OpenGL3 support!\n");
    return WindowReturnCode::IMGUI_GLINIT_FAIL;
  }
  window_valid = true;
  return WindowReturnCode::WINDOW_OK;
}

void imgui_destroy() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
}

} // namespace window_impl

Window::Window() {
  if (window_impl::sdl_window_create()) return;
  if (window_impl::imgui_create()) return;
  window_impl::imgui_initialised = true;
}

Window::~Window() {
  if (window_impl::imgui_initialised) window_impl::imgui_destroy();
  if (window_impl::window_valid) window_impl::sdl_window_destroy();
}

void* Window::getHandle() {
  return (void*)window_impl::window;
}

void Window::swap_buffers() {
  if(window_impl::window_valid) SDL_GL_SwapWindow(window_impl::window);
}
