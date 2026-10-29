#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace orcsdr::maps {
// Rolling raw intervals between distinct presented frames, including user dwell.
// Not an FPS or touch-latency metric; sample during a controlled gesture run.
class UpdateTiming {
 public:
  struct Summary {size_t count=0;uint64_t p50_us=0,p95_us=0,max_us=0;};
  void reset() {*this=UpdateTiming{};}
  void duration(uint64_t elapsed_us) {
    samples_[next_]=elapsed_us;next_=(next_+1)%samples_.size();
    count_=std::min(count_+1,samples_.size());
  }
  void present(uint64_t now,uint32_t revision) {
    if(have_baseline_&&revision==revision_)return;
    if(have_baseline_&&now>=last_) {
      duration(now-last_);
    }
    have_baseline_=true;last_=now;revision_=revision;
  }
  Summary summary() const {
    if(!count_)return {};
    auto sorted=samples_;
    std::sort(sorted.begin(),sorted.begin()+count_);
    return {count_,sorted[(count_*50+99)/100-1],
            sorted[(count_*95+99)/100-1],sorted[count_-1]};
  }
 private:
  std::array<uint64_t,256> samples_{};
  size_t next_=0,count_=0;
  uint64_t last_=0;
  uint32_t revision_=0;
  bool have_baseline_=false;
};

// First preview of each camera request. Intervals still include user dwell;
// feedback starts at draw's request acceptance, not at physical touch sampling.
class PreviewTiming {
 public:
  void present(uint32_t request,uint64_t requested,uint64_t started,uint64_t finished,
      uint64_t clear_us=0,uint64_t transfer_us=0) {
    if((have_request_&&request==request_)||finished<started||started<requested)return;
    paint_.duration(finished-started);
    clear_.duration(clear_us);transfer_.duration(transfer_us);
    feedback_.duration(finished-requested);
    intervals_.present(finished,request);
    request_=request;have_request_=true;
  }
  UpdateTiming::Summary paint() const {return paint_.summary();}
  UpdateTiming::Summary feedback() const {return feedback_.summary();}
  UpdateTiming::Summary intervals() const {return intervals_.summary();}
  UpdateTiming::Summary clear() const {return clear_.summary();}
  UpdateTiming::Summary transfer() const {return transfer_.summary();}
 private:
  UpdateTiming paint_,feedback_,intervals_,clear_,transfer_;
  uint32_t request_=0;
  bool have_request_=false;
};
}
