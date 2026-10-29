#include "map_delivery_nvs.hpp"
#include <nvs.h>
#include <cstdio>
#include <cstring>
namespace d=orcsdr::maps::delivery;
namespace {
std::string value,pending;
bool namespace_exists=false,key_exists=false,open_error=false,get_error=false;
bool short_read=false,set_error=false,commit_error=false,wrong_target=false;
int opened=0,closed=0;
void key(const char* key){wrong_target|=std::strcmp(key,"state_v1")!=0;}
}
esp_err_t nvs_open(const char* name,nvs_open_mode_t mode,nvs_handle_t* handle) {
  wrong_target|=std::strcmp(name,"orcmapsdl")!=0;
  if(open_error)return ESP_FAIL;
  if(!namespace_exists&&mode==NVS_READONLY)return ESP_ERR_NVS_NOT_FOUND;
  namespace_exists=true;*handle=1;++opened;return ESP_OK;
}
void nvs_close(nvs_handle_t){++closed;}
esp_err_t nvs_get_blob(nvs_handle_t,const char* name,void* output,size_t* size) {
  key(name);if(get_error)return ESP_FAIL;
  if(!key_exists)return ESP_ERR_NVS_NOT_FOUND;
  if(!output){*size=value.size();return ESP_OK;}
  if(*size<value.size())return ESP_FAIL;
  std::memcpy(output,value.data(),value.size());*size=value.size()-(short_read?1:0);return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t,const char* name,const void* bytes,size_t size) {
  key(name);if(set_error)return ESP_FAIL;
  pending.assign(static_cast<const char*>(bytes),size);return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t) {
  if(commit_error)return ESP_FAIL;
  value=pending;key_exists=true;return ESP_OK;
}
int main() {
  int failures=0;
  auto check=[&](bool ok,const char* message){if(!ok){++failures;std::printf("FAIL %s\n",message);}};
  d::NvsStateStore store;std::string output="unchanged";
  check(store.read(&output)==d::Read::missing&&output=="unchanged","missing namespace distinguished");
  namespace_exists=true;
  check(store.read(&output)==d::Read::missing,"missing key distinguished");
  get_error=true;
  check(store.read(&output)==d::Read::error&&output=="unchanged","read failure preserves output");
  get_error=false;key_exists=true;value.assign(d::kMaximumStateBytes+1,'x');
  check(store.read(&output)==d::Read::error,"oversized NVS record bounded before allocation");
  value="sample";short_read=true;
  check(store.read(&output)==d::Read::error&&output=="unchanged","short read cannot return partial state");
  short_read=false;key_exists=false;
  d::State state;state.accepted_revision=1;state.catalog_sha256=std::string(64,'a');
  check(d::save(store,state)==d::Error::none,"production adapter persists verified state");
  const auto before=value;state.accepted_revision=2;set_error=true;
  check(d::save(store,state)==d::Error::storage&&value==before,"NVS set failure does not commit");
  set_error=false;commit_error=true;
  check(d::save(store,state)==d::Error::storage&&value==before,"NVS commit failure reported");
  commit_error=false;open_error=true;
  check(d::save(store,state)==d::Error::storage&&value==before,"NVS open failure cannot reset trust");
  open_error=false;
  check(d::save(store,state)==d::Error::none,"retry after storage recovery succeeds");
  check(!wrong_target&&opened==closed,"only isolated namespace/key used and all handles closed");
  std::printf("RESULT: delivery NVS %s\n",failures?"FAIL":"PASS");return failures?1:0;
}
