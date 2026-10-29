#include "map_delivery_nvs.hpp"
#include <nvs.h>
namespace orcsdr::maps::delivery {
namespace {
constexpr char kNamespace[]="orcmapsdl",kKey[]="state_v1";
struct Handle {nvs_handle_t value;~Handle(){nvs_close(value);}};
}
Read NvsStateStore::read(std::string* output) {
  if(!output)return Read::error;
  nvs_handle_t handle=0;
  const auto opened=nvs_open(kNamespace,NVS_READONLY,&handle);
  if(opened==ESP_ERR_NVS_NOT_FOUND)return Read::missing;
  if(opened!=ESP_OK)return Read::error;
  Handle close{handle};size_t size=0;
  const auto result=nvs_get_blob(handle,kKey,nullptr,&size);
  if(result==ESP_ERR_NVS_NOT_FOUND)return Read::missing;
  if(result!=ESP_OK||size==0||size>kMaximumStateBytes)return Read::error;
  std::string bytes(size,'\0');
  if(nvs_get_blob(handle,kKey,bytes.data(),&size)!=ESP_OK||size!=bytes.size())return Read::error;
  *output=std::move(bytes);return Read::ok;
}
bool NvsStateStore::write(std::string_view bytes) {
  if(bytes.empty()||bytes.size()>kMaximumStateBytes)return false;
  nvs_handle_t handle=0;
  if(nvs_open(kNamespace,NVS_READWRITE,&handle)!=ESP_OK)return false;
  Handle close{handle};
  return nvs_set_blob(handle,kKey,bytes.data(),bytes.size())==ESP_OK&&nvs_commit(handle)==ESP_OK;
}
}
