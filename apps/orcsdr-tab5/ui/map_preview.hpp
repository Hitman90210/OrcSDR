#pragma once
#include <string_view>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "orcmap/frame_transform.hpp"
namespace orcsdr::maps {
// Call while holding the snapshot lock. Lifetime changes on cancel/rescan,
// not on camera movement, so an old camera can follow a new one in one session.
inline bool prepare_preview(const orcmap::Viewport& source,const orcmap::Viewport& destination,
    std::string_view source_pack,std::string_view destination_pack,uint32_t source_lifetime,
    uint32_t current_lifetime,bool valid,orcmap::FrameTransform* output,bool source_labels=true,bool destination_labels=true) {
  if(!output||!valid||source_labels!=destination_labels||source_lifetime!=current_lifetime||source_pack!=destination_pack||
      source.width_px!=destination.width_px||source.height_px!=destination.height_px)return false;
  orcmap::FrameTransform result;
  if(!orcmap::MakeFrameTransform(source,destination,&result)||result.scale<0.25||result.scale>4)return false;
  *output=result;return true;
}
// Graphics adapter seam, shared by the native service and the recording test.
struct PreviewPaint {bool painted=false;uint64_t clear_us=0,transfer_us=0;};
using PreviewClock=uint64_t (*)();
template<class Image,class Target>
PreviewPaint paint_preview(Image& image,Target& target,const orcmap::FrameTransform& transform,
                   int x,int y,int width,int height,PreviewClock clock=nullptr) {
  int32_t cx,cy,cw,ch;target.getClipRect(&cx,&cy,&cw,&ch);
  const int64_t left=std::max<int64_t>(x,cx),top=std::max<int64_t>(y,cy);
  const int64_t right=std::min<int64_t>(int64_t{x}+width,int64_t{cx}+cw);
  const int64_t bottom=std::min<int64_t>(int64_t{y}+height,int64_t{cy}+ch);
  if(width<=0||height<=0||cw<=0||ch<=0||right<=left||bottom<=top)return {};
  target.setClipRect(left,top,right-left,bottom-top);
  const auto started=clock?clock():0;
  const auto clear=[&](int64_t l,int64_t t,int64_t r,int64_t b) {
    if(r>l&&b>t)target.fillRect(l,t,r-l,b-t,0x0841);
  };
  const auto dx=transform.scale==1?std::lround(transform.dx):0;
  const auto dy=transform.scale==1?std::lround(transform.dy):0;
  int64_t il=int64_t{x}+dx,it=int64_t{y}+dy;
  int64_t ir=il+width,ib=it+height;
  if(transform.scale!=1) {
    // M5GFX scales about pixel centres. Keep a one-pixel safety margin around
    // the guaranteed opaque interior for float/fixed-point raster rounding.
    const double scale=static_cast<float>(transform.scale);
    const double sx=static_cast<float>(x+transform.dx)+(1-scale)*0.5;
    const double sy=static_cast<float>(y+transform.dy)+(1-scale)*0.5;
    il=static_cast<int64_t>(std::ceil(sx+1));it=static_cast<int64_t>(std::ceil(sy+1));
    ir=static_cast<int64_t>(std::floor(sx+width*scale-1));
    ib=static_cast<int64_t>(std::floor(sy+height*scale-1));
  }
  // The opaque snapshot overwrites the covered intersection. Clear only
  // exposed strips; top/bottom and the two middle sides never overlap.
  il=std::max(left,il);it=std::max(top,it);
  ir=std::min(right,ir);ib=std::min(bottom,ib);
  if(ir<=il||ib<=it)clear(left,top,right,bottom);
  else {
    clear(left,top,right,it);clear(left,ib,right,bottom);
    clear(left,it,il,ib);clear(ir,it,right,ib);
  }
  const auto cleared=clock?clock():0;
  if(transform.scale==1)image.pushSprite(&target,x+dx,y+dy);
  else {
    image.setPivot(0,0);
    image.pushRotateZoom(&target,x+transform.dx,y+transform.dy,0,transform.scale,transform.scale);
  }
  const auto transferred=clock?clock():0;
  target.setClipRect(cx,cy,cw,ch);
  return {true,cleared>=started?cleared-started:0,transferred>=cleared?transferred-cleared:0};
}
}
