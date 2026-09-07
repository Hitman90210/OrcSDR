#include <array>
#include <cstdio>
#include <cstdlib>

#include "pocsag_decoder_core.hpp"

namespace {

[[noreturn]] void fail(const char* expression, int line) {
  std::fprintf(stderr, "FAIL line=%d check=%s\n", line, expression);
  std::exit(1);
}

#define CHECK(expression) \
  do { if (!(expression)) fail(#expression, __LINE__); } while (false)

void test_self_check() {
  CHECK(orcsdr::pocsag::Decoder::self_check());
}

void test_bad_input_is_inert() {
  using namespace orcsdr::pocsag;
  Decoder decoder;
  decoder.process_cu8(nullptr, 100, nullptr, nullptr);
  std::array<uint8_t, 4096> silence{};
  silence.fill(127);
  decoder.process_cu8(silence.data(), silence.size(), nullptr, nullptr);
  const Stats& stats = decoder.stats();
  CHECK(stats.messages_decoded == 0);
  CHECK(stats.batches_synced == 0);
  CHECK(stats.codewords_total == 0);
}

void test_manual_baud_polarity_restricts_search() {
  using namespace orcsdr::pocsag;
  Decoder decoder;
  decoder.configure(Baud::b1200, Polarity::normal);
  // Feed enough neutral (zero) samples that a 512 or 2400 baud / inverted
  // channel would never be exercised; this only proves configure() doesn't
  // crash and stats stay well-formed with a restricted candidate set. The
  // full decode-path proof lives in Decoder::self_check().
  for (int i = 0; i < 4000; ++i) decoder.process_discriminator_sample(0.0f, nullptr, nullptr);
  CHECK(decoder.stats().messages_decoded == 0);
}

}  // namespace

int main() {
  test_self_check();
  test_bad_input_is_inert();
  test_manual_baud_polarity_restricts_search();
  std::puts("pocsag_core_tests: all checks passed");
  return 0;
}
