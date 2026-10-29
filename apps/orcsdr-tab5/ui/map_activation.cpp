#include "map_activation.hpp"
#include "orcmap/pack_json.hpp"
#include <mbedtls/sha256.h>
#include <array>
#include <cstring>
#include <new>

namespace orcsdr::maps::activation {
namespace {
constexpr std::string_view kPrefix="/orcmaps/.stage-",kRoot="/orcmaps/";
constexpr size_t kBody=329,kRecord=361;
struct Close {storage::File& file;~Close(){file.close();}};
std::string hash(std::string_view bytes) {
  uint8_t out[32]{};
  if(mbedtls_sha256(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size(),out,0))return {};
  constexpr char hex[]="0123456789abcdef";std::string text;text.reserve(64);
  for(uint8_t c:out){text+=hex[c>>4];text+=hex[c&15];}return text;
}
bool hex(std::string_view text) {
  if(text.size()!=64)return false;
  for(char c:text)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
  return true;
}
bool read(storage::FileSystem& fs,const std::string& path,size_t maximum,std::string& out) {
  auto file=fs.open(path.c_str(),FILE_READ);Close close{file};
  if(!file||file.size()>maximum)return false;
  std::string next(file.size(),'\0');
  if(file.read(reinterpret_cast<uint8_t*>(next.data()),next.size())!=next.size()||!file.close())return false;
  out=std::move(next);return true;
}
bool matches(storage::FileSystem& fs,const std::string& path,const std::string& expected) {
  std::string actual;return read(fs,path,expected.size(),actual)&&actual==expected;
}
bool write(storage::FileSystem& fs,const std::string& path,std::string_view bytes) {
  auto file=fs.open(path.c_str(),FILE_WRITE,true);Close close{file};
  return file&&file.write(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size())==bytes.size()&&
      file.sync()&&file.close();
}
bool manifest_name(std::string_view name) {
  if(name.size()<=14||name.size()>255||name.substr(name.size()-14)!=".manifest.json")return false;
  for(unsigned char c:name)if(c<=31||c==127||c=='/'||c=='\\'||c==':')return false;
  return true;
}
struct Job {
  std::string directory,archive,manifest,staged_archive,staged_manifest,intent,pending,done;
  std::string old_name,old_hash,backup,tag,raw;
  orcmap::PackManifest expected;
};
Job job(const std::string& directory) {
  Job j;j.directory=directory;
  const auto stem=std::string(kRoot)+"map-"+directory.substr(kPrefix.size());
  j.archive=stem+".pmtiles";j.manifest=stem+".manifest.json";
  j.staged_archive=directory+"/archive.pmtiles";j.staged_manifest=directory+"/pack.manifest.json";
  j.intent=directory+"/activation.manifest";j.pending=directory+"/activation.pending";
  j.done=directory+"/activation.done";j.backup=directory+"/previous.bak";return j;
}
std::string record(const std::string& name,const std::string& old_hash) {
  std::string bytes(kRecord,'\0');std::memcpy(bytes.data(),"ORCACT01",8);
  bytes[8]=static_cast<char>(name.size());bytes[9]=static_cast<char>(name.size()>>8);
  std::memcpy(bytes.data()+10,name.data(),name.size());
  std::memcpy(bytes.data()+265,old_hash.data(),64);
  uint8_t digest[32]{};
  if(mbedtls_sha256(reinterpret_cast<const uint8_t*>(bytes.data()),kBody,digest,0))return {};
  std::memcpy(bytes.data()+kBody,digest,32);return bytes;
}
bool load(storage::FileSystem& fs,Job& j) {
  std::string bytes;
  if(!read(fs,j.pending,kRecord,bytes)||bytes.size()!=kRecord||std::memcmp(bytes.data(),"ORCACT01",8))return false;
  uint8_t digest[32]{};
  if(mbedtls_sha256(reinterpret_cast<const uint8_t*>(bytes.data()),kBody,digest,0)||
      std::memcmp(digest,bytes.data()+kBody,32))return false;
  const auto length=static_cast<unsigned char>(bytes[8])+(static_cast<size_t>(static_cast<unsigned char>(bytes[9]))<<8);
  if(length>255)return false;
  j.old_name=bytes.substr(10,length);j.old_hash=bytes.substr(265,64);
  if((length&&!manifest_name(j.old_name))||!hex(j.old_hash))return false;
  j.tag=hash(bytes)+"\n";
  if(!read(fs,j.intent,orcmap::kMaxPackManifestBytes,j.raw)||
      hash(j.raw)!=j.directory.substr(kPrefix.size()+65)||
      orcmap::ParsePackManifestJson(j.raw,&j.expected)!=orcmap::PackJsonError::kNone||
      orcmap::ValidatePackManifest(j.expected)!=orcmap::PackValidationError::kNone||
      j.expected.output_sha256!=j.directory.substr(kPrefix.size(),64))return false;
  return true;
}
bool old_matches(storage::FileSystem& fs,const std::string& path,const Job& j) {
  std::string bytes;return read(fs,path,orcmap::kMaxPackManifestBytes,bytes)&&hash(bytes)==j.old_hash;
}
Error retire(storage::FileSystem& fs,const Job& j,bool committed=false) {
  if(j.old_name.empty())return Error::none;
  const auto path=std::string(kRoot)+j.old_name;
  if(!fs.exists(path.c_str()))return committed||old_matches(fs,j.backup,j)?Error::none:Error::conflict;
  if(!old_matches(fs,path,j))return Error::conflict;
  if(fs.exists(j.backup.c_str())) {
    if(!old_matches(fs,j.backup,j))return Error::conflict;
    // An identical durable backup already exists. Remove only its duplicate
    // visible metadata; the previous archive is never removed or replaced.
    return fs.remove(path.c_str())?Error::none:Error::storage;
  }
  return fs.rename(path.c_str(),j.backup.c_str())?Error::none:Error::storage;
}
Error rollback(storage::FileSystem& fs,const Job& j) {
  if(fs.exists(j.manifest.c_str())) {
    if(!matches(fs,j.manifest,j.raw))return Error::conflict;
    if(!fs.remove(j.manifest.c_str()))return Error::storage;
  }
  if(!j.old_name.empty()) {
    const auto path=std::string(kRoot)+j.old_name;
    if(fs.exists(path.c_str())) {
      if(!old_matches(fs,path,j))return Error::conflict;
    }else {
      if(!old_matches(fs,j.backup,j))return Error::corrupt;
      if(!fs.rename(j.backup.c_str(),path.c_str()))return Error::storage;
    }
  }
  if(fs.exists(j.archive.c_str())&&!fs.exists(j.staged_archive.c_str())&&
      !fs.rename(j.archive.c_str(),j.staged_archive.c_str()))return Error::storage;
  if(!fs.exists(j.staged_manifest.c_str())&&!write(fs,j.staged_manifest,j.raw))return Error::storage;
  if(fs.exists(j.done.c_str())&&!fs.remove(j.done.c_str()))return Error::storage;
  return fs.remove(j.pending.c_str())?Error::none:Error::storage;
}
Error recover_job(storage::FileSystem& fs,Job& j) {
  const auto pending=fs.status(j.pending.c_str());
  if(pending==storage::EntryStatus::missing)return Error::none;
  if(pending!=storage::EntryStatus::file)return Error::storage;
  for(const auto* path:{&j.archive,&j.manifest,&j.staged_archive,&j.staged_manifest,&j.done})
    if(fs.status(path->c_str())==storage::EntryStatus::error||
        fs.status(path->c_str())==storage::EntryStatus::directory)return Error::storage;
  if(!load(fs,j)) {
    // A torn initial journal is safe to quarantine only before any mutation
    // output exists. Preserve its bytes instead of guessing its old path.
    if(fs.status(j.archive.c_str())==storage::EntryStatus::missing&&
        fs.status(j.manifest.c_str())==storage::EntryStatus::missing&&
        fs.status((j.directory+"/previous.bak").c_str())==storage::EntryStatus::missing) {
      const auto invalid=j.directory+"/activation.invalid";
      if(fs.status(invalid.c_str())==storage::EntryStatus::missing&&fs.rename(j.pending.c_str(),invalid.c_str()))return Error::none;
    }
    return Error::corrupt;
  }
  if(!j.old_name.empty()) {
    const auto old=std::string(kRoot)+j.old_name;
    if(fs.status(old.c_str())==storage::EntryStatus::error||
        fs.status(j.backup.c_str())==storage::EntryStatus::error)return Error::storage;
  }
  if(fs.status(j.done.c_str())==storage::EntryStatus::file) {
    if(!matches(fs,j.done,j.tag))return Error::corrupt;
    if(!matches(fs,j.manifest,j.raw))return Error::corrupt;
    auto archive=fs.open(j.archive.c_str(),FILE_READ);Close close{archive};
    if(archive&&archive.size()==j.expected.size_bytes&&archive.close())return retire(fs,j,true);
    return Error::corrupt;
  }
  return rollback(fs,j);
}
Error prepare_reuse(storage::FileSystem& fs,const Job& j) {
  // Only reached with no pending journal. A previously finalized artifact
  // may be selected again after another version; its old marker is not a
  // commit for the new transaction. Keep older backup bytes before reuse.
  const auto done=fs.status(j.done.c_str());
  if(done==storage::EntryStatus::file) {
    std::string tag;
    if(!read(fs,j.done,65,tag)||tag.size()!=65||tag.back()!='\n'||!hex(std::string_view(tag).substr(0,64)))
      return Error::corrupt;
  }else if(done!=storage::EntryStatus::missing)return Error::storage;
  const auto backup=fs.status(j.backup.c_str());
  if(backup==storage::EntryStatus::file) {
    std::string raw;
    if(!read(fs,j.backup,orcmap::kMaxPackManifestBytes,raw))return Error::storage;
    const auto digest=hash(raw);if(!hex(digest))return Error::corrupt;
    constexpr char history[]="/orcmaps/.recovery";
    const auto root=fs.status(history);
    if(root==storage::EntryStatus::missing) {if(!fs.mkdir(history))return Error::storage;}
    else if(root!=storage::EntryStatus::directory)return Error::storage;
    const auto path=std::string(history)+"/"+digest+".bak";
    const auto destination=fs.status(path.c_str());
    if(destination==storage::EntryStatus::missing) {
      if(!fs.rename(j.backup.c_str(),path.c_str()))return Error::storage;
    }else if(destination==storage::EntryStatus::file) {
      if(!matches(fs,path,raw))return Error::conflict;
      if(!fs.remove(j.backup.c_str()))return Error::storage;
    }else return Error::storage;
  }else if(backup!=storage::EntryStatus::missing)return Error::storage;
  return done==storage::EntryStatus::missing||fs.remove(j.done.c_str())?Error::none:Error::storage;
}
Error apply(storage::FileSystem& fs,const catalog::Pack& pack,const std::string& previous,
            uint8_t* buffer,size_t size,const transfer::Hooks& hooks) {
  const auto p=transfer::paths(pack);
  if(!valid_directory(p.directory))return Error::input;
  auto j=job(p.directory);
  if(!previous.empty()&&(previous.substr(0,kRoot.size())!=kRoot||
      !manifest_name(previous.substr(kRoot.size()))))return Error::input;
  for(const auto* path:{&j.archive,&j.manifest,&j.staged_archive,&j.staged_manifest,&j.intent,&j.pending,&j.done,&j.backup}) {
    const auto status=fs.status(path->c_str());
    if(status==storage::EntryStatus::error||status==storage::EntryStatus::directory)return Error::storage;
  }
  if(fs.exists(j.pending.c_str())) {
    const auto recovered=recover_job(fs,j);
    if(recovered!=Error::none)return recovered;
    if(fs.exists(j.pending.c_str())) {
      const auto verified=transfer::verify_archive(fs,pack,j.archive,buffer,size,hooks);
      return verified==transfer::Error::none?Error::none:Error::corrupt;
    }
  }
  if(fs.exists(j.manifest.c_str())) {
    if(previous!=j.manifest||!matches(fs,j.manifest,pack.manifest_json))return Error::conflict;
    // Repair only a missing owned archive; never replace an existing file here.
    if(fs.status(j.archive.c_str())==storage::EntryStatus::missing) {
      const auto verified=transfer::verify_archive(fs,pack,p.archive,buffer,size,hooks);
      if(verified==transfer::Error::cancelled)return Error::cancelled;
      if(verified==transfer::Error::allocation)return Error::allocation;
      if(verified!=transfer::Error::none)return Error::corrupt;
      if(!fs.rename(p.archive.c_str(),j.archive.c_str()))return Error::storage;
      return Error::none;
    }
    return transfer::verify_archive(fs,pack,j.archive,buffer,size,hooks)==transfer::Error::none?Error::none:Error::corrupt;
  }
  if(!matches(fs,p.manifest,pack.manifest_json))return Error::corrupt;
  const auto verified=transfer::verify_archive(fs,pack,p.archive,buffer,size,hooks);
  if(verified==transfer::Error::cancelled)return Error::cancelled;
  if(verified==transfer::Error::allocation)return Error::allocation;
  if(verified!=transfer::Error::none)return Error::corrupt;
  j.old_name=previous.empty()?"":previous.substr(kRoot.size());
  j.old_hash=std::string(64,'0');
  if(!previous.empty()) {
    std::string raw;orcmap::PackManifest old;
    if(!read(fs,previous,orcmap::kMaxPackManifestBytes,raw)||
        orcmap::ParsePackManifestJson(raw,&old)!=orcmap::PackJsonError::kNone||
        orcmap::ValidatePackManifest(old)!=orcmap::PackValidationError::kNone)return Error::conflict;
    const auto archive=previous.substr(0,previous.size()-14)+".pmtiles";
    auto file=fs.open(archive.c_str(),FILE_READ);Close close{file};
    if(!file||file.size()!=old.size_bytes||!file.close())return Error::conflict;
    j.old_hash=hash(raw);
  }
  if(fs.exists(j.intent.c_str())) {
    if(!matches(fs,j.intent,pack.manifest_json))return Error::conflict;
  }else if(!write(fs,j.intent,pack.manifest_json))return Error::storage;
  const auto reused=prepare_reuse(fs,j);if(reused!=Error::none)return reused;
  const auto bytes=record(j.old_name,j.old_hash);if(bytes.size()!=kRecord)return Error::corrupt;
  // Sync the final journal filename before the first visible metadata change.
  if(!write(fs,j.pending,bytes)||!load(fs,j))return Error::storage;
  if(fs.exists(j.archive.c_str())) {
    const auto existing=transfer::verify_archive(fs,pack,j.archive,buffer,size,hooks);
    if(existing!=transfer::Error::none)return Error::conflict;
  }else if(!fs.rename(p.archive.c_str(),j.archive.c_str()))return Error::storage;
  const auto retired=retire(fs,j);if(retired!=Error::none)return retired;
  if(!fs.rename(p.manifest.c_str(),j.manifest.c_str()))return Error::storage;
  const auto done_temporary=j.directory+"/commit.tmp";
  if(!write(fs,done_temporary,j.tag)||fs.exists(j.done.c_str())||
      !fs.rename(done_temporary.c_str(),j.done.c_str()))return Error::storage;
  return Error::none;
}
} // namespace
bool valid_directory(std::string_view path) {
  return path.size()==kPrefix.size()+129&&path.substr(0,kPrefix.size())==kPrefix&&
      path[kPrefix.size()+64]=='-'&&hex(path.substr(kPrefix.size(),64))&&hex(path.substr(kPrefix.size()+65));
}
std::string target_manifest(const catalog::Pack& pack) {
  const auto p=transfer::paths(pack);return valid_directory(p.directory)?job(p.directory).manifest:std::string{};
}
Error recover(storage::FileSystem& fs,const std::string& directory) {
  if(!valid_directory(directory))return Error::input;
  try {auto j=job(directory);return recover_job(fs,j);}
  catch(const std::bad_alloc&){return Error::allocation;}
}
Error activate(storage::FileSystem& fs,const catalog::Pack& pack,const std::string& previous,
               uint8_t* buffer,size_t size,const transfer::Hooks& hooks) {
  try {
    const auto result=apply(fs,pack,previous,buffer,size,hooks);
    if(result!=Error::none) {
      const auto p=transfer::paths(pack);
      if(valid_directory(p.directory)&&fs.exists((p.directory+"/activation.pending").c_str())) {
        const auto recovered=recover(fs,p.directory);
        if(recovered!=Error::none)return recovered;
      }
    }
    return result;
  }catch(const std::bad_alloc&){return Error::allocation;}
}
Error finalize(storage::FileSystem& fs,const std::string& directory) {
  if(!valid_directory(directory))return Error::input;
  try {
    auto j=job(directory);
    const auto status=fs.status(j.pending.c_str());
    if(status==storage::EntryStatus::missing)return Error::none;
    if(status!=storage::EntryStatus::file)return Error::storage;
    if(!load(fs,j)||!matches(fs,j.done,j.tag))return Error::input;
    const auto checked=recover_job(fs,j);
    if(checked!=Error::none)return checked;
    return fs.remove(j.pending.c_str())?Error::none:Error::storage;
  }catch(const std::bad_alloc&){return Error::allocation;}
}
} // namespace orcsdr::maps::activation
