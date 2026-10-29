#pragma once
#include <cstdint>
namespace orcsdr::maps::preferences {
// UI-task owned. Rendering receives an immutable copy in each request.
enum class Consumer:uint8_t {picker,lora,adsb,ft8,count};
void load();
bool labels(Consumer);
bool set_labels(Consumer,bool);
}
