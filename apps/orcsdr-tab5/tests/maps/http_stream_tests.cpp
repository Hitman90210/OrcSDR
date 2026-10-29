#include "map_http_stream.hpp"
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>
namespace {
struct Response {
  int status=200;
  int64_t length=8;
  std::vector<std::pair<std::string,std::string>> headers={{"Content-Length","8"},{"ETag","\"v1\""}};
  std::string body="abcdefgh";
  bool chunked=false,init_fail=false,header_fail=false,open_fail=false,read_fail=false;
};
struct Trace {
  std::string url;
  std::map<std::string,std::string> headers;
  bool opened=false,closed=false,cleaned=false;
};
std::deque<Response> responses;
std::vector<Trace> traces;
int live=0,reads=0;
void reset() {assert(live==0);responses.clear();traces.clear();reads=0;}
}
struct esp_http_client {
  esp_http_client_config_t config;
  Response response;
  size_t trace=0,offset=0;
};
extern "C" {
esp_err_t esp_crt_bundle_attach(void*){return ESP_OK;}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t* config) {
  assert(config&&config->crt_bundle_attach==esp_crt_bundle_attach);
  assert(config->transport_type==HTTP_TRANSPORT_OVER_SSL&&config->disable_auto_redirect);
  assert(config->timeout_ms==5000&&config->buffer_size==4096&&config->buffer_size_tx==2048);
  assert(!responses.empty());auto response=responses.front();responses.pop_front();
  if(response.init_fail)return nullptr;
  auto* client=new esp_http_client{*config,std::move(response),traces.size(),0};
  traces.push_back({config->url,{}});++live;return client;
}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t client,const char* key,const char* value) {
  if(client->response.header_fail)return ESP_FAIL;
  traces[client->trace].headers[key]=value;return ESP_OK;
}
esp_err_t esp_http_client_open(esp_http_client_handle_t client,int) {
  if(client->response.open_fail)return ESP_FAIL;
  traces[client->trace].opened=true;return ESP_OK;
}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t client) {
  for(auto& header:client->response.headers) {
    esp_http_client_event_t event{HTTP_EVENT_ON_HEADER,client->config.user_data,header.first.data(),header.second.data()};
    // Do not rely on the fake SDK honoring callback failure: the adapter must
    // retain its own invalid-header state and refuse the response afterwards.
    client->config.event_handler(&event);
  }
  return client->response.length;
}
int esp_http_client_get_status_code(esp_http_client_handle_t client){return client->response.status;}
bool esp_http_client_is_chunked_response(esp_http_client_handle_t client){return client->response.chunked;}
int esp_http_client_read(esp_http_client_handle_t client,char* data,int size) {
  ++reads;if(client->response.read_fail)return -1;
  assert(size>=0);const auto count=std::min<size_t>(size,client->response.body.size()-client->offset);
  std::memcpy(data,client->response.body.data()+client->offset,count);client->offset+=count;
  return static_cast<int>(count);
}
esp_err_t esp_http_client_close(esp_http_client_handle_t client){traces[client->trace].closed=true;return ESP_OK;}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client) {
  assert(traces[client->trace].closed);traces[client->trace].cleaned=true;delete client;--live;return ESP_OK;
}
}
int main() {
  esp_http_client_config_t unset{};assert(unset.transport_type!=HTTP_TRANSPORT_OVER_SSL);
  using namespace orcsdr::maps;
  using transfer::HttpStream;using transfer::Headers;
  const char* url="https://maps.example.test/world.pmtiles";
  const download::Request whole{8,0,8,{}},partial{8,2,3,"\"v1\""};
  reset();
  {
    HttpStream stream;Headers headers;responses.emplace_back();
    assert(stream.open(url,whole,headers));
    assert(headers.status==200&&headers.has_length&&headers.length==8&&headers.etag=="\"v1\"");
    assert(traces[0].headers.at("Accept-Encoding")=="identity"&&!traces[0].headers.count("Range"));
    uint8_t body[8]{};assert(stream.read(body,3)==3&&!std::memcmp(body,"abc",3));
    assert(stream.read(body,8)==5&&!std::memcmp(body,"defgh",5)&&stream.read(body,8)==0);
    const int before=reads;
    assert(stream.read(nullptr,1)==-1&&stream.read(body,size_t{INT_MAX}+1)==-1&&reads==before);
    stream.close();stream.close();assert(live==0&&traces[0].cleaned&&stream.read(body,1)==-1);
  }
  reset();
  {
    HttpStream stream;Headers headers;
    Response redirect;redirect.status=302;redirect.headers={{"Location","https://mirror.example.test/map.pmtiles?token=test"}};
    responses.push_back(redirect);
    Response range;range.status=206;range.length=3;range.body="cde";
    range.headers={{"Content-Length","3"},{"Content-Range","bytes 2-4/8"},{"ETag","\"v1\""}};
    responses.push_back(range);
    assert(stream.open(url,partial,headers)&&traces.size()==2&&traces[0].cleaned);
    for(const auto& trace:traces) {
      assert(trace.headers.at("Range")=="bytes=2-4"&&trace.headers.at("If-Range")=="\"v1\"");
      assert(trace.headers.at("Accept-Encoding")=="identity");
    }
    assert(headers.status==206&&headers.length==3&&headers.range=="bytes 2-4/8");
    uint8_t data[4];assert(stream.read(data,4)==3&&!std::memcmp(data,"cde",3));
    // Reopening closes the previous handle before accepting a new response.
    responses.emplace_back();assert(stream.open(url,whole,headers)&&traces[1].cleaned&&live==1);
  }
  assert(live==0);
  for(const char* endpoint:{"http://maps.example.test/a","https://user:pass@maps.example.test/a","https://maps.example.test/a\r\nX: bad"}) {
    reset();HttpStream stream;Headers headers;assert(!stream.open(endpoint,whole,headers)&&traces.empty());
  }
  for(const auto request:{download::Request{0,1,1,{}},download::Request{8,8,1,{}},download::Request{8,0,0,{}},download::Request{8,7,2,{}}}) {
    reset();HttpStream stream;Headers headers;assert(!stream.open(url,request,headers)&&traces.empty());
  }
  for(const char* tag:{"W/\"v1\"","v1","\"bad\r\ntag\""}) {
    reset();HttpStream stream;Headers headers;responses.emplace_back();
    assert(!stream.open(url,{8,2,3,tag},headers)&&live==0&&traces[0].cleaned&&!traces[0].opened);
  }
  for(int fault=0;fault<5;++fault) {
    reset();HttpStream stream;Headers headers;headers.status=987;
    Response response;
    if(fault==0)response.init_fail=true;
    if(fault==1)response.header_fail=true;
    if(fault==2)response.open_fail=true;
    if(fault==3)response.length=-1;
    if(fault==4)response.headers.push_back({"content-length","8"});
    responses.push_back(response);
    assert(!stream.open(url,whole,headers)&&live==0&&headers.status==987);
    for(const auto& trace:traces)assert(trace.cleaned);
  }
  for(const char* next:{"http://mirror.example.test/a","/relative","https://user@mirror.example.test/a"}) {
    reset();HttpStream stream;Headers headers;Response redirect;
    redirect.status=307;redirect.headers={{"Location",next}};responses.push_back(redirect);
    assert(!stream.open(url,partial,headers)&&live==0&&traces.size()==1&&traces[0].cleaned);
  }
  reset();
  {
    HttpStream stream;Headers headers;
    for(int i=0;i<5;++i){Response r;r.status=301;r.headers={{"Location",url}};responses.push_back(r);}
    assert(!stream.open(url,whole,headers)&&traces.size()==5&&live==0);
    for(const auto& trace:traces)assert(trace.cleaned);
  }
  reset();
  {
    HttpStream stream;Headers headers;
    for(int i=0;i<4;++i){Response r;r.status=308;r.headers={{"Location",url}};responses.push_back(r);}
    Response final;final.chunked=true;final.read_fail=true;responses.push_back(final);
    assert(stream.open(url,whole,headers)&&traces.size()==5&&!headers.has_length);
    uint8_t byte;assert(stream.read(&byte,1)==-1);
  }
  assert(live==0);
  std::puts("MAP_HTTP_STREAM_PASS TLS config, resume headers, redirects, bounded reads and failure cleanup; no network used");
}
