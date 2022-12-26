#include <mutex>
#include <fstream>
#include <cmath>
#include <random>
#include "Gpio.h"

#include <gl.h>

#include "imgui.h"
#include "imgui_internal.h"

#include "ST7796Device.h"

#include <src/HAL/NATIVE_SIM/tft/xpt2046.h>
#include <src/HAL/NATIVE_SIM/tft/tft_spi.h>

#define ST7796S_CASET      0x2A // Column Address Set
#define ST7796S_RASET      0x2B // Row Address Set
#define ST7796S_RAMWR      0x2C // Memory Write

ST7796Device::ST7796Device(SpiBus& spi_bus, pin_type tft_cs, SpiBus& touch_spi_bus, pin_type touch_cs, pin_type dc, pin_type beeper, pin_type enc1, pin_type enc2, pin_type enc_but, pin_type back, pin_type kill)
  : SPISlavePeripheral(spi_bus, tft_cs), dc_pin(dc), beeper_pin(beeper), enc1_pin(enc1), enc2_pin(enc2), enc_but_pin(enc_but), back_pin(back), kill_pin(kill) {
  touch = add_component<XPT2046Device>("Touch", touch_spi_bus, touch_cs);
  Gpio::attach(dc_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(beeper_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(kill_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(enc_but_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(back_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(enc1_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(enc2_pin, [this](GpioEvent& event){ this->interrupt(event); });
}

ST7796Device::~ST7796Device() {}

const uint32_t id_code = 0x00CB3B00;
void ST7796Device::process_command(Command cmd) {
  if (cmd.cmd == ST7796S_CASET) {
    xMin = (cmd.data[0] << 8) + cmd.data[1];
    xMax = (cmd.data[2] << 8) + cmd.data[3];
    if (xMin >= width) xMin = width - 1;
    if (xMax >= width) xMax = width - 1;
    graphic_ram_index_x = xMin;
  }
  else if (cmd.cmd == ST7796S_RASET) {
    yMin = (cmd.data[0] << 8) + cmd.data[1];
    yMax = (cmd.data[2] << 8) + cmd.data[3];
    if (yMin >= height) yMin = height - 1;
    if (yMax >= height) yMax = height - 1;
    graphic_ram_index_y = yMin;
  }
  else if (cmd.cmd == LCD_READ_ID) {
    setResponse((uint8_t*)&id_code, 4);
  }
}

void ST7796Device::update() {
  auto now = clock.now();
  float delta = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_update).count();

  if (dirty && delta > 1.0 / 30.0) {
    last_update = now;
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, graphic_ram);
    glBindTexture(GL_TEXTURE_2D, 0);
  }
}

void ST7796Device::interrupt(GpioEvent& ev) {
  if (ev.pin_id == beeper_pin) {
    if (ev.event == GpioEvent::RISE) {
      // play sound
    } else if (ev.event == GpioEvent::FALL) {
      // stop sound
    }
  } else if (ev.pin_id == dc_pin && ev.event == GpioEvent::FALL) {
    //start new command, execute last one
    process_command({command, data});
    data.clear();
  }
}

void ST7796Device::onEndTransaction() {
  SPISlavePeripheral::onEndTransaction();
  //end of transaction, execute pending command
  process_command({command, data});
  data.clear();
};

void ST7796Device::onByteReceived(uint8_t _byte) {
  SPISlavePeripheral::onByteReceived(_byte);
  if (Gpio::get_pin_value(dc_pin)) {
    data.push_back(_byte);
    //direct write to memory, to optimize
    if (command == ST7796S_RAMWR && data.size() == 2) {
      auto pixel = (data[0] << 8) + data[1];
      graphic_ram[graphic_ram_index_x + (graphic_ram_index_y * width)] = pixel;
      if (graphic_ram_index_x >= xMax) {
        graphic_ram_index_x = xMin;
        graphic_ram_index_y++;
      }
      else {
        graphic_ram_index_x++;
      }
      if (graphic_ram_index_y >= yMax && graphic_ram_index_x >= xMax) {
        dirty = true;
      }
      if (graphic_ram_index_y >= height) graphic_ram_index_y = yMin;
      data.clear();
    }
  }
  else {
    //command
    command = _byte;
  }
}

void ST7796Device::ui_init() {
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void ST7796Device::ui_widget() {
  bool popout_begin = false;
  auto size = ImGui::GetContentRegionAvail();
  size.y = ((size.x / (width / (float)height)) * !render_popout) + 60;

  if (ImGui::BeginChild("ST7796Device", size)) {
    ImGui::GetCurrentWindow()->ScrollMax.y = 1.0f; // disable window scroll
    ImGui::Checkbox("Integer Scaling", &render_integer_scaling);
    ImGui::Checkbox("Popout", &render_popout);

    if (render_popout) {
      ImGui::SetNextWindowSize(ImVec2(width + 10, height + 10), ImGuiCond_Once);
      popout_begin = ImGui::Begin("ST7796DeviceRender", &render_popout);
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
    touch->ui_callback();

    if (popout_begin) ImGui::End();
  }
  ImGui::EndChild();
}
