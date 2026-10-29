#pragma once
#include <cstdint>

namespace orcsdr::setup {
enum class Step : uint8_t { welcome, network, location, maps, complete };
// Only progress is stored here; an empty record cannot overwrite location.
struct Flow {
  Step step = Step::welcome;
  bool automatic = false, done = false, writable = true;
  static constexpr uint32_t kSignature = 0x4d010000;
  static Flow restore(bool present,uint32_t record,bool legacy) {
    Flow flow;
    if(!present) {flow.automatic=!legacy;return flow;}
    if((record&0xfffffe00)!=kSignature || (record&0xff)>4 ||
        ((record&0x100)!=0&&(record&0xff)!=4)) {
      flow.writable=false;return flow;
    }
    flow.step=static_cast<Step>(record&0xff);
    flow.done=(record&0x100)!=0;
    flow.automatic=!flow.done;
    return flow;
  }
  uint32_t record() const {return kSignature|(done ? 0x100u:0u)|static_cast<uint8_t>(step);}
  void next() {if(step<Step::complete)step=static_cast<Step>(static_cast<uint8_t>(step)+1);}
  void back() {if(step>Step::welcome)step=static_cast<Step>(static_cast<uint8_t>(step)-1);}
  void quick_start() {step=Step::complete;}
};

inline bool parse_e7(const char* value,bool latitude,int32_t* out) {
  if(!value||!out)return false;
  const bool negative=*value=='-';if(negative)++value;
  if(!*value)return false;
  const uint32_t limit=latitude?900000000u:1800000000u;
  uint32_t number=0;
  for(;*value;++value) {
    if(*value<'0'||*value>'9')return false;
    const uint32_t digit=static_cast<uint32_t>(*value-'0');
    if(number>(limit-digit)/10)return false;
    number=number*10+digit;
  }
  *out=negative?-static_cast<int32_t>(number):static_cast<int32_t>(number);return true;
}
} // namespace orcsdr::setup
