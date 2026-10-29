#include "map_download_response.hpp"
#include <cstdio>
#include <limits>
#include <string>

namespace d=orcsdr::maps::download;
namespace {
int failures=0;
void check(bool ok,const char* what) {if(!ok){std::printf("FAIL: %s\n",what);++failures;}}
const std::string hash(64,'a');
void resume_identity() {
  const d::Identity wanted{1000,hash};
  check(d::can_resume(wanted,{wanted,"\"object-v1\""},400),"same artifact resumes");
  check(!d::can_resume(wanted,{{1001,hash},"\"object-v1\""},400),"changed size restarts");
  check(!d::can_resume(wanted,{{1000,std::string(64,'b')},"\"object-v1\""},400),"changed hash restarts");
  check(d::can_resume(wanted,{{1000,std::string(64,'A')},"\"object-v1\""},400),"hash case is not a new artifact");
  for(const auto bad:{"", "W/\"weak\"", "unquoted", "\"a\"b\"", "\"line\rbreak\""})
    check(!d::can_resume(wanted,{wanted,bad},400),"invalid or weak entity tag restarts");
  check(!d::can_resume(wanted,{wanted,"\"v1\""},0),"zero prefix starts fresh");
  check(!d::can_resume(wanted,{wanted,"\"v1\""},1000),"complete prefix requires verification, not HTTP");
  check(!d::can_resume(wanted,{wanted,"\"v1\""},1001),"oversized saved prefix rejected");
  const std::string longest_tag="\""+std::string(126,'x')+"\"";
  check(d::can_resume(wanted,{wanted,longest_tag},400),"128-byte strong validator accepted");
  const std::string oversized_tag="\""+std::string(127,'x')+"\"";
  check(!d::can_resume(wanted,{wanted,oversized_tag},400),"129-byte validator rejected");
  check(!d::can_resume({1000,std::string(64,'g')},{{1000,std::string(64,'g')},"\"v1\""},400),"invalid digest rejected");
}
void responses() {
  d::Request request{1000,400,600,"\"v1\""};
  d::Response response{206,true,600,"bytes 400-999/1000","\"v1\"",""};
  check(d::validate_response(request,response)==d::Error::none,"valid resumed response");
  auto modified=response;modified.status=200;
  check(d::validate_response(request,modified)==d::Error::status,"ignored range must never append whole object");
  for(const auto status:{301,302,404,416,500}) {
    modified=response;modified.status=status;
    check(d::validate_response(request,modified)==d::Error::status,"redirect/error is not payload");
  }
  for(const auto tag:{"", "W/\"v1\"", "\"v2\""}) {
    modified=response;modified.etag=tag;
    check(d::validate_response(request,modified)==d::Error::identity,"changed/missing/weak identity rejected");
  }
  modified=response;modified.has_length=false;
  check(d::validate_response(request,modified)==d::Error::length,"unknown body length rejected by installer policy");
  modified=response;modified.length=601;
  check(d::validate_response(request,modified)==d::Error::length,"body length mismatch rejected");
  modified=response;modified.encoding="gzip";
  check(d::validate_response(request,modified)==d::Error::encoding,"encoded range cannot mix with raw archive bytes");
  modified.encoding="IDENTITY";
  check(d::validate_response(request,modified)==d::Error::none,"identity coding is case insensitive");
  modified=response;modified.etag="\"V1\"";
  check(d::validate_response(request,modified)==d::Error::identity,"ETag identity is case sensitive");
  const std::string standard_range="bytes 400-999/1000";
  const std::string longest_range="bytes "+std::string(128-standard_range.size(),'0')+"400-999/1000";
  modified=response;modified.range=longest_range;
  check(d::validate_response(request,modified)==d::Error::none,"128-byte numeric range accepted");
  const std::string oversized_range="bytes "+std::string(129-standard_range.size(),'0')+"400-999/1000";
  modified.range=oversized_range;
  check(d::validate_response(request,modified)==d::Error::range,"129-byte range rejected");
  for(const auto range:{"", "items 400-999/1000", "bytes 399-998/1000", "bytes 400-998/1000",
      "bytes 400-1000/1000", "bytes 400-999/1001", "bytes 400-999/*", "bytes */1000",
      "bytes +400-999/1000", "bytes 400-999/1000junk", "bytes 400-999/1000,0-1/1000",
      "bytes 18446744073709551616-999/1000", "bytes 400-18446744073709551616/1000",
      "bytes 400-999/18446744073709551616", "bytes 400-999/1000\r\n"}) {
    modified=response;modified.range=range;
    check(d::validate_response(request,modified)==d::Error::range,"invalid/unexpected range rejected");
  }
  auto bad=request;bad.etag="";
  check(d::validate_response(bad,response)==d::Error::identity,"resume requires saved strong validator");
  bad=request;bad.offset=1000;
  check(d::validate_response(bad,response)==d::Error::request,"offset beyond end rejected");
  bad=request;bad.bytes=0;
  check(d::validate_response(bad,response)==d::Error::request,"zero range rejected");
  bad=request;bad.bytes=std::numeric_limits<uint64_t>::max();
  check(d::validate_response(bad,response)==d::Error::request,"overflowing requested end rejected");

  const d::Request fresh{1000,0,1000,{}};
  const d::Response full{200,true,1000,{},{},{}};
  check(d::validate_response(fresh,full)==d::Error::none,"fresh complete response needs final hash, not ETag");
  modified=full;modified.range="bytes 0-999/1000";
  check(d::validate_response(fresh,modified)==d::Error::range,"200 with unexpected range rejected");
  const d::Request first_chunk{1000,0,400,{}};
  const d::Response chunk{206,true,400,"BYTES 0-399/1000","\"v1\"",{}};
  check(d::validate_response(first_chunk,chunk)==d::Error::none,"initial bounded chunk accepted");
  modified=chunk;modified.etag="";
  check(d::validate_response(first_chunk,modified)==d::Error::identity,"bounded transfer requires strong object identity");
  check(d::validate_response(first_chunk,full)==d::Error::status,"ignored bounded initial range requires explicit restart");
  const auto max=std::numeric_limits<uint64_t>::max();
  check(d::validate_response({max,max-1,1,"\"v1\""},
      {206,true,1,"bytes 18446744073709551614-18446744073709551614/18446744073709551615","\"v1\"",{}})
      ==d::Error::none,"largest valid range avoids end overflow");
}
}
int main(){resume_identity();responses();std::printf("download response checks: %s\n",failures?"FAIL":"PASS");return failures?1:0;}
