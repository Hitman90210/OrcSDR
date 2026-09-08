#include <array>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <vector>

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

uint32_t encode_word(uint32_t info) {
  uint32_t word = info << 11;
  uint32_t remainder = word;
  for (int bit = 31; bit >= 11; --bit)
    if (remainder & (1u << bit)) remainder ^= 0xED200000u >> (31 - bit);
  word |= remainder;
  unsigned parity = 0;
  for (uint32_t n = word; n; n >>= 1) parity ^= n & 1;
  return word | parity;
}

void test_iq(float offset_hz, int phase_samples, bool inverted, int samples_per_bit = 800) {
  using namespace orcsdr::pocsag;
  std::vector<bool> bits;
  auto word = [&](uint32_t w) { for (int b = 31; b >= 0; --b) bits.push_back((w >> b) & 1); };
  for (int i = 0; i < 576; ++i) bits.push_back(i & 1);
  word(kSyncWord);
  word(encode_word((1234560u >> 3) << 2 | 3));
  std::vector<bool> text_bits;
  for (char c : "TEST") if (c) for (int i = 0; i < 7; ++i) text_bits.push_back((c >> i) & 1);
  while (text_bits.size() % 20) text_bits.push_back(false);
  for (size_t i = 0; i < text_bits.size(); i += 20) {
    uint32_t payload = 0;
    for (size_t j = 0; j < 20; ++j) payload = (payload << 1) | text_bits[i + j];
    word(encode_word(0x100000u | payload));
  }
  for (int i = 3; i < 16; ++i) word(kIdleWord);
  word(kSyncWord);
  Decoder decoder;
  struct Capture { unsigned count = 0; Message message{}; } capture;
  std::array<uint8_t, 4096> iq{};
  size_t used = 0;
  double phase = 0;
  auto sample = [&](float frequency) {
    phase += 6.283185307179586 * frequency / kInputSampleRateHz;
    iq[used++] = static_cast<uint8_t>(127 + 65 * std::cos(phase));
    iq[used++] = static_cast<uint8_t>(127 + 65 * std::sin(phase));
    if (used == iq.size()) {
      decoder.process_cu8(iq.data(), used, [](const Message& m, void* ctx) {
        auto& c = *static_cast<Capture*>(ctx); ++c.count; c.message = m;
      }, &capture);
      used = 0;
    }
  };
  for (int i = 0; i < phase_samples; ++i) sample(offset_hz + 4500);
  for (bool bit : bits)
    for (int i = 0; i < samples_per_bit; ++i) sample(offset_hz + ((bit != inverted) ? -4500 : 4500));
  while (used) sample(offset_hz + 4500);
  std::printf("IQ offset=%.0f phase=%d inverted=%d sync=%u messages=%u text=%s\n",
      offset_hz, phase_samples, inverted, decoder.stats().batches_synced, capture.count, capture.message.text);
  CHECK(capture.count == 1);
  CHECK(capture.message.capcode == 1234560);
  CHECK(std::strncmp(capture.message.text, "TEST", 4) == 0);
}

}  // namespace

int main() {
  test_self_check();
  test_bad_input_is_inert();
  test_manual_baud_polarity_restricts_search();
  test_iq(0, 0, false);
  test_iq(0, 317, true);
  test_iq(6000, 317, false);
  test_iq(-6000, 613, true);
  test_iq(-13500, 317, false);
  test_iq(13500, 451, true);
  test_iq(6000, 123, false, 792);
  test_iq(-6000, 451, true, 808);
  std::puts("pocsag_core_tests: all checks passed");
  return 0;
}
