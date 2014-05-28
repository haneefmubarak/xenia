/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_PROFILING_H_
#define XENIA_PROFILING_H_

#include <memory>

#include <xenia/config.h>
#include <xenia/platform.h>
#include <xenia/platform_includes.h>
#include <xenia/string.h>
#include <xenia/types.h>

#if XE_OPTION_PROFILING
// Pollutes the global namespace. Yuck.
#include <microprofile/microprofile.h>
#endif  // XE_OPTION_PROFILING

namespace xe {

#if XE_OPTION_PROFILING

// Defines a profiling scope for CPU tasks.
// Use `SCOPE_profile_cpu(name)` to activate the scope.
#define DEFINE_profile_cpu(name, group_name, scope_name, color) \
    MICROPROFILE_DEFINE(name, group_name, scope_name, color)

// Declares a previously defined profile scope. Use in a translation unit.
#define DECLARE_profile_cpu(name) MICROPROFILE_DECLARE(name)

// Defines a profiling scope for GPU tasks.
// Use `COUNT_profile_gpu(name)` to activate the scope.
#define DEFINE_profile_gpu(name, group_name, scope_name, color) \
    MICROPROFILE_DEFINE_GPU(name, group_name, scope_name, color)

// Declares a previously defined profile scope. Use in a translation unit.
#define DECLARE_profile_gpu(name) MICROPROFILE_DECLARE_GPU(name)

// Enters a previously defined CPU profiling scope, active for the duration
// of the containing block.
#define SCOPE_profile_cpu(name) \
    MICROPROFILE_SCOPE(name)

// Enters a CPU profiling scope, active for the duration of the containing
// block. No previous definition required.
#define SCOPE_profile_cpu_i(group_name, scope_name, color) \
    MICROPROFILE_SCOPEI(group_name, scope_name, color)

// Enters a previously defined GPU profiling scope, active for the duration
// of the containing block.
#define SCOPE_profile_gpu(name) \
    MICROPROFILE_SCOPEGPU(name)

// Enters a GPU profiling scope, active for the duration of the containing
// block. No previous definition required.
#define SCOPE_profile_gpu_i(group_name, scope_name, color) \
    MICROPROFILE_SCOPEGPUI(group_name, scope_name, color)

// Tracks a CPU value counter.
#define COUNT_profile_cpu(name, count) MICROPROFILE_META_CPU(name, count)

// Tracks a GPU value counter.
#define COUNT_profile_gpu(name, count) MICROPROFILE_META_GPU(name, count)

#else

#define DEFINE_profile_cpu(name, group_name, scope_name, color)
#define DEFINE_profile_gpu(name, group_name, scope_name, color)
#define DECLARE_profile_cpu(name)
#define DECLARE_profile_gpu(name)
#define SCOPE_profile_cpu(name) do {} while (false)
#define SCOPE_profile_cpu_i(group_name, scope_name, color) do {} while (false)
#define SCOPE_profile_gpu(name) do {} while (false)
#define SCOPE_profile_gpu_i(group_name, scope_name, color) do {} while (false)
#define COUNT_profile_cpu(name, count) do {} while (false)
#define COUNT_profile_gpu(name, count) do {} while (false)

#endif  // XE_OPTION_PROFILING

class ProfilerDisplay {
public:
  enum BoxType {
    BOX_TYPE_BAR = MicroProfileBoxTypeBar,
    BOX_TYPE_FLAT = MicroProfileBoxTypeFlat,
  };

  virtual uint32_t width() const = 0;
  virtual uint32_t height() const = 0;

  // TODO(benvanik): GPU timestamping.

  virtual void Begin() = 0;
  virtual void End() = 0;
  virtual void DrawBox(int x, int y, int x1, int y1, uint32_t color, BoxType type) = 0;
  virtual void DrawLine2D(uint32_t count, float* vertices, uint32_t color) = 0;
  virtual void DrawText(int x, int y, uint32_t color, const char* text, size_t text_length) = 0;
};

class Profiler {
public:
  // Dumps data to stdout.
  static void Dump();
  // Cleans up profiling, releasing all memory.
  static void Shutdown();

  // Activates the calling thread for profiling.
  // This must be called immediately after launching a thread.
  static void ThreadEnter(const char* name = nullptr);
  // Deactivates the calling thread for profiling.
  static void ThreadExit();

  // Gets the current display, if any.
  static ProfilerDisplay* display() { return display_.get(); }
  // Initializes drawing with the given display.
  static void set_display(std::unique_ptr<ProfilerDisplay> display);
  // Presents the profiler to the bound display, if any.
  static void Present();

  // TODO(benvanik): display mode/pause/etc?
  // TODO(benvanik): mouse, keys

private:
  static std::unique_ptr<ProfilerDisplay> display_;
};

}  // namespace xe

#endif  // XENIA_PROFILING_H_
