#pragma once

#include <cstdint>

namespace orcsdr::shortwave {

enum class Modal : uint8_t {
  none,
  frequency,
  memory_label,
  memory_notes,
  log_notes,
};

class DashboardState {
 public:
  void open(Modal modal);
  void close_modal();
  Modal modal() const { return modal_; }
  bool background_redraw_allowed() const { return modal_ == Modal::none; }
  bool spectrum_allowed() const { return modal_ == Modal::none; }

 private:
  Modal modal_ = Modal::none;
};

}  // namespace orcsdr::shortwave
