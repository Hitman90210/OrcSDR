#pragma once
#if __has_include("map_catalog_configuration.hpp")
#include "map_catalog_configuration.hpp"
#else
namespace orcsdr::maps::catalog_source {
inline constexpr char base_url[]="",public_key[]="";
}
#endif
