// Execute the production lookup/parser; only HTTP and task scheduling are faked.
#include "location_estimate.hpp"
#include <esp_http_client.h>
#include <freertos/task.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

struct esp_http_client {std::string body;size_t offset=0;};
namespace {
TaskFunction_t pending=nullptr;
eTaskState task_state=eRunning;int created=0,deleted=0;
bool task_ok=true;
int requests=0,status=200;
std::string address_body=R"([{"lat":"52.5","lon":"13.4","display_name":"Berlin"}])";
std::string ip_body=R"({"latitude":40.7,"longitude":-74.0,"city":"IP test area"})";
void run(){assert(pending);auto task=pending;pending=nullptr;task(nullptr);}
void ready(int32_t lat,int32_t lon){auto s=orcsdr::location_estimate::state();assert(s.ready&&!s.busy&&s.latitude_e7==lat&&s.longitude_e7==lon);}
}
extern "C" esp_err_t esp_crt_bundle_attach(void*){return ESP_OK;}
extern "C" esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t* config){
  assert(config->crt_bundle_attach&&config->user_agent&&config->timeout_ms==12000);
  assert(std::string(config->url).rfind("https://",0)==0);++requests;
  return new esp_http_client{std::strstr(config->url,"nominatim")?address_body:ip_body};
}
extern "C" esp_err_t esp_http_client_open(esp_http_client_handle_t,int){return ESP_OK;}
extern "C" int64_t esp_http_client_fetch_headers(esp_http_client_handle_t client){return client->body.size();}
extern "C" int esp_http_client_get_status_code(esp_http_client_handle_t){return status;}
extern "C" int esp_http_client_read(esp_http_client_handle_t client,char* out,int capacity){
  const size_t n=std::min<size_t>(capacity,client->body.size()-client->offset);
  std::memcpy(out,client->body.data()+client->offset,n);client->offset+=n;return static_cast<int>(n);
}
extern "C" bool esp_http_client_is_complete_data_received(esp_http_client_handle_t client){return client->offset==client->body.size();}
extern "C" esp_err_t esp_http_client_close(esp_http_client_handle_t){return ESP_OK;}
extern "C" esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client){delete client;return ESP_OK;}
int xTaskCreatePinnedToCoreWithCaps(TaskFunction_t task,const char*,unsigned bytes,void*,unsigned,TaskHandle_t* handle,int core,unsigned caps){
  assert(bytes==8192&&core==1&&caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!task_ok)return 0;assert(!pending&&created==deleted);pending=task;task_state=eRunning;*handle=reinterpret_cast<void*>(1);++created;return pdPASS;
}
void vTaskSuspend(TaskHandle_t task){assert(!task);task_state=eSuspended;}
void vTaskDeleteWithCaps(TaskHandle_t task){assert(task==reinterpret_cast<void*>(1)&&task_state==eSuspended);++deleted;}
eTaskState eTaskGetState(TaskHandle_t task){assert(task==reinterpret_cast<void*>(1));return task_state;}

int main(){
  using namespace orcsdr::location_estimate;
  assert(self_check());
  assert(!request("Berlin",false)&&!state().ready);
  assert(request("Berlin",true)&&state().busy);
  assert(!request("Other",true));run();ready(525000000,134000000);
  assert(request("Berlin",true)&&!state().busy&&!pending&&requests==1);
  assert(request(true));run();ready(407000000,-740000000);
  assert(request("Berlin",true)&&state().busy&&pending); // IP result must not masquerade as cached address.
  run();ready(525000000,134000000);assert(requests==3);
  for(const char* bad:{"","oops","52.5junk","nan","inf","91"}){
    address_body=std::string("[{\"lat\":\"")+bad+"\",\"lon\":\"13.4\"}]";
    assert(request(bad[0]?bad:"empty",true));run();assert(!state().ready&&!state().busy);
  }
  address_body=R"([{"lat":"-33.9","lon":"151.2","display_name":"Sydney"}])";
  assert(request("Sydney",true));run();ready(-339000000,1512000000);
  status=503;assert(request("Unavailable",true));run();assert(!state().ready);status=200;
  task_ok=false;assert(!request("Task failure",true)&&!state().busy&&!pending);task_ok=true;
  assert(request("Sydney",true));run();ready(-339000000,1512000000);
  for(int i=0;i<20;++i){char query[24];std::snprintf(query,sizeof(query),"repeat-%d",i);assert(request(query,true));run();ready(-339000000,1512000000);assert(created==deleted);}
  std::puts("LOCATION_LOOKUP_PASS cache identity, busy ownership, malformed coordinates, HTTP/task failures; no network or persistence");
}
