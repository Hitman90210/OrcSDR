#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pocsag_store.hpp"

namespace {

[[noreturn]] void fail(const char* expression, int line) {
  std::fprintf(stderr, "FAIL line=%d check=%s\n", line, expression);
  std::exit(1);
}

#define CHECK(expression) \
  do { if (!(expression)) fail(#expression, __LINE__); } while (false)

void test_self_check() {
  CHECK(orcsdr::pocsag_store::Table::self_check());
}

void test_reset_clears_table() {
  using namespace orcsdr::pocsag_store;
  Table table;
  table.record_hit(7, 100);
  table.record_hit(8, 200);
  CHECK(table.count() == 2);
  table.reset();
  CHECK(table.count() == 0);
  CHECK(table.find(7) < 0);
}

void test_mute_independent_of_watch() {
  using namespace orcsdr::pocsag_store;
  Table table;
  table.record_hit(9001, 1);
  CHECK(table.set_muted(9001, true));
  CHECK(table.set_watched(9001, true));
  const Identity* identity = table.at(0);
  CHECK(identity != nullptr && identity->muted && identity->watched);
  CHECK(table.set_muted(9001, false));
  identity = table.at(0);
  CHECK(identity->muted == false && identity->watched == true);
}

void test_field_truncation_is_bounded_and_terminated() {
  using namespace orcsdr::pocsag_store;
  Table table;
  table.record_hit(1, 1);
  char long_alias[64];
  std::memset(long_alias, 'A', sizeof(long_alias) - 1);
  long_alias[sizeof(long_alias) - 1] = '\0';
  CHECK(table.set_alias(1, long_alias));
  const Identity* identity = table.at(0);
  CHECK(std::strlen(identity->alias) == kAliasLen - 1);
  CHECK(identity->alias[kAliasLen - 1] == '\0');
}

}  // namespace

int main() {
  test_self_check();
  test_reset_clears_table();
  test_mute_independent_of_watch();
  test_field_truncation_is_bounded_and_terminated();
  std::puts("pocsag_store_tests: all checks passed");
  return 0;
}
