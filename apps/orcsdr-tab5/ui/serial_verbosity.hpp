#pragma once

#include <cstdint>

enum class SerialVerbosity : uint8_t { quiet = 0, normal = 1, debug = 2, trace = 3 };

bool serial_verbosity_at(SerialVerbosity level);
SerialVerbosity current_serial_verbosity();
const char* serial_verbosity_name(SerialVerbosity level);
bool parse_serial_verbosity(const char* text, SerialVerbosity* out);
void apply_serial_verbosity(SerialVerbosity level);
