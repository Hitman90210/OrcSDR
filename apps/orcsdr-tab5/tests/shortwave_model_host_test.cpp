#include "shortwave_model.hpp"

#include <cstring>
#include <cstdio>

#define CHECK(expression)                                                      \
  do {                                                                         \
    if (!(expression)) {                                                       \
      std::printf("SHORTWAVE_MODEL_HOST_TEST fail=%s line=%d\n", #expression, \
                  __LINE__);                                                   \
      return 1;                                                                \
    }                                                                          \
  } while (false)

int main() {
  using namespace orcsdr::shortwave;
  const auto broadcast = mode_guide_for(9800000);
  const auto amateur_lsb = mode_guide_for(3800000);
  const auto amateur_usb = mode_guide_for(14200000);
  const auto unknown = mode_guide_for(27500000);
  CHECK(model_self_check());
  CHECK(region_for(24000) == SpectrumRegion::vlf_edge);
  CHECK(region_for(30000) == SpectrumRegion::lf);
  CHECK(region_for(300000) == SpectrumRegion::mf);
  CHECK(region_for(3000000) == SpectrumRegion::hf);
  CHECK(region_for(30000001) == SpectrumRegion::outside);
  CHECK(std::strcmp(broadcast.likely_mode, "AM") == 0);
  CHECK(broadcast.supported_now);
  CHECK(std::strcmp(amateur_lsb.likely_mode, "LSB") == 0);
  CHECK(!amateur_lsb.supported_now);
  CHECK(std::strcmp(amateur_usb.likely_mode, "USB") == 0);
  CHECK(!amateur_usb.supported_now);
  CHECK(std::strcmp(unknown.likely_mode, "CHECK") == 0);
  CHECK(!unknown.supported_now);
  std::puts("SHORTWAVE_MODEL_HOST_TEST pass=1");
  return 0;
}
