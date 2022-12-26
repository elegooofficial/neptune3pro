#pragma once

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <functional>

#include <glm/glm.hpp>

class VirtualPrinter {
public:
  struct Component {
    Component() : identifier("UnnamedComponent") {}
    Component(std::string identifier) : identifier(identifier) {}
    virtual ~Component() {}

    virtual void update() {};
    virtual void ui_init() {};
    virtual void ui_widget() {};
    virtual void ui_widgets();

    template<typename T, class... Args>
    auto add_component(std::string name, Args&&... args) {
      auto component = VirtualPrinter::add_component<T>(name, args...);
      component->parent = get_component<Component>(this->name);
      children.push_back(component);
      return component;
    }

    std::string name;
    const std::string identifier;

    std::shared_ptr<Component> parent;
    std::vector<std::shared_ptr<Component>> children;
  };

  static void update() {
    for(auto const& it : components) it->update();
  }

  static void ui_widgets();

  static void build();
  static void update_kinematics();

  template<typename T, class... Args>
  static auto add_component(std::string name, Args&&... args) {
    auto component = std::make_shared<T>(args...);
    component->name = name;
    components.push_back(component);
    component_map[name] = component;
    return component;
  }

  template<typename T>
  static auto get_component(std::string name) {
    return std::static_pointer_cast<T>(component_map[name]);
  }

  static std::function<void(glm::vec4)> on_kinematic_update;

private:
  static std::map<std::string, std::shared_ptr<Component>> component_map;
  static std::vector<std::shared_ptr<Component>> components;
  static std::shared_ptr<Component> root;
};
