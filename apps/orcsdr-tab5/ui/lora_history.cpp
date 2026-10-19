#include "lora_history.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace orcsdr::lora_history {
namespace {
template<size_t N> void copy(char (&out)[N], const char* value) {
  std::snprintf(out,N,"%s",value);
}
bool position(const lora_log::Record& r) {
  return r.latitude_e7>=-900000000 && r.latitude_e7<=900000000 &&
         r.longitude_e7>=-1800000000 && r.longitude_e7<=1800000000;
}
bool take(const char*& p,const char* text) {
  const size_t n=std::strlen(text);
  if(std::strncmp(p,text,n)!=0)return false;
  p+=n;return true;
}
bool number(const char*& p,int64_t low,int64_t high,int64_t& value) {
  if(!(*p>='0'&&*p<='9') && !(*p=='-' && p[1]>='0'&&p[1]<='9'))return false;
  char* end=nullptr;errno=0;const long long v=std::strtoll(p,&end,10);
  if(errno || end==p || v<low || v>high)return false;
  value=v;p=end;return true;
}
int hex(char c) { return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1; }
template<size_t N> bool read_hex(const char*& p,char (&out)[N]) {
  if(!take(p,"\""))return false;
  size_t n=0;
  while(*p && *p!='\"') {
    if(n+1>=N || !p[1] || hex(p[0])<0 || hex(p[1])<0)return false;
    const char c=static_cast<char>(hex(p[0])*16+hex(p[1]));
    if(!c)return false;
    out[n++]=c;p+=2;
  }
  out[n]=0;return take(p,"\"");
}
template<size_t N> void to_hex(const char (&in)[N],char* out) {
  constexpr char digits[]="0123456789abcdef";size_t n=0;
  for(size_t i=0;i<N && in[i];++i) {
    const auto c=static_cast<unsigned char>(in[i]);out[n++]=digits[c>>4];out[n++]=digits[c&15];
  }
  out[n]=0;
}
}
void Store::clear() {
  count_ = next_key_ = 0;
  for (auto& key : recent_) key = {};
}
const Node* Store::find(uint32_t id) const {
  for(size_t i=0;i<count_;++i)if(nodes_[i].id==id)return &nodes_[i];
  return nullptr;
}
bool Store::observe(const lora_log::Record& r) {
  if(!r.sender || r.from_host)return false;
  if(r.packet_id) {
    for(const auto& key:recent_)if(key.sender==r.sender && key.packet==r.packet_id)return false;
    recent_[next_key_]={r.sender,r.packet_id};next_key_=(next_key_+1)%128;
  }
  size_t i=0;while(i<count_ && nodes_[i].id!=r.sender)++i;
  Node node{};
  if(i<count_)node=nodes_[i];
  else {node.id=r.sender;node.first_utc=r.received_utc;i=std::min(count_,kNodeCapacity-1);if(count_<kNodeCapacity)++count_;}
  node.last_utc=r.received_utc;node.last_ms=r.received_ms;
  if(node.packets<UINT32_MAX)++node.packets;
  if(r.signal_tenths!=INT16_MAX)node.signal_tenths=r.signal_tenths;
  if(r.snr_tenths!=INT16_MAX)node.snr_tenths=r.snr_tenths;
  // encrypted describes the RF frame, including successfully decrypted payloads.
  if(position(r)) {
    node.latitude_e7=r.latitude_e7;node.longitude_e7=r.longitude_e7;node.position_utc=r.received_utc;
  }
  if(r.port==4) {
    if(r.short_name[0])copy(node.short_name,r.short_name);
    if(r.long_name[0])copy(node.long_name,r.long_name);
    if(r.short_name[0]||r.long_name[0])node.name_utc=r.received_utc;
  }
  if(i)std::memmove(nodes_+1,nodes_,sizeof(Node)*i);
  nodes_[0]=node;return true;
}
size_t format_line(const lora_log::Record& r,char* out,size_t size) {
  char sn[17],ln[65],text[225];to_hex(r.short_name,sn);to_hex(r.long_name,ln);to_hex(r.text,text);
  const int n=std::snprintf(out,size,
    "{\"v\":1,\"up\":%lu,\"utc\":%lu,\"hz\":%lu,\"from\":%lu,\"to\":%lu,\"id\":%lu,\"port\":%u,\"lat\":%ld,\"lon\":%ld,\"snr\":%d,\"sig\":%d,\"enc\":%u,\"host\":%u,\"short_hex\":\"%s\",\"long_hex\":\"%s\",\"text_hex\":\"%s\"}\n",
    static_cast<unsigned long>(r.received_ms),static_cast<unsigned long>(r.received_utc),
    static_cast<unsigned long>(r.frequency_hz),static_cast<unsigned long>(r.sender),
    static_cast<unsigned long>(r.destination),static_cast<unsigned long>(r.packet_id),unsigned(r.port),
    static_cast<long>(r.latitude_e7),static_cast<long>(r.longitude_e7),int(r.snr_tenths),int(r.signal_tenths),
    unsigned(r.encrypted),unsigned(r.from_host),sn,ln,text);
  return n>0 && static_cast<size_t>(n)<size?static_cast<size_t>(n):0;
}
bool parse_line(const char* line,size_t size,lora_log::Record* out) {
  if(!line || !out || !size || size>=kLineMax || std::memchr(line,0,size))return false;
  char buffer[kLineMax];std::memcpy(buffer,line,size);buffer[size]=0;const char* p=buffer;
  lora_log::Record r{};int64_t n;
  if(!take(p,"{\"v\":1"))return false;
#define FIELD(key,member,low,high) if(!take(p,",\"" key "\":") || !number(p,low,high,n))return false; r.member=static_cast<decltype(r.member)>(n)
  FIELD("up",received_ms,0,UINT32_MAX);FIELD("utc",received_utc,0,UINT32_MAX);
  FIELD("hz",frequency_hz,0,UINT32_MAX);FIELD("from",sender,0,UINT32_MAX);
  FIELD("to",destination,0,UINT32_MAX);FIELD("id",packet_id,0,UINT32_MAX);
  FIELD("port",port,0,UINT16_MAX);FIELD("lat",latitude_e7,INT32_MIN,INT32_MAX);
  FIELD("lon",longitude_e7,INT32_MIN,INT32_MAX);FIELD("snr",snr_tenths,INT16_MIN,INT16_MAX);
  FIELD("sig",signal_tenths,INT16_MIN,INT16_MAX);FIELD("enc",encrypted,0,1);FIELD("host",from_host,0,1);
#undef FIELD
  if(!take(p,",\"short_hex\":") || !read_hex(p,r.short_name) ||
     !take(p,",\"long_hex\":") || !read_hex(p,r.long_name) ||
     !take(p,",\"text_hex\":") || !read_hex(p,r.text) || !take(p,"}"))return false;
  if(*p=='\r')++p;
  if(*p=='\n')++p;
  if(*p)return false;
  if((r.latitude_e7!=INT32_MAX || r.longitude_e7!=INT32_MAX) && !position(r))return false;
  *out=r;return true;
}
void day_key(uint32_t utc,char* out,size_t size) {
  const time_t value=utc;tm date{};
  if(!utc || !gmtime_r(&value,&date) || !std::strftime(out,size,"%Y%m%d",&date))
    std::snprintf(out,size,"undated");
}
}  // namespace orcsdr::lora_history
