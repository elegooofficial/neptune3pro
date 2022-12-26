#include <mutex>
#include <fstream>
#include <cmath>
#include <random>
#include "Gpio.h"

#include <gl.h>

#include "imgui.h"
#include "imgui_internal.h"

#include "ST7920Device.h"

ST7920Device::ST7920Device(pin_type clk, pin_type mosi, pin_type cs, pin_type beeper, pin_type enc1, pin_type enc2, pin_type enc_but, pin_type back, pin_type kill)
  : clk_pin(clk), mosi_pin(mosi), cs_pin(cs), beeper_pin(beeper), enc1_pin(enc1), enc2_pin(enc2), enc_but_pin(enc_but), back_pin(back), kill_pin(kill) {

  Gpio::attach(clk_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(cs_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(beeper_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(enc1_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(enc2_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(enc_but_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(back_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(kill_pin, [this](GpioEvent& event){ this->interrupt(event); });
}

ST7920Device::~ST7920Device() {}

void ST7920Device::process_command(Command cmd) {
  if (cmd.rs) {
    graphic_ram[coordinate[1] + (coordinate[0] * (256 / 8))] = cmd.data;
    if (++coordinate[1] > 32) coordinate[1] = 0;
    dirty = true;
  }
  else if (extended_instruction_set) {
    if (cmd.data & (1 << 7)) {
      // cmd [7] SET GRAPHICS RAM COORDINATE
      coordinate[coordinate_index++] = cmd.data & 0x7F;
      if(coordinate_index == 2) {
        coordinate_index = 0;
        coordinate[1] *= 2;
        if (coordinate[1] >= 128 / 8) {
          coordinate[1] = 0;
          coordinate[0] += 32;
        }
      }
    } else if (cmd.data & (1 << 6)) {
      //printf("cmd: [6] SET IRAM OR SCROLL ADDRESS (0x%02X)\n", cmd.data);
    } else if (cmd.data & (1 << 5)) {
      extended_instruction_set = cmd.data & 0b100;
      //printf("cmd: [5] EXTENDED FUNCTION SET (0x%02X)\n", cmd.data);
    } else if (cmd.data & (1 << 4)) {
      //printf("cmd: [4] UNKNOWN? (0x%02X)\n", cmd.data);
    } else if (cmd.data & (1 << 3)) {
      //printf("cmd: [3] DISPLAY STATUS (0x%02X)\n", cmd.data);
    } else if (cmd.data & (1 << 2)) {
      //printf("cmd: [2] REVERSE (0x%02X)\n", cmd.data);
    } else if (cmd.data & (1 << 1)) {
      //printf("cmd: [1] VERTICAL SCROLL OR RAM ADDRESS SELECT (0x%02X)\n", cmd.data);
    } else if (cmd.data & (1 << 0)) {
      //printf("cmd: [0] STAND BY\n");
    }
  } else {
    if (cmd.data & (1 << 7)) {
      //printf("cmd: [7] SET DDRAM ADDRESS (0x%02X)\n", cmd.data);
    } else if (cmd.data & (1 << 6)) {
      //printf("cmd: [6] SET CGRAM ADDRESS (0x%02X)\n", cmd.data);
    } else if (cmd.data & (1 << 5)) {
      extended_instruction_set = cmd.data & 0b100;
      //printf("cmd: [5] FUNCTION SET (0x%02X)\n", cmd.data);
    } else if (cmd.data & (1 << 4)) {
      //printf("cmd: [4] CURSOR DISPLAY CONTROL (0x%02X)\n", cmd.data);
    } else if (cmd.data & (1 << 3)) {
      //printf("cmd: [3] DISPLAY CONTROL (0x%02X)\n", cmd.data);
    } else if (cmd.data & (1 << 2)) {
      address_increment = cmd.data & 0x1;
      //printf("cmd: [2] ENTRY MODE SET (0x%02X)\n", cmd.data);
    } else if (cmd.data & (1 << 1)) {
      //printf("cmd: [1] RETURN HOME (0x%02X)\n", cmd.data);
    } else if (cmd.data & (1 << 0)) {
      //printf("cmd: [0] DISPLAY CLEAR\n");
    }
  }
}

void ST7920Device::update() {
  auto now = clock.now();
  float delta = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_update).count();

  if (dirty && delta > 1.0 / 30.0) {
    last_update = now;
    dirty = false;
    for (std::size_t x = 0; x < 128; x += 8) {                 // ST7920 graphics ram has 256 bit horizontal resolution, the second 128bit is unused
      for (std::size_t y = 0; y < 64; y++) {                   // 64 bit vertical resolution
        std::size_t texture_index = (y * 128) + x;             // indexed by pixel coordinate
        std::size_t graphic_ram_index = (y * 32) + (x / 8);    // indexed by byte (8 horizontal pixels), 32 byte (256 pixel) stride per row
        for (std::size_t j = 0; j < 8; j++) {
          texture_data[texture_index + j] = TEST(graphic_ram[graphic_ram_index], 7 - j) ? forground_color :  background_color;
        }
      }
    }
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 128, 64, 0, GL_RGB, GL_UNSIGNED_BYTE, texture_data);
    glBindTexture(GL_TEXTURE_2D, 0);
  }
}

void ST7920Device::interrupt(GpioEvent& ev) {
  if (ev.pin_id == clk_pin && ev.event == GpioEvent::FALL && Gpio::get_pin_value(cs_pin)){
    incoming_byte = (incoming_byte << 1) | Gpio::get_pin_value(mosi_pin);
    if (++incoming_bit_count == 8) {
      if (incoming_byte_count == 0 && (incoming_byte & 0xF8) != 0xF8) {
        incoming_byte_count++;
      }
      incoming_cmd[incoming_byte_count++] = incoming_byte;
      incoming_byte = incoming_bit_count = 0;
      if (incoming_byte_count == 3) {
        process_command({(incoming_cmd[0] & 0b100) != 0, (incoming_cmd[0] & 0b010) != 0, uint8_t(incoming_cmd[1] | incoming_cmd[2] >> 4)});
        incoming_byte_count = 0;
      }
    }
  } else if (ev.pin_id == cs_pin && ev.event == GpioEvent::RISE) {
    incoming_bit_count = incoming_byte_count = incoming_byte = 0;
  } else if (ev.pin_id == beeper_pin) {
    if (ev.event == GpioEvent::RISE) {
      // play sound
    } else if (ev.event == GpioEvent::FALL) {
      // stop sound
    }
  } else if (ev.pin_id == kill_pin) {
    Gpio::set_pin_value(kill_pin,  !key_pressed[KeyName::KILL_BUTTON]);
  } else if (ev.pin_id == enc_but_pin) {
    Gpio::set_pin_value(enc_but_pin,  !key_pressed[KeyName::ENCODER_BUTTON]);
  } else if (ev.pin_id == back_pin) {
    Gpio::set_pin_value(back_pin,  !key_pressed[KeyName::BACK_BUTTON]);
  } else if (ev.pin_id == enc1_pin || ev.pin_id == enc2_pin) {
    const uint8_t encoder_state = encoder_position % 4;
    Gpio::set_pin_value(enc1_pin,  encoder_table[encoder_state] & 0x01);
    Gpio::set_pin_value(enc2_pin,  encoder_table[encoder_state] & 0x02);
  }
}

void ST7920Device::ui_init() {
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void ST7920Device::ui_widget() {
  static long int call_count = 0;
  static uint8_t up_held = 0, down_held = 0;
  call_count++;
  bool popout_begin = false;

  auto size = ImGui::GetContentRegionAvail();
  size.y = ((size.x / (width / (float)height)) * !render_popout) + 60;

  if (ImGui::BeginChild("ST7920Device", size)) {
    ImGui::GetCurrentWindow()->ScrollMax.y = 1.0f; // disable window scroll
    ImGui::Checkbox("Integer Scaling", &render_integer_scaling);
    ImGui::Checkbox("Popout", &render_popout);

    if (render_popout) {
      ImGui::SetNextWindowSize(ImVec2(width + 10, height + 10), ImGuiCond_Once);
      popout_begin = ImGui::Begin("ST7920DeviceRender", &render_popout);
      if (!popout_begin) {
        ImGui::End();
        return;
      }
      size = ImGui::GetContentRegionAvail();
    }

    double scale = size.x / width;
    if (render_integer_scaling) {
      scale = scale > 1.0 ? std::floor(scale) : scale;
      size.x = width * scale;
    }
    size.y = height * scale;

    ImGui::Image((ImTextureID)(intptr_t)texture_id, size, ImVec2(0,0), ImVec2(1,1));
    if (ImGui::IsWindowFocused()) {
      key_pressed[KeyName::KILL_BUTTON]    = ImGui::IsKeyDown(SDL_SCANCODE_K);
      key_pressed[KeyName::ENCODER_BUTTON] = ImGui::IsKeyDown(SDL_SCANCODE_SPACE) || ImGui::IsKeyDown(SDL_SCANCODE_RETURN) || ImGui::IsKeyDown(SDL_SCANCODE_RIGHT);
      key_pressed[KeyName::BACK_BUTTON]    = ImGui::IsKeyDown(SDL_SCANCODE_LEFT);

      // Turn keypresses (and repeat) into encoder clicks
      if (up_held) { up_held--; encoder_position--; }
      else if (ImGui::IsKeyPressed(SDL_SCANCODE_UP)) up_held = 4;
      if (down_held) { down_held--; encoder_position++; }
      else if (ImGui::IsKeyPressed(SDL_SCANCODE_DOWN)) down_held = 4;

      if (ImGui::IsItemHovered()) {
        key_pressed[KeyName::ENCODER_BUTTON] |= ImGui::IsMouseClicked(0);
        encoder_position += ImGui::GetIO().MouseWheel > 0 ? 1 : ImGui::GetIO().MouseWheel < 0 ? -1 : 0;
      }
    }

    if (popout_begin) ImGui::End();
  }
  ImGui::EndChild();
}
