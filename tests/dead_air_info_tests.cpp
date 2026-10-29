// Checks the Dead Air (i) popup text: every tab has a page, every line fits the popup, nothing overflows two pages.
#include <cassert>
#include <cstdio>
#include <cstring>

#include "dead_air_info.hpp"

int main() {
  using namespace orcsdr::dead_air::info;
  constexpr size_t kLinesPerPage = 12;  // matches the dashboard
  for (size_t t = 0; t < static_cast<size_t>(Topic::count); ++t) {
    const Page p = page(static_cast<Topic>(t));
    assert(p.title && p.title[0]);
    assert(p.lines && p.line_count >= 5);
    assert(p.line_count <= kLinesPerPage * 2);
    for (size_t i = 0; i < p.line_count; ++i) {
      assert(p.lines[i] != nullptr);
      assert(std::strlen(p.lines[i]) <= kMaxLineChars);
    }
    assert(p.lines[0][0] != '\0');  // never opens on a blank line
  }
  // The honesty statement the owner asked for stays on the first page.
  const Page overview = page(Topic::overview);
  bool says_no_detection = false;
  for (size_t i = 0; i < overview.line_count; ++i)
    if (std::strstr(overview.lines[i], "does not detect ghosts")) says_no_detection = true;
  assert(says_no_detection);
  // An out-of-range topic falls back to the overview instead of reading past the table.
  assert(page(Topic::count).title == overview.title);
  std::puts("Dead Air info text tests PASS");
  return 0;
}
