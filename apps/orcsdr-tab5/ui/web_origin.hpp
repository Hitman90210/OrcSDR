#pragma once

// Cross-site protection for the Companion web console.
//
// A browser attaches `Origin` to every cross-site POST, so an `Origin` naming a
// different authority than the request's own `Host` is another site driving
// this receiver: a page left open on any machine on the network could retune
// it, because the reply never has to be readable for the action to happen.
// Requests carrying no `Origin` are left alone -- curl, the regression scripts
// and the release tooling send none, and a browser never omits it on a
// cross-site POST.
//
// Kept free of ESP-IDF headers so the policy is unit-testable on the host.

namespace orcsdr::web_console {

// True when the request may act. `origin` is the raw `Origin` header (null or
// empty when absent) and `host` the raw `Host` header.
//
// Only the authority is compared: the scheme is ignored, ASCII case is ignored,
// and both sides must match to their ends, so a port present on one side alone
// is a mismatch. Anything not shaped like "scheme://authority" is refused --
// that includes the literal "null" a sandboxed frame or a file:// page sends.
constexpr bool origin_allowed(const char* origin, const char* host) {
  if (origin == nullptr || origin[0] == '\0') return true;
  if (host == nullptr || host[0] == '\0') return false;

  const char* authority = nullptr;
  for (const char* p = origin; *p != '\0'; ++p) {
    if (p[0] == ':' && p[1] == '/' && p[2] == '/') {
      authority = p + 3;
      break;
    }
  }
  if (authority == nullptr || *authority == '\0') return false;

  const auto fold = [](char c) -> char {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
  };
  const char* a = authority;
  const char* h = host;
  while (*a != '\0' && *h != '\0') {
    if (fold(*a) != fold(*h)) return false;
    ++a;
    ++h;
  }
  return *a == '\0' && *h == '\0';
}

// The page's own requests must keep passing.
static_assert(origin_allowed("http://192.168.1.250", "192.168.1.250"),
              "same authority is allowed");
static_assert(origin_allowed("http://orcsdr.local:8080", "orcsdr.local:8080"),
              "ports match when both carry one");
static_assert(origin_allowed(nullptr, "192.168.1.250"), "absent Origin is allowed");
// And the cross-site cases must not.
static_assert(!origin_allowed("http://evil.example", "192.168.1.250"),
              "another site is refused");
static_assert(!origin_allowed("http://192.168.1.250:8080", "192.168.1.250"),
              "a port on one side only is a different authority");
static_assert(!origin_allowed("null", "192.168.1.250"), "opaque origin is refused");

}  // namespace orcsdr::web_console
