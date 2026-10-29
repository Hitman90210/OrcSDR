#pragma once

#include <cstddef>
#include <cstdint>

// Pure, graphics/ESP-IDF-independent mapping between a UI page/row and
// the fixed catalog slot used by catalog_sync's existing action API.
namespace orcsdr::map_catalog_paging {

inline constexpr std::size_t kRowsPerPage = 5;

template <typename Pack, std::size_t N>
std::size_t entry_count(const Pack (&packs)[N]) {
  std::size_t count = 0;
  for (const auto& pack : packs) {
    if (pack.id[0] != '\0') ++count;
  }
  return count;
}

template <typename Pack, std::size_t N>
std::size_t page_count(const Pack (&packs)[N]) {
  const auto count = entry_count(packs);
  return count == 0 ? 1 : (count + kRowsPerPage - 1) / kRowsPerPage;
}

template <typename Pack, std::size_t N>
int index_for_row(const Pack (&packs)[N], std::size_t page, std::size_t row) {
  if (row >= kRowsPerPage || page >= page_count(packs)) return -1;
  const std::size_t target = page * kRowsPerPage + row;
  std::size_t found = 0;
  for (std::size_t i = 0; i < N; ++i) {
    if (packs[i].id[0] == '\0') continue;
    if (found++ == target) return static_cast<int>(i);
  }
  return -1;
}

// Selection state carries original catalog indices across sparse pages.
struct Cursor {
  std::size_t page = 0;
  int armed = -1;

  template <typename Pack, std::size_t N>
  void catalog_changed(const Pack (&packs)[N]) {
    const auto pages = page_count(packs);
    if (page >= pages) page = pages - 1;
    armed = -1;
  }

  template <typename Pack, std::size_t N>
  bool turn(const Pack (&packs)[N], int direction) {
    const auto pages = page_count(packs);
    if (direction == -1 && page > 0) --page;
    else if (direction == 1 && page + 1 < pages) ++page;
    else return false;
    armed = -1;
    return true;
  }

  template <typename Pack, std::size_t N>
  int install(const Pack (&packs)[N], std::size_t row, bool ready, bool busy) const {
    return ready && !busy ? index_for_row(packs, page, row) : -1;
  }

  template <typename Pack, std::size_t N>
  int remove(const Pack (&packs)[N], std::size_t row, bool busy) {
    const int index = index_for_row(packs, page, row);
    if (index < 0 || busy || !packs[index].installed) { armed = -1; return -1; }
    if (armed != index) { armed = index; return -1; }
    armed = -1;
    return index;
  }
};

}  // namespace orcsdr::map_catalog_paging
