#pragma once
// Plain-language "about this page" text for the Dead Air (i) popup, one topic per tab (same idea as the FT8 popup).
// Pure data with no drawing, so a host test can check line length and content. Each line is at most kMaxLineChars.
#include <cstddef>

namespace orcsdr::dead_air::info {

constexpr size_t kMaxLineChars = 70;

enum class Topic : unsigned char { overview, spectrum, audio, environment, events, review, count };

struct Page {
  const char* title;
  const char* const* lines;  // an empty string is a blank line
  size_t line_count;
};

Page page(Topic topic);

}  // namespace orcsdr::dead_air::info
