#include <cmath>

#include <imgui.h>

#include "KinematicSystem.h"

#include <src/inc/MarlinConfig.h>

constexpr float steps_per_unit[] = DEFAULT_AXIS_STEPS_PER_UNIT;

KinematicSystem::KinematicSystem(std::function<void(glm::vec4)> on_kinematic_update) : VirtualPrinter::Component("Kinematic System"), on_kinematic_update(on_kinematic_update) {

  steppers.push_back(add_component<StepperDriver>("Stepper0", X_ENABLE_PIN, X_DIR_PIN, X_STEP_PIN, [this](){ this->kinematic_update(); }));
  steppers.push_back(add_component<StepperDriver>("Stepper1", Y_ENABLE_PIN, Y_DIR_PIN, Y_STEP_PIN, [this](){ this->kinematic_update(); }));
  steppers.push_back(add_component<StepperDriver>("Stepper2", Z_ENABLE_PIN, Z_DIR_PIN, Z_STEP_PIN, [this](){ this->kinematic_update(); }));
  steppers.push_back(add_component<StepperDriver>("Stepper3", E0_ENABLE_PIN, E0_DIR_PIN, E0_STEP_PIN, [this](){ this->kinematic_update(); }));

  srand(time(0));
  origin.x = (rand() % (int)((X_MAX_POS / 4) - X_MIN_POS)) + X_MIN_POS;
  origin.y = (rand() % (int)((Y_MAX_POS / 4) - Y_MIN_POS)) + Y_MIN_POS;
  origin.z = (rand() % (int)((Z_MAX_POS / 8) - Z_MIN_POS)) + Z_MIN_POS;
}

void KinematicSystem::kinematic_update() {
  stepper_position = glm::vec4{
    std::static_pointer_cast<StepperDriver>(steppers[0])->steps() / steps_per_unit[0] * (((INVERT_X_DIR * 2) - 1) * -1.0),
    std::static_pointer_cast<StepperDriver>(steppers[1])->steps() / steps_per_unit[1] * (((INVERT_Y_DIR * 2) - 1) * -1.0),
    std::static_pointer_cast<StepperDriver>(steppers[2])->steps() / steps_per_unit[2] * (((INVERT_Z_DIR * 2) - 1) * -1.0),
    std::static_pointer_cast<StepperDriver>(steppers[3])->steps() / steps_per_unit[3] * (((INVERT_E0_DIR * 2) - 1) * -1.0)
  };

  effector_position = glm::vec4(origin, 0.0f) + stepper_position;
  on_kinematic_update(effector_position);
}

void KinematicSystem::ui_widget() {
  auto pos = (glm::vec4(origin, 0) + stepper_position);
  auto value = pos.x;
  if (ImGui::SliderFloat("X Position(mm)", &value, X_MIN_POS, X_MAX_POS)) {
    origin.x = value - stepper_position.x;
    kinematic_update();
  }
  value = pos.y;
  if (ImGui::SliderFloat("Y Position(mm)",  &value, Y_MIN_POS, Y_MAX_POS)) {
    origin.y = value - stepper_position.y;
    kinematic_update();
  }
  value = pos.z;
  if (ImGui::SliderFloat("Z Position(mm)",  &value, Z_MIN_POS, Z_MAX_POS)) {
    origin.z = value - stepper_position.z;
    kinematic_update();
  }
}

#if ENABLED(DELTA)
#define A_AXIS 0
#define B_AXIS 1
#define C_AXIS 2

// Stolen from Marlin Firmware delta.cpp
void DeltaKinematicSystem::recalc_delta_settings() {
  //constexpr abc_float_t trt = DELTA_RADIUS_TRIM_TOWER;
  delta_tower[A_AXIS] = { std::cos(glm::radians(210.0 + delta_tower_angle_trim.x)) * (delta_radius + delta_radius_trim_tower.x), // front left tower
                          std::sin(glm::radians(210.0 + delta_tower_angle_trim.x)) * (delta_radius + delta_radius_trim_tower.x) };
  delta_tower[B_AXIS] = { std::cos(glm::radians(330.0 + delta_tower_angle_trim.y)) * (delta_radius + delta_radius_trim_tower.y), // front right tower
                          std::sin(glm::radians(330.0 + delta_tower_angle_trim.y)) * (delta_radius + delta_radius_trim_tower.y) };
  delta_tower[C_AXIS] = { std::cos(glm::radians( 90.0 + delta_tower_angle_trim.z)) * (delta_radius + delta_radius_trim_tower.z), // back middle tower
                          std::sin(glm::radians( 90.0 + delta_tower_angle_trim.z)) * (delta_radius + delta_radius_trim_tower.z) };
  delta_diagonal_rod_2_tower = { std::pow(delta_diagonal_rod + delta_diagonal_rod_trim.x, 2),
                                 std::pow(delta_diagonal_rod + delta_diagonal_rod_trim.y, 2),
                                 std::pow(delta_diagonal_rod + delta_diagonal_rod_trim.z, 2) };
}

glm::vec3 DeltaKinematicSystem::forward_kinematics(const double z1, const double z2, const double z3) {

  // Create a vector in old coordinates along x axis of new coordinate
  const double p12[3] = { delta_tower[B_AXIS].x - delta_tower[A_AXIS].x,
                          delta_tower[B_AXIS].y - delta_tower[A_AXIS].y,
                          z2 - z1 };

  // Get the reciprocal of Magnitude of vector.
  const double d2 = std::pow(p12[0], 2) + std::pow(p12[1], 2) + std::pow(p12[2], 2), inv_d = 1.0 / std::sqrt(d2);

  // Create unit vector by multiplying by the inverse of the magnitude.
  const double ex[3] = { p12[0] * inv_d,
                         p12[1] * inv_d,
                         p12[2] * inv_d };

  // Get the vector from the origin of the new system to the third point.
  const double p13[3] = { delta_tower[C_AXIS].x - delta_tower[A_AXIS].x,
                          delta_tower[C_AXIS].y - delta_tower[A_AXIS].y,
                          z3 - z1 };

  // Use the dot product to find the component of this vector on the X axis.
  const double i = ex[0] * p13[0] + ex[1] * p13[1] + ex[2] * p13[2];

  // Create a vector along the x axis that represents the x component of p13.
  const double iex[3] = { ex[0] * i,
                          ex[1] * i,
                          ex[2] * i };

  // Subtract the X component from the original vector leaving only Y. We use the
  // variable that will be the unit vector after we scale it.
  double ey[3] = { p13[0] - iex[0],
                   p13[1] - iex[1],
                   p13[2] - iex[2] };

  // The magnitude and the inverse of the magnitude of Y component
  const double j2 = std::pow(ey[0], 2) + std::pow(ey[1], 2) + std::pow(ey[2], 2), inv_j = 1.0 / std::sqrt(j2);

  // Convert to a unit vector
  ey[0] *= inv_j;
  ey[1] *= inv_j;
  ey[2] *= inv_j;

  // The cross product of the unit x and y is the unit z
  // double[] ez = vectorCrossProd(ex, ey);
  const double ez[3] = {
    ex[1] * ey[2] - ex[2] * ey[1],
    ex[2] * ey[0] - ex[0] * ey[2],
    ex[0] * ey[1] - ex[1] * ey[0]
  };

  // We now have the d, i and j values defined in Wikipedia.
  // Plug them into the equations defined in Wikipedia for Xnew, Ynew and Znew
  const double Xnew = (delta_diagonal_rod_2_tower.x - delta_diagonal_rod_2_tower.y + d2) * inv_d * 0.5;
  const double Ynew = ((delta_diagonal_rod_2_tower.x - delta_diagonal_rod_2_tower.z + std::pow(i, 2) + j2) * 0.5 - i * Xnew) * inv_j;
  const double Znew = std::sqrt(delta_diagonal_rod_2_tower.x - (std::pow(Xnew, 2) + std::pow(Ynew, 2)));

  // Start from the origin of the old coordinates and add vectors in the
  // old coords that represent the Xnew, Ynew and Znew to find the point
  // in the old system.
  return glm::vec3{ delta_tower[A_AXIS].x + ex[0] * Xnew + ey[0] * Ynew - ez[0] * Znew,
                    delta_tower[A_AXIS].y + ex[1] * Xnew + ey[1] * Ynew - ez[1] * Znew,
                    z1 + ex[2] * Xnew + ey[2] * Ynew - ez[2] * Znew
  };
}

DeltaKinematicSystem::DeltaKinematicSystem(std::function<void(glm::vec4)> on_kinematic_update) : VirtualPrinter::Component("Delta Kinematic System"), on_kinematic_update(on_kinematic_update) {

  steppers.push_back(add_component<StepperDriver>("Stepper0", X_ENABLE_PIN, X_DIR_PIN, X_STEP_PIN, [this](){ this->kinematic_update(); }));
  steppers.push_back(add_component<StepperDriver>("Stepper1", Y_ENABLE_PIN, Y_DIR_PIN, Y_STEP_PIN, [this](){ this->kinematic_update(); }));
  steppers.push_back(add_component<StepperDriver>("Stepper2", Z_ENABLE_PIN, Z_DIR_PIN, Z_STEP_PIN, [this](){ this->kinematic_update(); }));
  steppers.push_back(add_component<StepperDriver>("Stepper3", E0_ENABLE_PIN, E0_DIR_PIN, E0_STEP_PIN, [this](){ this->kinematic_update(); }));
  recalc_delta_settings();

  // Add an offset as on deltas the linear rails are offset from the bed
  origin.x = 207.124;//215.0 + DELTA_HEIGHT;
  origin.y = 207.124;//215.0 + DELTA_HEIGHT;
  origin.z = 207.124;//215.0 + DELTA_HEIGHT;
}

void DeltaKinematicSystem::kinematic_update() {
  stepper_position = glm::vec4{
    std::static_pointer_cast<StepperDriver>(steppers[0])->steps() / steps_per_unit[0] * (((INVERT_X_DIR * 2) - 1) * -1.0),
    std::static_pointer_cast<StepperDriver>(steppers[1])->steps() / steps_per_unit[1] * (((INVERT_Y_DIR * 2) - 1) * -1.0),
    std::static_pointer_cast<StepperDriver>(steppers[2])->steps() / steps_per_unit[2] * (((INVERT_Z_DIR * 2) - 1) * -1.0),
    std::static_pointer_cast<StepperDriver>(steppers[3])->steps() / steps_per_unit[3] * (((INVERT_E0_DIR * 2) - 1) * -1.0)
  };

  // Add an offset to fudge the coordinate system onto the bed
  effector_position = glm::vec4{ forward_kinematics(stepper_position.x + origin.x, stepper_position.y + origin.y, stepper_position.z + origin.z), stepper_position.a} + glm::vec4{X_BED_SIZE / 2.0, Y_BED_SIZE / 2.0, 0, 0};
  on_kinematic_update(effector_position);
}

void DeltaKinematicSystem::ui_widget() {
  auto value = stepper_position.x + origin.x;
  if (ImGui::SliderFloat("Stepper(A) Position (mm)", &value, -100, DELTA_HEIGHT + 100)) {
    origin.x = value - stepper_position.x;
    kinematic_update();
  }
  value = stepper_position.y + origin.y;
  if (ImGui::SliderFloat("Stepper(B) Position (mm)",  &value, -100, DELTA_HEIGHT + 100)) {
    origin.y = value - stepper_position.y;
    kinematic_update();
  }
  value = stepper_position.z + origin.z;
  if (ImGui::SliderFloat("Stepper(C) Position (mm)",  &value, -100, DELTA_HEIGHT + 100)) {
    origin.z = value - stepper_position.z;
    kinematic_update();
  }
  ImGui::Text("Stepper Position:");
  ImGui::Text("x: %f", stepper_position.x);
  ImGui::Text("y: %f", stepper_position.y);
  ImGui::Text("z: %f", stepper_position.z);
  ImGui::Text("Cartesian Position:");
  ImGui::Text("x: %f", effector_position.x);
  ImGui::Text("y: %f", effector_position.y);
  ImGui::Text("z: %f", effector_position.z);
}

#endif
