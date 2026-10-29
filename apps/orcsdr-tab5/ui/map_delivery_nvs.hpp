#pragma once
#include "map_delivery_state.hpp"
namespace orcsdr::maps::delivery {
// Separate namespace; never migrates or rewrites receiver/Wi-Fi settings.
class NvsStateStore final:public Store {
 public:
  Read read(std::string*) override;
  bool write(std::string_view) override;
};
}
