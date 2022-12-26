#pragma once

#include <string>
#include <vector>
#include <gl.h>

enum WindowReturnCode {
  WINDOW_OK,
  SDL_INIT_FAILED,
  SDL_CREATE_WINDOW_FAILED,
  SDL_GL_CREATECONTEXT_FAILED,
  GLEW_INIT_FAILED,
  IMGUI_NULL_WINDOW,
  IMGUI_SDLINIT_FAIL,
  IMGUI_GLINIT_FAIL
};

struct WindowConfig {
  enum GlProfile {
    CORE           = 0x0001,
    COMPATIBILITY  = 0x0002,
    ES             = 0x0004
  };

  std::string title = "Marlin Simulator";
  GLuint gl_version_major = 3, gl_version_minor = 3, gl_profile = GlProfile::CORE, multisamples = 4, vsync = 1;
};

class Window {
public:
  Window();
  ~Window();

  void* getHandle();
  void swap_buffers();
};

namespace opengl_util {

struct FrameBuffer {
  virtual bool update(GLuint w, GLuint h) = 0;
  virtual void release() = 0;
  virtual void bind() = 0;
  virtual void render() = 0;
  virtual void unbind() = 0;
  virtual GLuint texture_id() = 0;
  virtual ~FrameBuffer() {}
};

struct MsaaFrameBuffer : public FrameBuffer {

  bool create(GLuint w, GLuint h, GLint msaa) {
    msaa_levels = msaa;
    return update(w, h);
  }

  bool update(GLuint w, GLuint h) {
    if (w == width && h == height) {
      return true;
    }

    width = w;
    height = h;

    if(color_attachment_id) {
      release();
    }

    glGenTextures(1, &color_attachment_id);
    glBindTexture(GL_TEXTURE_2D, color_attachment_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenFramebuffers(1, &framebuffer_msaa_id);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_msaa_id);
    glGenRenderbuffers(1, &color_attachment_msaa_id);
    glBindRenderbuffer(GL_RENDERBUFFER, color_attachment_msaa_id);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa_levels, GL_RGB8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glGenRenderbuffers(1, &depth_attachment_msaa_id);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_attachment_msaa_id);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa_levels, GL_DEPTH_COMPONENT, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_attachment_msaa_id);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_attachment_msaa_id);
    glGenFramebuffers(1, &framebuffer_id);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
    glGenRenderbuffers(1, &render_buffer_id);
    glBindRenderbuffer(GL_RENDERBUFFER, render_buffer_id);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_attachment_id, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, render_buffer_id);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      release();
      return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
  }

  void release() {
    glDeleteRenderbuffers(1, &render_buffer_id);
    glDeleteFramebuffers(1, &framebuffer_id);
    glDeleteRenderbuffers(1, &depth_attachment_msaa_id);
    glDeleteRenderbuffers(1, &color_attachment_msaa_id);
    glDeleteFramebuffers(1, &framebuffer_msaa_id);
    glDeleteTextures(1, &color_attachment_id);
  }

  void bind() {
    if(framebuffer_msaa_id) {
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_msaa_id);
      glViewport(0, 0, width, height);
    }
  }

  void render() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_msaa_id);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_id);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  void unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  GLuint texture_id() {
    return color_attachment_id;
  }

  GLuint framebuffer_id = 0, framebuffer_msaa_id = 0,
          color_attachment_id = 0, color_attachment_msaa_id = 0, depth_attachment_msaa_id = 0,
          render_buffer_id = 0,
          width = 0, height = 0;
  GLint msaa_levels = 0;
};

struct TextureFrameBuffer : public FrameBuffer {
  bool create(GLuint w, GLuint h) {
    return update(w, h);
  }

  bool update(GLuint w, GLuint h) {
    if (w == width && h == height) {
      return true;
    }

    width = w;
    height = h;

    if(color_attachment_id) {
      release();
    }

    glGenTextures(1, &color_attachment_id);
    glBindTexture(GL_TEXTURE_2D, color_attachment_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &depth_attachment_id);
    glBindTexture(GL_TEXTURE_2D, depth_attachment_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenFramebuffers(1, &framebuffer_id);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_attachment_id, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depth_attachment_id, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      release();
      return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
  }

  void release() {
    glDeleteFramebuffers(1, &framebuffer_id);
    glDeleteTextures(1, &depth_attachment_id);
    glDeleteTextures(1, &color_attachment_id);
  }

  void bind() {
    if(framebuffer_id) {
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
      glViewport(0, 0, width, height);
    }
  }

  void render() {
    unbind();
  }

  void unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  GLuint texture_id() {
    return color_attachment_id;
  }

  GLuint framebuffer_id = 0, color_attachment_id = 0, depth_attachment_id = 0, width = 0, height = 0;
};

} // namespace opengl_util
