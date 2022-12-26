#pragma once

#include <fstream>

#include "window.h"
#include "user_interface.h"

#include "hardware/Heater.h"
#include "hardware/print_bed.h"

#include "visualisation.h"

#include <string>
#include <imgui.h>
#include <implot.h>
#include "user_interface.h"

#include "virtual_printer.h"

struct GraphWindow : public UiWindow {
  bool hovered = false;
  bool focused = false;
  GLuint texture_id = 0;
  float aspect_ratio = 0.0f;

  template<class... Args>
  GraphWindow(std::string name, GLuint texture_id, float aratio, Args... args) : UiWindow(name, args...), texture_id{texture_id}, aspect_ratio(aratio) {}
  void show() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{2, 2});
    ImGui::Begin((char *)name.c_str(), nullptr);
    auto size = ImGui::GetContentRegionAvail();
    size.y = size.x / aspect_ratio;
    ImGui::Image((ImTextureID)(intptr_t)texture_id, size, ImVec2(0,0), ImVec2(1,1));
    hovered = ImGui::IsItemHovered();
    focused = ImGui::IsWindowFocused();
    ImGui::End();
    ImGui::PopStyleVar();
  }
};

class Simulation {
public:

  Simulation() : vis(testPrinter) {
    testPrinter.build();
  }

  void process_event(SDL_Event& e) {}

  void update() {
    testPrinter.update();
  }

  void ui_callback(UiWindow*) {

  }

  void ui_info_callback(UiWindow *w) {
    vis.ui_info_callback(w);
  }

  VirtualPrinter testPrinter;
  Visualisation vis;
};

class Application {
public:
  Application();
  ~Application();

  void update();
  void render();

  bool active = true;
  Window window;
  UserInterface user_interface;
  Simulation sim;
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  std::ifstream input_file;
};
