// Host tests for the Companion web console's cross-site guard.
//
// The policy lives in web_origin.hpp precisely so it can be exercised here,
// away from esp_http_server. The request-level wrapper in web_console.cpp only
// reads the two headers and hands them to origin_allowed().

#include <cstdio>

#include "web_origin.hpp"

namespace {

int failures = 0;
int checks = 0;

void expect(bool actual, bool wanted, const char* what) {
  ++checks;
  if (actual == wanted) return;
  ++failures;
  std::printf("FAIL: %s (wanted %s, got %s)\n", what, wanted ? "allowed" : "refused",
              actual ? "allowed" : "refused");
}

void allowed(const char* origin, const char* host, const char* what) {
  expect(orcsdr::web_console::origin_allowed(origin, host), true, what);
}

void refused(const char* origin, const char* host, const char* what) {
  expect(orcsdr::web_console::origin_allowed(origin, host), false, what);
}

}  // namespace

int main() {
  // Requests that must keep working.
  allowed(nullptr, "192.168.1.250", "no Origin header at all (curl, tooling)");
  allowed("", "192.168.1.250", "empty Origin header");
  allowed("http://192.168.1.250", "192.168.1.250", "the page's own origin");
  allowed("https://192.168.1.250", "192.168.1.250", "scheme is not compared");
  allowed("http://192.168.1.250:8080", "192.168.1.250:8080", "matching explicit ports");
  allowed("http://OrcSDR.local", "orcsdr.local", "host case is ignored");
  allowed("http://orcsdr.local", "OrcSDR.LOCAL", "header case is ignored");

  // The attack this exists for: a page on another site posting to us.
  refused("http://evil.example", "192.168.1.250", "another site");
  refused("https://evil.example", "orcsdr.local", "another site over https");
  refused("http://192.168.1.251", "192.168.1.250", "a neighbour address");

  // Near-misses that must not be treated as our own origin.
  refused("http://192.168.1.250:8080", "192.168.1.250", "port on the origin only");
  refused("http://192.168.1.250", "192.168.1.250:8080", "port on the host only");
  refused("http://192.168.1.250.evil.example", "192.168.1.250", "our host as a prefix");
  refused("http://192.168.1.25", "192.168.1.250", "a truncated address");
  refused("null", "192.168.1.250", "opaque origin from a sandboxed frame");
  refused("192.168.1.250", "192.168.1.250", "authority with no scheme");
  refused("http://", "192.168.1.250", "scheme with an empty authority");

  // A missing or empty Host cannot be matched against, so nothing is allowed
  // through on the strength of it.
  refused("http://192.168.1.250", nullptr, "Origin present, Host missing");
  refused("http://192.168.1.250", "", "Origin present, Host empty");

  if (failures != 0) {
    std::printf("web console origin guard: %d of %d checks FAILED\n", failures, checks);
    return 1;
  }
  std::printf("web console origin guard: %d checks passed\n", checks);
  return 0;
}
