#include "map_transfer.hpp"
#include "orcmap/byte_source.hpp"
#include "orcmap/pmtiles.hpp"
#include "orcmap/pack_json.hpp"
#include <mbedtls/sha256.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <memory>
#include <new>

namespace orcsdr::maps::transfer {
namespace {
constexpr uint64_t kMaximum=600ULL*1024*1024;
constexpr uint64_t kCheckpoint=1024*1024;
constexpr uint64_t kSpaceReserve=1024*1024;
constexpr size_t kRecordBytes=250,kRecordBody=218;
std::string digest_text(const void* data,size_t size) {
  uint8_t hash[32]{};
  if(mbedtls_sha256(static_cast<const uint8_t*>(data),size,hash,0))return {};
  constexpr char hex[]="0123456789abcdef";std::string out;out.reserve(64);
  for(uint8_t c:hash){out+=hex[c>>4];out+=hex[c&15];}return out;
}
void put64(uint8_t* p,uint64_t value){for(int i=0;i<8;++i)p[i]=static_cast<uint8_t>(value>>(8*i));}
uint64_t get64(const uint8_t* p){uint64_t n=0;for(int i=7;i>=0;--i)n=(n<<8)|p[i];return n;}
struct Record {uint64_t offset=0;std::string etag;int slot=-1;};
struct CloseFile {storage::File& file;~CloseFile(){file.close();}};
struct CloseStream {Stream& stream;~CloseStream(){stream.close();}};
bool read_record(storage::FileSystem& fs,const Paths& paths,const orcmap::PackManifest& expected,
                 uint64_t physical_size,Record& result) {
  bool found=false;
  for(int slot=0;slot<2;++slot) {
    auto file=fs.open(paths.record[slot].c_str(),FILE_READ);CloseFile close{file};
    std::array<uint8_t,kRecordBytes> bytes{};uint8_t hash[32]{};
    if(!file||file.size()!=bytes.size()||file.read(bytes.data(),bytes.size())!=bytes.size()||
        mbedtls_sha256(bytes.data(),kRecordBody,hash,0)||
        std::memcmp(hash,bytes.data()+kRecordBody,32)||std::memcmp(bytes.data(),"ORCMRS01",8)||
        get64(bytes.data()+8)!=expected.size_bytes||
        std::memcmp(bytes.data()+24,expected.output_sha256.data(),64))continue;
    const auto offset=get64(bytes.data()+16);
    const size_t length=bytes[88]|static_cast<size_t>(bytes[89])<<8;
    if(offset>physical_size||offset>expected.size_bytes||length>128)continue;
    std::string tag(reinterpret_cast<const char*>(bytes.data()+90),length);
    if(!download::detail::strong_etag(tag))continue;
    if(!found||offset>result.offset){result={offset,std::move(tag),slot};found=true;}
  }
  return found;
}
bool checkpoint(storage::FileSystem& fs,const Paths& paths,const orcmap::PackManifest& expected,
                uint64_t offset,const std::string& etag,int& active) {
  std::array<uint8_t,kRecordBytes> bytes{};
  std::memcpy(bytes.data(),"ORCMRS01",8);put64(bytes.data()+8,expected.size_bytes);
  put64(bytes.data()+16,offset);std::memcpy(bytes.data()+24,expected.output_sha256.data(),64);
  bytes[88]=static_cast<uint8_t>(etag.size());bytes[89]=static_cast<uint8_t>(etag.size()>>8);
  std::memcpy(bytes.data()+90,etag.data(),etag.size());
  if(mbedtls_sha256(bytes.data(),kRecordBody,bytes.data()+kRecordBody,0))return false;
  const int next=active==0?1:0;
  if(fs.exists(paths.record[next].c_str())&&!fs.remove(paths.record[next].c_str()))return false;
  auto file=fs.open(paths.record[next].c_str(),FILE_WRITE,true);CloseFile close{file};
  if(!file||file.write(bytes.data(),bytes.size())!=bytes.size()||!file.sync()||!file.close())return false;
  active=next;return true;
}
class Source final:public orcmap::ByteSource {
 public:
  Source(storage::FileSystem& fs,const std::string& path,VerificationProfile* profile=nullptr):file_(fs.open(path.c_str(),FILE_READ)),profile_(profile){}
  ~Source(){file_.close();}
  size_t Read(uint64_t offset,void* data,size_t size)override{
    const auto started=std::chrono::steady_clock::now();
    const auto got=offset<=Size()&&size<=Size()-offset&&file_.seek(static_cast<size_t>(offset))?
        file_.read(static_cast<uint8_t*>(data),size):0;
    if(profile_) {
      profile_->bytes+=got;if(got)++profile_->blocks;
      profile_->read_us+=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now()-started).count());
    }
    return got;
  }
  uint64_t Size()const override{return file_?file_.size():0;}
  bool Valid()const override{return static_cast<bool>(file_);}
 private:storage::File file_;VerificationProfile* profile_;
};
Error verify_file(storage::FileSystem& fs,const std::string& path,const orcmap::PackManifest& expected,
                  uint8_t* buffer,size_t capacity,const Hooks& hooks) {
  using Clock=std::chrono::steady_clock;
  const auto started=Clock::now();
  VerificationProfile profile;
  const auto micros=[](auto start) {return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count());};
  const auto done=[&](Error result) {
    profile.result=result;profile.elapsed_us=micros(started);
    if(hooks.verify_profile)hooks.verify_profile(hooks.context,profile);
    return result;
  };
  auto file=fs.open(path.c_str(),FILE_READ);CloseFile close{file};
  if(!file||file.size()!=expected.size_bytes)return done(Error::storage);
  if(!file.close())return done(Error::storage);
  (void)buffer;(void)capacity;
  if(hooks.verify_progress)hooks.verify_progress(hooks.context,0,0);
  if(hooks.cancelled&&hooks.cancelled(hooks.context))return done(Error::cancelled);
  // Quick structural check only: size, bounded header/root directory and coverage.
  // Payload corruption is discovered on tile read, not by a full archive scan.
  Source source(fs,path,&profile);
  auto reader=std::make_unique<orcmap::PmTilesReader>(&source,1);
  if(!source.Valid()||!reader->Open())return done(Error::archive);
  const auto& h=reader->Header();const auto& b=expected.bounds;
  if(expected.min_zoom<h.min_zoom||expected.max_zoom>h.max_zoom||
      b.min_lon_deg<h.min_lon_e7/1e7-1e-6||b.max_lon_deg>h.max_lon_e7/1e7+1e-6||
      b.min_lat_deg<h.min_lat_e7/1e7-1e-6||b.max_lat_deg>h.max_lat_e7/1e7+1e-6)return done(Error::archive);
  if(hooks.cancelled&&hooks.cancelled(hooks.context))return done(Error::cancelled);
  if(hooks.verify_progress)hooks.verify_progress(hooks.context,profile.bytes,profile.bytes);
  return done(Error::none);
}
Error prepare(storage::FileSystem& fs,const catalog::Pack& pack,size_t mirror,Stream& stream,
              uint64_t free,uint8_t* buffer,size_t capacity,const Hooks& hooks) {
  orcmap::PackManifest expected;
  if(!buffer||capacity<4096||capacity>65536||mirror>=pack.urls.size()||
      orcmap::ParsePackManifestJson(pack.manifest_json,&expected)!=orcmap::PackJsonError::kNone||
      orcmap::ValidatePackManifest(expected)!=orcmap::PackValidationError::kNone||
      expected.output_sha256!=pack.manifest.output_sha256||expected.pack_id!=pack.manifest.pack_id||
      !expected.size_bytes||expected.size_bytes>kMaximum)return Error::input;
  for(char c:expected.output_sha256)if(c>='A'&&c<='F')return Error::input;
  if(hooks.cancelled&&hooks.cancelled(hooks.context))return Error::cancelled;
  const auto p=paths(pack);
  if(p.directory.empty())return Error::hash;
  if(!fs.exists("/orcmaps")&&!fs.mkdir("/orcmaps"))return Error::storage;
  if(!fs.exists(p.directory.c_str())&&!fs.mkdir(p.directory.c_str()))return Error::storage;
  std::string ready=p.archive;
  bool complete=fs.exists(p.archive.c_str());
  uint64_t physical=0;Record record;
  if(!complete&&fs.exists(p.partial.c_str())) {
    auto file=fs.open(p.partial.c_str(),FILE_READ);CloseFile close{file};
    if(!file)return Error::storage;
    physical=file.size();
    if(physical!=expected.size_bytes&&physical&&!read_record(fs,p,expected,physical,record))return Error::resume;
    complete=physical==expected.size_bytes;ready=p.partial;
  }
  if(!complete) {
    if(record.offset>=expected.size_bytes)return Error::resume;
    if(free<expected.size_bytes-record.offset+pack.manifest_json.size()+kSpaceReserve)return Error::space;
    const download::Request request{expected.size_bytes,record.offset,expected.size_bytes-record.offset,
                                    record.offset?std::string_view(record.etag):std::string_view{}};
    Headers header;CloseStream close_stream{stream};
    if(!stream.open(pack.urls[mirror],request,header))return Error::network;
    const download::Response response{header.status,header.has_length,header.length,header.range,header.etag,header.encoding};
    if(download::validate_response(request,response)!=download::Error::none||
        !download::detail::strong_etag(header.etag))return Error::response;
    auto file=fs.open(p.partial.c_str(),FILE_APPEND,true);CloseFile close_file{file};
    if(!file||(physical!=record.offset&&!file.truncate(static_cast<size_t>(record.offset))))return Error::storage;
    if(physical==0&&record.slot<0) {
      for(const auto& path:p.record)
        if(fs.exists(path.c_str())&&!fs.remove(path.c_str()))return Error::storage;
    }
    if(!file.sync()||!checkpoint(fs,p,expected,record.offset,header.etag,record.slot))return Error::storage;
    uint64_t used=record.offset,last=used;Error result=Error::none;
    while(used<expected.size_bytes) {
      if(hooks.cancelled&&hooks.cancelled(hooks.context)){result=Error::cancelled;break;}
      const size_t take=static_cast<size_t>(std::min<uint64_t>(capacity,expected.size_bytes-used));
      const int got=stream.read(buffer,take);
      if(got<=0||static_cast<size_t>(got)>take){result=Error::network;break;}
      if(file.write(buffer,static_cast<size_t>(got))!=static_cast<size_t>(got))return Error::storage;
      used+=static_cast<size_t>(got);
      if(hooks.progress)hooks.progress(hooks.context,used,expected.size_bytes);
      if(used-last>=kCheckpoint) {
        if(!file.sync()||!checkpoint(fs,p,expected,used,header.etag,record.slot))return Error::storage;
        last=used;
      }
    }
    stream.close();
    if(!file.sync()||!checkpoint(fs,p,expected,used,header.etag,record.slot)||!file.close())return Error::storage;
    if(result!=Error::none)return result;
    ready=p.partial;
  }
  if(hooks.verifying)hooks.verifying(hooks.context);
  const auto verified=verify_file(fs,ready,expected,buffer,capacity,hooks);
  if(verified!=Error::none)return verified;
  if(ready==p.partial) {
    if(fs.exists(p.archive.c_str())||!fs.rename(p.partial.c_str(),p.archive.c_str()))return Error::storage;
  }
  if(fs.exists(p.manifest.c_str())) {
    auto file=fs.open(p.manifest.c_str(),FILE_READ);CloseFile close{file};
    if(!file||file.size()!=pack.manifest_json.size())return Error::storage;
    size_t pos=0;
    while(pos<pack.manifest_json.size()) {
      const auto take=std::min(capacity,pack.manifest_json.size()-pos);
      if(file.read(buffer,take)!=take||std::memcmp(buffer,pack.manifest_json.data()+pos,take))return Error::storage;
      pos+=take;
    }
    return file.close()?Error::none:Error::storage;
  }
  if(free<pack.manifest_json.size()+kSpaceReserve)return Error::space;
  const auto temporary=p.directory+"/manifest.tmp";
  auto manifest=fs.open(temporary.c_str(),FILE_WRITE,true);CloseFile close{manifest};
  if(!manifest||manifest.write(reinterpret_cast<const uint8_t*>(pack.manifest_json.data()),pack.manifest_json.size())!=pack.manifest_json.size()||
      !manifest.sync()||!manifest.close()||fs.exists(p.manifest.c_str())||!fs.rename(temporary.c_str(),p.manifest.c_str()))return Error::storage;
  return Error::none;
}
} // namespace
Paths paths(const catalog::Pack& pack) {
  Paths p;const auto manifest_hash=digest_text(pack.manifest_json.data(),pack.manifest_json.size());
  if(manifest_hash.size()!=64)return p;
  p.directory="/orcmaps/.stage-"+pack.manifest.output_sha256+"-"+manifest_hash;
  p.partial=p.directory+"/archive.part";p.archive=p.directory+"/archive.pmtiles";
  p.manifest=p.directory+"/pack.manifest.json";
  p.record[0]=p.directory+"/resume.0";p.record[1]=p.directory+"/resume.1";return p;
}
Error stage(storage::FileSystem& fs,const catalog::Pack& pack,size_t mirror,Stream& stream,
            uint64_t free,uint8_t* buffer,size_t bytes,const Hooks& hooks) noexcept {
  try{return prepare(fs,pack,mirror,stream,free,buffer,bytes,hooks);}
  catch(const std::bad_alloc&){return Error::allocation;}
}
Error verify_archive(storage::FileSystem& fs,const catalog::Pack& pack,const std::string& path,
                     uint8_t* buffer,size_t bytes,const Hooks& hooks) noexcept {
  try {
    orcmap::PackManifest expected;
    if(!buffer||bytes<4096||bytes>65536||
        orcmap::ParsePackManifestJson(pack.manifest_json,&expected)!=orcmap::PackJsonError::kNone||
        orcmap::ValidatePackManifest(expected)!=orcmap::PackValidationError::kNone||
        expected.output_sha256!=pack.manifest.output_sha256||expected.pack_id!=pack.manifest.pack_id||
        expected.size_bytes>kMaximum)return Error::input;
    return verify_file(fs,path,expected,buffer,bytes,hooks);
  }catch(const std::bad_alloc&){return Error::allocation;}
}
} // namespace orcsdr::maps::transfer
