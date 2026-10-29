// Host-only feasibility probe, not a production cache or persistent format.
#include "map_pipeline.hpp"
#include "orcmap/experimental/mvt_classify.hpp"
#include "orcmap/style.hpp"
#include "file_byte_source.hpp"
#include "framebuffer_target.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <tuple>

using Bytes=std::vector<uint8_t>;
void put(Bytes& b,uint64_t n){while(n>=128){b.push_back(uint8_t(n)|128);n>>=7;}b.push_back(uint8_t(n));}
void delta(Bytes& b,int64_t n){put(b,n<0?uint64_t(-(n+1))*2+1:uint64_t(n)*2);}
bool get(const Bytes& b,size_t& p,uint64_t& n){n=0;for(unsigned s=0;s<64;s+=7){if(p==b.size())return false;const auto v=b[p++];if(s==63&&(v&254))return false;n|=uint64_t(v&127)<<s;if(!(v&128))return true;}return false;}
bool point(const Bytes& b,size_t& p,int64_t& x){uint64_t n;if(!get(b,p,n))return false;const auto d=(n&1)?-int64_t(n>>1)-1:int64_t(n>>1);if(d<int64_t(INT32_MIN)-x||d>int64_t(INT32_MAX)-x)return false;x+=d;return true;}
bool encode(const orcmap::Feature& f,void* opaque){
  auto& b=*static_cast<Bytes*>(opaque);orcmap::FeatureKind kind;
  if(!orcmap::experimental::TryClassifyFeatureForProfile("protomaps-v4",f,&kind))return true;
  put(b,static_cast<unsigned>(kind));put(b,static_cast<unsigned>(f.geometry.type));put(b,f.extent);put(b,f.geometry.paths.size());
  for(const auto& path:f.geometry.paths){put(b,path.size());int64_t x=0,y=0;for(const auto& q:path){delta(b,int64_t(q.x)-x);delta(b,int64_t(q.y)-y);x=q.x;y=q.y;}}
  return b.size()<=512*1024;
}
bool decode(const Bytes& b,size_t& p,orcmap::Feature& f){
  uint64_t kind,type,extent,paths;
  if(!get(b,p,kind)||kind>=static_cast<unsigned>(orcmap::FeatureKind::kCount)||!get(b,p,type)||type>3||!get(b,p,extent)||!extent||extent>UINT32_MAX||!get(b,p,paths)||paths>b.size()-p)return false;
  f.kind=static_cast<orcmap::FeatureKind>(kind);f.kind_assigned=true;f.extent=uint32_t(extent);f.geometry.type=static_cast<orcmap::GeomType>(type);f.geometry.paths.resize(size_t(paths));
  for(auto& path:f.geometry.paths){uint64_t n;if(!get(b,p,n)||n>(b.size()-p)/2)return false;path.resize(size_t(n));int64_t x=0,y=0;for(auto& q:path){if(!point(b,p,x)||!point(b,p,y))return false;q={int32_t(x),int32_t(y)};}}
  return true;
}
using Key=std::tuple<unsigned,unsigned,unsigned>;
int main(int argc,char**argv){assert(argc==2||argc==5);
  // Truncation and invalid enum/count checks on one complete synthetic feature.
  orcmap::Feature f;f.kind_assigned=true;f.kind=orcmap::FeatureKind::kLand;f.geometry.type=orcmap::GeomType::kPolygon;
  Bytes sample;put(sample,1);put(sample,3);put(sample,4096);put(sample,1);put(sample,3);for(int n:{0,0,10,0,-10,10})delta(sample,n);
  for(size_t cut=0;cut<sample.size();++cut){Bytes bad(sample.begin(),sample.begin()+cut);size_t at=0;assert(!decode(bad,at,f));}
  size_t at=0;assert(decode(sample,at,f)&&at==sample.size()&&f.geometry.paths[0][2].y==10);
  Bytes bad=sample;bad[0]=255;at=0;assert(!decode(bad,at,f));bad=sample;bad[4]=127;at=0;assert(!decode(bad,at,f));
  orcmap::host::FileByteSource source(argv[1]);orcsdr::maps::Pipeline pipeline(source);assert(pipeline.open("protomaps-v4"));
  orcmap::PmTilesReader reader(&source,1);assert(reader.Open());orcmap::MvtStreamScratch scratch;assert(orcmap::ReserveMvtStreamScratch(&scratch,48*1024));
  orcmap::Viewport camera;camera.width_px=842;camera.height_px=330;camera.zoom=9;camera.center_lat_deg=44.0628866;camera.center_lon_deg=-123.0160273;
  if(argc==5){camera.center_lat_deg=std::strtod(argv[2],nullptr);camera.center_lon_deg=std::strtod(argv[3],nullptr);camera.zoom=uint8_t(std::strtoul(argv[4],nullptr,10));}
  std::map<Key,Bytes> cache;size_t total=0,views=0;
  for(auto pan:{std::pair<int,int>{0,0},{40,0},{0,20},{-40,0},{0,-20}}){
    auto moved=camera;assert(orcmap::PanByPixels(&moved,pan.first,pan.second));orcmap::Viewport view;assert(orcsdr::maps::pack_source_view(moved,reader.Header().min_zoom,reader.Header().max_zoom,&view));
    std::vector<orcmap::TilePlacement> placements;assert(orcmap::EnumerateVisibleTilePlacements(view,&placements));
    orcmap::host::FramebufferTarget actual(842,330),expected(842,330);assert(pipeline.render(view,expected));assert(orcmap::ClearMapBackground(view,orcmap::styles::OrcSdrDark(),&actual));
    for(const auto& placement:placements){const auto t=placement.tile;const Key key{t.z,t.x,t.y};auto found=cache.find(key);if(found==cache.end()){
      Bytes bytes;orcmap::MvtStreamOptions options;options.include_layer=&orcmap::experimental::IncludeNoTextBasemapLayer;options.feature_sink=&encode;options.feature_sink_ctx=&bytes;
      assert(reader.StreamTile(t.z,t.x,t.y,options,&scratch));total+=bytes.size();if(total>512*1024){std::printf("REPLAY_CAPACITY_MISS bytes=%zu cap=%u requires_fallback=1\n",total,512*1024);return 3;}found=cache.emplace(key,std::move(bytes)).first;
    }
    orcmap::OrderedRenderScratch ordered;size_t pos=0;while(pos<found->second.size()){assert(decode(found->second,pos,f));assert(orcmap::RenderOrderedFeatureAt(f,placement,view,orcmap::styles::OrcSdrDark(),&actual,&ordered));}
    assert(orcmap::FinishOrderedTile(&ordered,&actual));
    }
    size_t mismatch=0;for(int y=0;y<330;++y)for(int x=0;x<842;++x)if(!(actual.At(x,y)==expected.At(x,y)))++mismatch;
    std::printf("REPLAY_VIEW pan=%d,%d mismatches=%zu cached_tiles=%zu retained_bytes=%zu\n",pan.first,pan.second,mismatch,cache.size(),total);assert(mismatch==0);++views;
  }
  size_t capacity=0;for(const auto& entry:cache)capacity+=entry.second.capacity();
  std::printf("REPLAY_RESULT PASS lat=%.7f lon=%.7f zoom=%u views=%zu label_off_only=1 geometry_payload=%zu payload_cap=%u vector_capacity=%zu\n",camera.center_lat_deg,camera.center_lon_deg,camera.zoom,views,total,512*1024,capacity);
}
