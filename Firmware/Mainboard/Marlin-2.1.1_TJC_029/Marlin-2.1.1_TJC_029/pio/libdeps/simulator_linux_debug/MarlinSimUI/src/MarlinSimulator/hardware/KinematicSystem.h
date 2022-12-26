#pragma once

#include <memory>
#include <atomic>
#include <glm/glm.hpp>

#include <imgui.h>

#include "Gpio.h"
#include "../virtual_printer.h"
#include "StepperDriver.h"

class KinematicSystem : public VirtualPrinter::Component {
public:
  KinematicSystem(std::function<void(glm::vec4)> on_kinematic_update);
  ~KinematicSystem() {}

  void ui_widget();
  void kinematic_update();

  glm::vec4 effector_position{}, stepper_position{};
  glm::vec3 origin{};
  std::vector<std::shared_ptr<VirtualPrinter::Component>> steppers;
  std::function<void(glm::vec4)> on_kinematic_update;
};

class DeltaKinematicSystem : public VirtualPrinter::Component {
public:
  DeltaKinematicSystem(std::function<void(glm::vec4)> on_kinematic_update);
  ~DeltaKinematicSystem() {}

  void ui_widget();
  void kinematic_update();

  glm::vec4 effector_position{}, stepper_position{};
  glm::vec3 origin{};
  std::vector<std::shared_ptr<VirtualPrinter::Component>> steppers;
  std::function<void(glm::vec4)> on_kinematic_update;

  double delta_radius = 140.0;
  double delta_diagonal_rod = 250.0;
  glm::vec2 delta_tower[3]{};
  glm::vec3 delta_tower_angle_trim{};
  glm::vec3 delta_radius_trim_tower{};
  glm::vec3 delta_diagonal_rod_trim{};
  glm::vec3 delta_diagonal_rod_2_tower{};

  glm::vec3 forward_kinematics(const double z1, const double z2, const double z3);
  void recalc_delta_settings();
};
