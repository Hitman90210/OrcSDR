#include "map_read_cache.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include <new>
#include <vector>
using orcsdr::maps::ReadCache;
struct Source final:orcmap::ByteSource {
  std::vector<uint8_t> data=std::vector<uint8_t>(3*ReadCache::kCapacity+37);
  size_t reads=0;bool short_read=false,valid=true;
  Source(){for(size_t i=0;i<data.size();++i)data[i]=static_cast<uint8_t>((i*17+i/4096)%251);}
  uint64_t Size() const override{return data.size();}
  bool Valid() const override{return valid;}
  size_t Read(uint64_t offset,void* out,size_t count) override{
    ++reads;if(offset>=data.size())return 0;
    size_t n=std::min(count,data.size()-static_cast<size_t>(offset));
    if(short_read)n=std::min(n,size_t{23});
    std::memcpy(out,data.data()+offset,n);return n;
  }
};
int main(){
  Source source;std::vector<uint8_t> buffer(ReadCache::kCapacity),out(13001);
  // The P4 service explicitly constructs its polymorphic cache in zeroed PSRAM.
  alignas(ReadCache) uint8_t zeroed[sizeof(ReadCache)]{};
  auto* placed=new(zeroed) ReadCache();
  orcmap::ByteSource* polymorphic=placed;
  assert(!polymorphic->Valid());
  placed->Reset(&source,buffer.data(),buffer.size());
  assert(polymorphic->Valid() && polymorphic->Read(0,out.data(),111)==111);
  assert(std::equal(out.begin(),out.begin()+111,source.data.begin()));
  placed->~ReadCache();

  ReadCache cache;
  assert(cache.Read(0,out.data(),1)==0);
  cache.Reset(&source,buffer.data(),buffer.size());
  assert(cache.Capacity()==ReadCache::kCapacity);
  for(size_t offset:{size_t{1},size_t{4090},size_t{65530},size_t{262140}}){
    assert(cache.Read(offset,out.data(),out.size())==out.size());
    assert(std::equal(out.begin(),out.end(),source.data.begin()+offset));
  }
  cache.Reset(&source,buffer.data(),buffer.size());
  assert(cache.Read(33,out.data(),200)==200);const size_t reads=source.reads;
  assert(cache.Read(33,out.data(),200)==200 && source.reads==reads);
  assert(cache.stats().hits==1 && cache.stats().misses==1);
  cache.ResetStats();assert(cache.stats().backing_reads==0);
  assert(cache.Read(33,out.data(),200)==200 && cache.stats().hits==1);
  // Direct-mapped collisions must evict, never return the old block.
  for(size_t offset:{size_t{0},ReadCache::kCapacity,size_t{0}}){
    assert(cache.Read(offset,out.data(),200)==200);
    assert(std::equal(out.begin(),out.begin()+200,source.data.begin()+offset));
  }
  const size_t tail=source.data.size()-11;
  assert(cache.Read(tail,out.data(),200)==11);
  assert(std::equal(out.begin(),out.begin()+11,source.data.begin()+tail));
  assert(cache.Read(std::numeric_limits<uint64_t>::max(),out.data(),200)==0);
  assert(cache.Read(0,nullptr,1)==0 && cache.Read(0,out.data(),0)==0);
  cache.Reset(&source,buffer.data(),buffer.size());source.short_read=true;
  assert(cache.Read(0,out.data(),100)==23);
  source.short_read=false;assert(cache.Read(0,out.data(),100)==100);
  assert(cache.stats().misses==2);
  cache.Reset(&source,buffer.data(),buffer.size());source.short_read=true;
  assert(cache.Read(100,out.data(),100)==0);
  source.short_read=false;assert(cache.Read(100,out.data(),100)==100);
  // Attachment changes invalidate every block, even with the same buffer.
  Source other;other.data[0]=42;
  cache.Reset(&other,buffer.data(),buffer.size());
  assert(cache.Read(0,out.data(),1)==1 && out[0]==42);
  for(size_t bytes:{size_t{0},size_t{4095}}){
    cache.Reset(&source,buffer.data(),bytes);assert(cache.Capacity()==0);
    assert(cache.Read(7,out.data(),111)==111);
    assert(std::equal(out.begin(),out.begin()+111,source.data.begin()+7));
  }
  cache.Reset(&source,nullptr,ReadCache::kCapacity);
  assert(cache.Capacity()==0 && cache.Read(0,out.data(),1)==1);
  source.valid=false;assert(!cache.Valid() && cache.Read(0,out.data(),1)==0);
  puts("MAP_READ_CACHE_PASS");
}
