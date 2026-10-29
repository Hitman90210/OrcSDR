#include "map_inventory.hpp"
#include <algorithm>
#include <cstring>
#include <new>

namespace orcsdr::maps {
namespace {
class BoundedFiles final : public orcmap::PackFileSystem {
 public:
  BoundedFiles(orcmap::PackFileSystem& files, bool& exceeded) : files_(files), exceeded_(exceeded) {}
  bool ListManifestNames(const std::string& directory, std::vector<std::string>* names) override {
    if (!files_.ListManifestNames(directory,names)) return false;
    std::sort(names->begin(),names->end());
    if (names->size()>Inventory::kPackLimit) {
      exceeded_=true;
      names->clear(); // fail explicitly rather than silently hiding installed packs
      return false;
    }
    return true;
  }
  bool ReadTextFile(const std::string& path,size_t limit,std::string* text) override {
    return files_.ReadTextFile(path,limit,text);
  }
  bool FileSize(const std::string& path,uint64_t* size) override { return files_.FileSize(path,size); }
 private:
  orcmap::PackFileSystem& files_;
  bool& exceeded_;
};
}
bool Inventory::scan(orcmap::PackFileSystem& files,const std::string& directory,bool sd_mounted) {
  catalog_={};report_={};limit_exceeded_=false;sd_mounted_=sd_mounted;out_of_memory_=false;
  if (!sd_mounted_) return false;
  BoundedFiles bounded(files,limit_exceeded_);
  try {return orcmap::DiscoverPacks(bounded,directory,&catalog_,&report_);}
  catch(const std::bad_alloc&) {
    catalog_={};report_={};out_of_memory_=true;return false;
  }
}
const orcmap::PackManifest* Inventory::find(const char* identity) const {
  if (!identity || !identity[0]) return nullptr;
  for (const auto& pack:catalog_.Packs()) if (pack.pack_id==identity) return &pack;
  return nullptr;
}
const orcmap::PackManifest* Inventory::resolve(const orcmap::GeoBounds& bounds,uint8_t zoom,
                                              const char* preferred) const {
  if(zoom>20)return nullptr;
  if (const auto* pack=find(preferred)) {
    orcmap::PackCatalog selected;
    if (selected.Add(*pack) && orcmap::ResolvePack(selected,bounds,std::min(zoom,pack->max_zoom)))return pack;
  }
  // Prefer current-detail coverage, then the highest available source level.
  // ResolvePack still enforces bounds, minimum zoom, priority and stable ties.
  for(int source=zoom;source>=0;--source)
    if(const auto* pack=orcmap::ResolvePack(catalog_,bounds,static_cast<uint8_t>(source)))return pack;
  return nullptr;
}
} // namespace orcsdr::maps
