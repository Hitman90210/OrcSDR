#pragma once
#include <cstdint>
#include <limits>
#include <string_view>

namespace orcsdr::maps::download {
// Pure response checks for the shared map-acquisition adapter. No network,
// filesystem, authentication or hash verification takes place in this header.
struct Identity {uint64_t bytes=0;std::string_view sha256;};
struct ResumeRecord {Identity artifact;std::string_view etag;};
struct Request {uint64_t total=0,offset=0,bytes=0;std::string_view etag;};
struct Response {
  int status=0;
  bool has_length=false;
  uint64_t length=0;
  std::string_view range,etag,encoding;
};
enum class Error {none,request,status,encoding,length,identity,range};

namespace detail {
inline char lower(char c) {return c>='A'&&c<='Z'?static_cast<char>(c+'a'-'A'):c;}
inline bool equal_ascii(std::string_view a,std::string_view b) {
  if(a.size()!=b.size())return false;
  for(size_t i=0;i<a.size();++i)if(lower(a[i])!=lower(b[i]))return false;
  return true;
}
inline bool digest(std::string_view text) {
  if(text.size()!=64)return false;
  for(char c:text)if(!((c>='0'&&c<='9')||(lower(c)>='a'&&lower(c)<='f')))return false;
  return true;
}
inline bool strong_etag(std::string_view tag) {
  // Bounded representation for a persisted If-Range validator. Quotes are
  // part of the validator; weak tags and control characters cannot be sent.
  if(tag.size()<2||tag.size()>128||tag.front()!='"'||tag.back()!='"')return false;
  for(size_t i=1;i+1<tag.size();++i) {
    const auto c=static_cast<unsigned char>(tag[i]);
    if(c<=0x20||c==0x7f||c=='"')return false;
  }
  return true;
}
inline bool number(std::string_view& text,uint64_t& out) {
  if(text.empty()||text.front()<'0'||text.front()>'9')return false;
  uint64_t value=0;
  while(!text.empty()&&text.front()>='0'&&text.front()<='9') {
    const uint64_t digit=static_cast<uint64_t>(text.front()-'0');
    if(value>(std::numeric_limits<uint64_t>::max()-digit)/10)return false;
    value=value*10+digit;text.remove_prefix(1);
  }
  out=value;return true;
}
inline bool consume(std::string_view& text,char c) {
  if(text.empty()||text.front()!=c)return false;
  text.remove_prefix(1);return true;
}
inline bool content_range(std::string_view text,uint64_t& first,uint64_t& last,uint64_t& total) {
  // One known-length byte range only. Multipart, wildcard and oversized
  // headers are deliberately outside this installer's response contract.
  if(text.size()>128||!equal_ascii(text.substr(0,6),"bytes "))return false;
  text.remove_prefix(6);
  return number(text,first)&&consume(text,'-')&&number(text,last)&&consume(text,'/')&&
      number(text,total)&&text.empty()&&first<=last&&last<total;
}
}

// expected must come from the authenticated catalog. saved describes only the
// synced staging prefix; compare before opening a range request or appending.
// A completed staging file goes to full hash/schema verification, not resume.
inline bool can_resume(const Identity& expected,const ResumeRecord& saved,uint64_t synced_bytes) {
  return expected.bytes>0&&expected.bytes==saved.artifact.bytes&&
      detail::digest(expected.sha256)&&detail::digest(saved.artifact.sha256)&&
      detail::equal_ascii(expected.sha256,saved.artifact.sha256)&&
      synced_bytes>0&&synced_bytes<expected.bytes&&detail::strong_etag(saved.etag);
}

// Accept only exact declared lengths and unencoded archive bytes. This is
// stricter than general HTTP: unknown lengths are not installable here.
// Rejection never means truncate/delete/activate; the adapter must preserve
// working data and explicitly start a new staging transfer if appropriate.
inline Error validate_response(const Request& request,const Response& response) {
  if(request.total==0||request.offset>=request.total||request.bytes==0||
      request.bytes>request.total-request.offset)return Error::request;
  if(response.status!=200&&response.status!=206)return Error::status;
  if(response.status==200&&(request.offset!=0||request.bytes!=request.total))return Error::status;
  if(!response.encoding.empty()&&!detail::equal_ascii(response.encoding,"identity"))return Error::encoding;
  if(!response.has_length||response.length!=request.bytes)return Error::length;
  if(request.offset>0||request.bytes<request.total||!request.etag.empty()) {
    if(!detail::strong_etag(response.etag)||
        (request.offset>0&&!detail::strong_etag(request.etag))||
        (!request.etag.empty()&&request.etag!=response.etag))return Error::identity;
  }
  if(response.status==200)return response.range.empty()?Error::none:Error::range;
  uint64_t first=0,last=0,total=0;
  if(!detail::content_range(response.range,first,last,total)||first!=request.offset||
      total!=request.total||last!=request.offset+(request.bytes-1))return Error::range;
  return Error::none;
}
} // namespace orcsdr::maps::download
