#include "map_http_stream.hpp"
#include "map_http_headers.hpp"
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <climits>
#include <cstring>
#include <new>

namespace orcsdr::maps::transfer {
struct HttpStream::State {
  esp_http_client_handle_t client=nullptr;
  HeaderCapture capture;
  static esp_err_t event(esp_http_client_event_t* event) {
    if(event->event_id!=HTTP_EVENT_ON_HEADER)return ESP_OK;
    auto* state=static_cast<State*>(event->user_data);
    if(!event->header_key||!event->header_value)return ESP_FAIL;
    const auto length=strnlen(event->header_value,2049);
    try {
      if(!state->capture.add(event->header_key,{event->header_value,length}))return ESP_FAIL;
    }catch(const std::bad_alloc&){state->capture.valid=false;return ESP_ERR_NO_MEM;}
    return ESP_OK;
  }
};
HttpStream::HttpStream() noexcept=default;
HttpStream::~HttpStream(){close();}
void HttpStream::close() {
  if(!state_||!state_->client)return;
  esp_http_client_close(state_->client);esp_http_client_cleanup(state_->client);
  state_->client=nullptr;
}
bool HttpStream::open(std::string_view endpoint,const download::Request& request,Headers& output) {
  close();
  if(!https_endpoint(endpoint))return false;
  if((request.total||request.offset||request.bytes)&&(!request.total||request.offset>=request.total||
      !request.bytes||request.bytes>request.total-request.offset))return false;
  if(!state_)state_=std::make_unique<State>();
  std::string url(endpoint);
  for(int hop=0;hop<=4;++hop) {
    state_->capture={};
    esp_http_client_config_t config{};
    config.url=url.c_str();config.timeout_ms=5000;
    config.transport_type=HTTP_TRANSPORT_OVER_SSL;
    config.crt_bundle_attach=esp_crt_bundle_attach;
    config.disable_auto_redirect=true;config.buffer_size=4096;config.buffer_size_tx=2048;
    config.event_handler=State::event;config.user_data=state_.get();
    state_->client=esp_http_client_init(&config);
    auto fail=[this](){close();return false;};
    if(!state_->client)return false;
    if(esp_http_client_set_header(state_->client,"Accept-Encoding","identity")!=ESP_OK)return fail();
    if(request.total&&(request.offset||request.bytes!=request.total)) {
      const auto range="bytes="+std::to_string(request.offset)+"-"+
          std::to_string(request.offset+request.bytes-1);
      if(esp_http_client_set_header(state_->client,"Range",range.c_str())!=ESP_OK)return fail();
    }
    if(request.offset) {
      if(!download::detail::strong_etag(request.etag))return fail();
      const std::string tag(request.etag);
      if(esp_http_client_set_header(state_->client,"If-Range",tag.c_str())!=ESP_OK)return fail();
    }
    if(esp_http_client_open(state_->client,0)!=ESP_OK)return fail();
    const auto length=esp_http_client_fetch_headers(state_->client);
    const int status=esp_http_client_get_status_code(state_->client);
    if(length<0||!state_->capture.valid)return fail();
    if(status==301||status==302||status==303||status==307||status==308) {
      const auto next=state_->capture.location;
      if(hop==4||!https_endpoint(next))return fail();
      close();url=next;continue;
    }
    output=state_->capture.value;output.status=status;
    output.has_length=state_->capture.seen[0]&&!esp_http_client_is_chunked_response(state_->client);
    output.length=static_cast<uint64_t>(length);return true;
  }
  return false;
}
int HttpStream::read(uint8_t* data,size_t bytes) {
  if(!state_||!state_->client||!data||bytes>INT_MAX)return -1;
  return esp_http_client_read(state_->client,reinterpret_cast<char*>(data),static_cast<int>(bytes));
}
} // namespace orcsdr::maps::transfer
