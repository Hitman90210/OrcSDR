#pragma once
#include "map_transfer.hpp"
#include <memory>
namespace orcsdr::maps::transfer {
class HttpStream final:public Stream {
 public:
  HttpStream() noexcept;
  ~HttpStream() override;
  bool open(std::string_view,const download::Request&,Headers&) override;
  int read(uint8_t*,size_t) override;
  void close() override;
 private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace orcsdr::maps::transfer
