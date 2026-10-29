#include "map_preview.hpp"
#include <array>
#include <cassert>

struct Target {
  static constexpr int width=40,height=30;
  int32_t x=0,y=0,w=width,h=height;
  int fills=0;
  std::array<unsigned,width*height> cleared{},covered{};
  void getClipRect(int32_t* a,int32_t* b,int32_t* c,int32_t* d){*a=x;*b=y;*c=w;*d=h;}
  void setClipRect(int32_t a,int32_t b,int32_t c,int32_t d){x=a;y=b;w=c;h=d;}
  void mark(int l,int t,int rw,int rh,std::array<unsigned,width*height>& pixels) {
    for(int j=std::max(0,std::max(y,t));j<std::min(height,std::min(y+h,t+rh));++j)
      for(int i=std::max(0,std::max(x,l));i<std::min(width,std::min(x+w,l+rw));++i)++pixels[j*width+i];
  }
  void fillRect(int l,int t,int rw,int rh,int color){assert(color==0x0841);++fills;mark(l,t,rw,rh,cleared);}
};
struct Image {
  int w=10,h=8,pushes=0,scales=0;
  void pushSprite(Target* target,int x,int y){++pushes;target->mark(x,y,w,h,target->covered);}
  void setPivot(int x,int y){assert(x==0&&y==0);}
  void pushRotateZoom(Target* target,double dx,double dy,int angle,double sx,double sy){
    ++scales;assert(angle==0);
    // Independent inverse nearest-neighbour sampling about pixel centres.
    for(int y=0;y<Target::height;++y)for(int x=0;x<Target::width;++x) {
      const double source_x=(x-dx)/sx+0.5,source_y=(y-dy)/sy+0.5;
      if(source_x>=0&&source_x<w&&source_y>=0&&source_y<h)
        target->mark(x,y,1,1,target->covered);
    }
  }
};
uint64_t tick=0;
uint64_t clock_tick(){tick+=7;return tick;}

void pan_case(double dx,double dy,int cx,int cy,int cw,int ch) {
  using namespace orcsdr::maps;
  Target target;target.setClipRect(cx,cy,cw,ch);Image image;
  const auto result=paint_preview(image,target,{1,dx,dy},3,4,image.w,image.h);
  assert(target.x==cx&&target.y==cy&&target.w==cw&&target.h==ch);
  bool any=false;
  for(int y=0;y<Target::height;++y)for(int x=0;x<Target::width;++x) {
    const bool visible=x>=3&&x<13&&y>=4&&y<12&&x>=cx&&x<cx+cw&&y>=cy&&y<cy+ch;
    const bool covered=visible&&x>=3+std::lround(dx)&&x<13+std::lround(dx)&&
        y>=4+std::lround(dy)&&y<12+std::lround(dy);
    const auto index=y*Target::width+x;
    assert(target.cleared[index]==unsigned(visible&&!covered));
    assert(target.covered[index]==unsigned(covered));
    any|=visible;
  }
  assert(result.painted==any&&result.clear_us==0&&result.transfer_us==0);
  assert(image.pushes==int(any)&&image.scales==0);
  if(dx==0&&dy==0)assert(target.fills==0);
}
void scaled_case(double scale,double dx,double dy,int cx,int cy,int cw,int ch) {
  using namespace orcsdr::maps;
  Target target;target.setClipRect(cx,cy,cw,ch);Image image;
  paint_preview(image,target,{scale,dx,dy},3,4,image.w,image.h);
  assert(target.x==cx&&target.y==cy&&target.w==cw&&target.h==ch);
  bool retained_interior=false;
  for(int y=0;y<Target::height;++y)for(int x=0;x<Target::width;++x) {
    const bool visible=x>=3&&x<13&&y>=4&&y<12&&x>=cx&&x<cx+cw&&y>=cy&&y<cy+ch;
    const auto index=y*Target::width+x;
    // Every old pixel is either overwritten by the image or explicitly cleared.
    assert(!visible||target.cleared[index]||target.covered[index]);
    assert(visible||(!target.cleared[index]&&!target.covered[index]));
    assert(target.cleared[index]<=1);
    retained_interior|=visible&&target.covered[index]&&!target.cleared[index];
  }
  if(scale>=2&&dx==0&&dy==0&&cx==0)assert(retained_interior);
}
int main() {
  using namespace orcsdr::maps;
  orcmap::Viewport a;a.zoom=8;a.width_px=1248;a.height_px=568;
  auto b=a;assert(orcmap::PanByPixels(&b,25,10));
  orcmap::FrameTransform result;
  assert(prepare_preview(a,b,"regional","regional",7,7,true,&result));
  assert(!prepare_preview(a,b,"regional","regional",7,8,true,&result));
  assert(!prepare_preview(a,b,"regional","other",7,7,true,&result));
  assert(!prepare_preview(a,b,"regional","regional",7,7,false,&result));
  assert(!prepare_preview(a,b,"regional","regional",7,7,true,&result,true,false));
  assert(!prepare_preview(a,b,"regional","regional",7,7,true,&result,false,true));
  assert(prepare_preview(a,b,"regional","regional",7,7,true,&result,false,false));
  b.width_px=900;assert(!prepare_preview(a,b,"","",7,7,true,&result));
  b=a;b.zoom=11;assert(!prepare_preview(a,b,"","",7,7,true,&result));
  b.zoom=10;assert(prepare_preview(a,b,"","",7,7,true,&result)&&result.scale==4);
  assert(!prepare_preview(a,b,"","",7,7,true,nullptr));

  for(double dx:{-12.,-3.,-0.5,0.,0.5,3.,12.})for(double dy:{-10.,-2.,0.,2.,10.}) {
    pan_case(dx,dy,0,0,40,30); // Every pan direction, identity and fully displaced image.
    pan_case(dx,dy,5,6,5,4); // Clip strictly inside map.
    pan_case(dx,dy,0,0,7,7); // Partial overlap with map.
    pan_case(dx,dy,20,20,5,5); // Fully clipped out.
  }
  pan_case(1,1,3,4,0,8);pan_case(1,1,3,4,10,0);
  for(double scale:{0.25,0.5,1.25,2.,4.})
    for(double dx:{-20.,-3.75,0.,0.25,4.5,20.})
      for(double dy:{-12.,-1.25,0.,0.75,12.}) {
        scaled_case(scale,dx,dy,0,0,40,30);
        scaled_case(scale,dx,dy,5,6,5,4);
        scaled_case(scale,dx,dy,20,20,5,5);
      }
  Target target;Image image;tick=0;
  const auto scaled=paint_preview(image,target,{2,0,0},3,4,10,8,clock_tick);
  assert(scaled.painted&&scaled.clear_us==7&&scaled.transfer_us==7&&tick==21);
  assert(image.scales==1&&image.pushes==0);
  // Zoom-in covers this viewport except for the conservative raster edge margin.
  for(int y=0;y<Target::height;++y)for(int x=0;x<Target::width;++x)
    assert(target.cleared[y*Target::width+x]==unsigned((x==3&&y>=4&&y<12)||(y==4&&x>=3&&x<13)));
  assert(target.x==0&&target.y==0&&target.w==40&&target.h==30);
  const auto before=tick;
  const auto clipped=paint_preview(image,target,{1,0,0},100,100,10,8,clock_tick);
  assert(!clipped.painted&&clipped.clear_us==0&&clipped.transfer_us==0&&tick==before);
  const auto pan=paint_preview(image,target,{1,2,-2},3,4,10,8,clock_tick);
  assert(pan.painted&&pan.clear_us==7&&pan.transfer_us==7);
  Target clipped_scaled;clipped_scaled.setClipRect(5,6,3,2);Image scaled_image;
  assert(paint_preview(scaled_image,clipped_scaled,{0.5,-2,3},3,4,10,8).painted);
  assert(scaled_image.scales==1&&clipped_scaled.fills==1);
  for(int y=0;y<Target::height;++y)for(int x=0;x<Target::width;++x)
    assert(clipped_scaled.cleared[y*Target::width+x]==unsigned(x>=5&&x<8&&y>=6&&y<8));
  assert(clipped_scaled.x==5&&clipped_scaled.y==6&&clipped_scaled.w==3&&clipped_scaled.h==2);
  assert(!paint_preview(image,target,{1,0,0},3,4,0,8,clock_tick).painted);
  assert(!paint_preview(image,target,{1,0,0},3,4,10,-1,clock_tick).painted);
}
