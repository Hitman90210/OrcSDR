#include "map_timing.hpp"
#include <cassert>
#include <cstdint>
int main() {
  static_assert(2*sizeof(orcsdr::maps::PreviewTiming)<=21*1024,"Bound ten 256-sample PSRAM rings plus metadata");
  orcsdr::maps::UpdateTiming timing;
  assert(timing.summary().count==0);
  timing.present(100,1);timing.present(200,1); // Repainting one frame is not a new update.
  assert(timing.summary().count==0);
  timing.present(133,2);timing.present(166,3);timing.present(266,4);
  auto summary=timing.summary();
  assert(summary.count==3&&summary.p50_us==33&&summary.p95_us==100&&summary.max_us==100);
  timing.reset();
  timing.present(1000,1);timing.present(900,2); // Clock discontinuity starts a new baseline.
  assert(timing.summary().count==0);
  for(uint32_t i=1;i<=300;++i)timing.present(900+i*10,2+i);
  summary=timing.summary();
  assert(summary.count==256&&summary.p50_us==10&&summary.p95_us==10&&summary.max_us==10);

  orcsdr::maps::PreviewTiming preview;
  preview.present(1,100,110,140,5,20);
  preview.present(1,100,150,190,30,35); // A duplicate repaint is not new camera feedback.
  assert(preview.clear().count==1&&preview.clear().max_us==5);
  assert(preview.transfer().count==1&&preview.transfer().max_us==20);
  assert(preview.paint().count==1&&preview.paint().max_us==30);
  assert(preview.feedback().count==1&&preview.feedback().max_us==40);
  assert(preview.intervals().count==0);
  preview.present(2,150,155,173);
  assert(preview.intervals().count==1&&preview.intervals().max_us==33);
  assert(preview.feedback().p50_us==23&&preview.feedback().max_us==40);
  preview.present(3,200,205,230); // Includes time waiting for a preview lock.
  assert(preview.feedback().max_us==40);
  preview.present(4,300,250,240); // Reject a backwards paint clock.
  assert(preview.paint().count==3);
  preview.present(4,300,290,320); // Reject paint beginning before request acceptance.
  assert(preview.feedback().count==3);
  preview.present(4,300,305,340);
  assert(preview.paint().count==4&&preview.intervals().max_us==110);
  for(uint32_t i=5;i<305;++i)preview.present(i,i*100,i*100+5,i*100+25,2,15);
  assert(preview.paint().count==256&&preview.paint().max_us==20);
  assert(preview.feedback().count==256&&preview.feedback().max_us==25);
  assert(preview.intervals().count==256&&preview.intervals().max_us==100);
  assert(preview.clear().count==256&&preview.clear().max_us==2);
  assert(preview.transfer().count==256&&preview.transfer().max_us==15);

  orcsdr::maps::PreviewTiming percentiles;
  for(uint32_t i=1;i<=20;++i)percentiles.present(i,i*100,i*100,i*100+i);
  assert(percentiles.paint().p50_us==10&&percentiles.paint().p95_us==19&&percentiles.paint().max_us==20);
  orcsdr::maps::PreviewTiming wrapped;
  wrapped.present(UINT32_MAX,100,100,110);wrapped.present(0,120,120,130);
  assert(wrapped.paint().count==2&&wrapped.intervals().max_us==20);
}
