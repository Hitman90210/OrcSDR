#include "map_preferences.hpp"
#include "nvs_store.hpp"
#include <cstddef>
namespace orcsdr::maps::preferences {
namespace {
constexpr const char* keys[]={"picker_labels","lora_labels","adsb_labels","ft8_labels"};
bool enabled[]={true,true,true,true};
bool valid(Consumer consumer){return static_cast<size_t>(consumer)<static_cast<size_t>(Consumer::count);}
}
void load() {
  NvsStore store;
  if(!store.begin("mapview",true))return;
  for(size_t i=0;i<static_cast<size_t>(Consumer::count);++i)enabled[i]=store.get_bool(keys[i],true);
}
bool labels(Consumer consumer){return valid(consumer)&&enabled[static_cast<size_t>(consumer)];}
bool set_labels(Consumer consumer,bool value) {
  if(!valid(consumer))return false;
  const auto index=static_cast<size_t>(consumer);
  if(enabled[index]==value)return true;
  NvsStore store;
  if(!store.begin("mapview")||!store.put_bool(keys[index],value)||store.get_bool(keys[index],!value)!=value)return false;
  enabled[index]=value;return true;
}
}
