#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "p25_decoder_core.hpp"
#include "p25_voice.hpp"

namespace {

std::atomic_size_t allocations{0};

[[noreturn]] void fail(const char* expression, int line) {
  std::fprintf(stderr, "FAIL line=%d check=%s\n", line, expression);
  std::exit(1);
}

#define CHECK(expression) \
  do { if (!(expression)) fail(#expression, __LINE__); } while (false)

uint16_t read_le16(const uint8_t* value) {
  return static_cast<uint16_t>(value[0]) |
         static_cast<uint16_t>(value[1]) << 8;
}

uint32_t read_le32(const uint8_t* value) {
  return static_cast<uint32_t>(value[0]) |
         static_cast<uint32_t>(value[1]) << 8 |
         static_cast<uint32_t>(value[2]) << 16 |
         static_cast<uint32_t>(value[3]) << 24;
}

void test_protocol_self_check_and_bad_input() {
  using namespace orcsdr::p25core;
  reset(UINT32_MAX - 10);
  CHECK(self_check());
  process_cu8(nullptr, 100, UINT32_MAX - 5);
  std::array<uint8_t, 4097> silence{};
  silence.fill(128);
  process_cu8(silence.data(), silence.size(), 20);
  const Snapshot state = snapshot();
  CHECK(!state.frame_sync);
  CHECK(state.sync_words == 0);
  CHECK(state.nid_good == 0);
  CHECK(state.tsbk_good == 0);
  CHECK(state.voice_frames == 0);
}

void test_voice_decode_bounds_and_reset() {
  using namespace orcsdr;
  constexpr uint8_t on_air[18] = {
      0x84,0xC6,0xA9,0x94,0x03,0xFF,0x81,0xC8,0x26,
      0x14,0x2C,0x03,0x90,0xEC,0x85,0x33,0x59,0xBC};
  p25core::VoiceFrame frame{};
  for (size_t bit = 0; bit < p25core::kVoiceFrameBits; ++bit)
    frame.bits[bit] = (on_air[bit / 8] >> (7 - bit % 8)) & 1u;

  CHECK(p25voice::Decoder::self_check());
  p25voice::Decoder decoder;
  std::array<int16_t, p25voice::kPcmSamplesPerFrame> pcm{};
  p25voice::Result result{};
  CHECK(decoder.process(frame, pcm.data(), &result));
  CHECK(result.pcm_samples == pcm.size());
  for (const int16_t sample : pcm) CHECK(sample >= -10000 && sample <= 10000);
  CHECK(!decoder.process(frame, nullptr, nullptr));
  decoder.reset();
  CHECK(decoder.process(frame, pcm.data(), nullptr));
}

uint16_t encode_hamming_10_6(uint8_t data) {
  constexpr uint16_t generator[6] = {0x20E, 0x10D, 0x08B, 0x047, 0x023, 0x01C};
  uint16_t codeword = 0;
  for (int bit = 0; bit < 6; ++bit)
    if (data & (0x20u >> bit)) codeword ^= generator[bit];
  return codeword;
}

std::array<uint8_t, 784> make_ldu2_payload(const std::array<uint8_t, 24>& symbols) {
  constexpr size_t offsets[6] = {288, 472, 656, 840, 1024, 1208};
  std::array<uint8_t, 784> payload{};
  size_t word = 0;
  for (const size_t offset : offsets) {
    for (size_t block_word = 0; block_word < 4; ++block_word) {
      const uint16_t encoded = encode_hamming_10_6(symbols[word++]);
      for (size_t bit = 0; bit < 10; ++bit) {
        const size_t destination = offset + block_word * 10 + bit;
        payload[destination / 2] |= static_cast<uint8_t>(
            ((encoded >> (9 - bit)) & 1u) << (1 - destination % 2));
      }
    }
  }
  return payload;
}

void test_encryption_sync_decode() {
  using namespace orcsdr::p25core;
  // Independent p25craft clear-voice vector: MI=0, ALGID=0x80, KID=0.
  constexpr std::array<uint8_t, 24> clear_symbols = {
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
      0x20,0x00,0x00,0x00,0x2B,0x0B,0x22,0x24,0x26,0x3D,0x31,0x35};
  auto payload = make_ldu2_payload(clear_symbols);
  EncryptionSync result{};
  CHECK(decode_ldu2_encryption(payload.data(), payload.size(), &result));
  CHECK(result.valid);
  CHECK(!result.encrypted);
  CHECK(result.algorithm_id == kClearAlgorithmId);
  CHECK(result.key_id == 0);

  constexpr std::array<uint8_t, 24> encrypted_symbols = {
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
      0x21,0x01,0x08,0x34,0x21,0x37,0x13,0x34,0x0D,0x1F,0x24,0x10};
  payload = make_ldu2_payload(encrypted_symbols);
  CHECK(decode_ldu2_encryption(payload.data(), payload.size(), &result));
  CHECK(result.encrypted);
  CHECK(result.algorithm_id == 0x84);
  CHECK(result.key_id == 0x1234);

  // One inner bit error and four whole-symbol errors are within the two FEC layers.
  payload[288 / 2] ^= 0x02;
  CHECK(decode_ldu2_encryption(payload.data(), payload.size(), &result));
  CHECK(result.algorithm_id == 0x84 && result.key_id == 0x1234);
  auto corrected_symbols = encrypted_symbols;
  for (size_t i = 0; i < 4; ++i) corrected_symbols[i * 3] ^= 0x01;
  payload = make_ldu2_payload(corrected_symbols);
  CHECK(decode_ldu2_encryption(payload.data(), payload.size(), &result));
  CHECK(result.algorithm_id == 0x84 && result.key_id == 0x1234);

  CHECK(!decode_ldu2_encryption(nullptr, payload.size(), &result));
  CHECK(!decode_ldu2_encryption(payload.data(), payload.size() - 1, &result));
  CHECK(!decode_ldu2_encryption(payload.data(), payload.size(), nullptr));
}

orcsdr::p25core::Snapshot decode_control_fixture(const char* path, size_t chunk_size,
                                                 orcsdr::p25core::Modulation modulation) {
  using namespace orcsdr::p25core;
  FILE* file = std::fopen(path, "rb");
  CHECK(file != nullptr);
  std::array<uint8_t, 36> header{};
  CHECK(std::fread(header.data(), 1, header.size(), file) == header.size());
  CHECK(std::memcmp(header.data(), "ORCIQ01\0", 8) == 0);
  CHECK(read_le32(header.data() + 8) == header.size());
  CHECK(read_le32(header.data() + 12) == 960000);
  CHECK(read_le32(header.data() + 16) == 453925000);
  const uint32_t bytes = read_le32(header.data() + 20);
  CHECK(bytes == 1048540);
  CHECK(read_le16(header.data() + 24) == 1);

  set_modulation(modulation);
  reset(1);
  std::array<uint8_t, 32768 + 512> iq{};
  CHECK(chunk_size <= iq.size());
  uint32_t consumed = 0;
  while (consumed < bytes) {
    const size_t request = std::min<size_t>(chunk_size, bytes - consumed);
    CHECK(std::fread(iq.data(), 1, request, file) == request);
    consumed += static_cast<uint32_t>(request);
    const uint32_t now_ms = 1u + static_cast<uint32_t>(
        (static_cast<uint64_t>(consumed / 2) * 1000u) / 960000u);
    process_cu8(iq.data(), request, now_ms);
  }
  CHECK(std::fgetc(file) == EOF);
  std::fclose(file);
  return snapshot();
}

void check_control_snapshot(const orcsdr::p25core::Snapshot& state) {
  CHECK(state.frame_sync);
  CHECK(state.identity_valid);
  CHECK(state.nac == 0x1F0);
  CHECK(state.wacn == 0xBEE00);
  CHECK(state.system_id == 0x1F3);
  CHECK(state.rfss == 1);
  CHECK(state.site == 1);
  CHECK(state.sync_words == 7);
  CHECK(state.nid_good == 7);
  CHECK(state.nid_failed == 0);
  CHECK(state.tsbk_good == 20);
  CHECK(state.tsbk_failed == 0);
  CHECK(state.voice_frames == 0);
}

void test_control_fixture(const char* path) {
  const auto even = decode_control_fixture(path, 32768 + 512, orcsdr::p25core::Modulation::c4fm);
  check_control_snapshot(even);
  const auto odd = decode_control_fixture(path, 32767, orcsdr::p25core::Modulation::c4fm);
  check_control_snapshot(odd);
  CHECK(odd.sync_words == even.sync_words);
  CHECK(odd.nid_good == even.nid_good);
  CHECK(odd.tsbk_good == even.tsbk_good);

  const auto cqpsk_even = decode_control_fixture(
      path, 32768 + 512, orcsdr::p25core::Modulation::cqpsk);
  const auto cqpsk_odd = decode_control_fixture(
      path, 32767, orcsdr::p25core::Modulation::cqpsk);
  CHECK(cqpsk_even.frame_sync);
  CHECK(cqpsk_even.identity_valid);
  CHECK(cqpsk_even.nac == 0x1F0);
  CHECK(cqpsk_even.wacn == 0xBEE00);
  CHECK(cqpsk_even.system_id == 0x1F3);
  CHECK(cqpsk_even.rfss == 1 && cqpsk_even.site == 1);
  CHECK(cqpsk_even.sync_words == 7);
  CHECK(cqpsk_even.nid_good == 7);
  CHECK(cqpsk_even.tsbk_good >= 6);
  CHECK(cqpsk_odd.sync_words == cqpsk_even.sync_words);
  CHECK(cqpsk_odd.nid_good == cqpsk_even.nid_good);
  CHECK(cqpsk_odd.tsbk_good == cqpsk_even.tsbk_good);

  const auto automatic = decode_control_fixture(
      path, 32767, orcsdr::p25core::Modulation::auto_detect);
  check_control_snapshot(automatic);
  CHECK(automatic.selected_modulation == orcsdr::p25core::Modulation::c4fm);
}

void test_allocation_free_stress() {
  using namespace orcsdr;
  std::array<uint8_t, 8193> iq{};
  for (size_t i = 0; i < iq.size(); ++i) iq[i] = static_cast<uint8_t>(i * 37u);
  p25core::VoiceFrame frame{};
  for (size_t bit = 0; bit < p25core::kVoiceFrameBits; ++bit)
    frame.bits[bit] = static_cast<uint8_t>((bit * 13u) & 1u);
  p25voice::Decoder voice;
  std::array<int16_t, p25voice::kPcmSamplesPerFrame> pcm{};

  const size_t before = allocations.load(std::memory_order_relaxed);
  for (uint32_t cycle = 0; cycle < 250; ++cycle) {
    p25core::reset(cycle);
    p25core::process_cu8(iq.data(), iq.size(), cycle + 1);
    CHECK(voice.process(frame, pcm.data(), nullptr));
    voice.reset();
  }
  CHECK(allocations.load(std::memory_order_relaxed) == before);
}

}  // namespace

void* operator new(std::size_t size) {
  allocations.fetch_add(1, std::memory_order_relaxed);
  if (void* memory = std::malloc(size)) return memory;
  throw std::bad_alloc();
}

void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main(int argc, char** argv) {
  CHECK(argc == 2);
  test_protocol_self_check_and_bad_input();
  test_voice_decode_bounds_and_reset();
  test_encryption_sync_decode();
  test_control_fixture(argv[1]);
  test_allocation_free_stress();
  std::puts("p25_core_tests: PASS");
  return 0;
}
