#include "map_preferences.hpp"
#include "nvs_store.hpp"
#include <map>
#include <string>
#include <cstdio>
namespace {std::map<std::string,bool> values;bool fail_open=false,fail_write=false,wrong_namespace=false;}
namespace orcsdr {
NvsStore::~NvsStore()=default;
bool NvsStore::begin(const char* name,bool){wrong_namespace|=std::string(name)!="mapview";return !fail_open;}
bool NvsStore::get_bool(const char* key,bool fallback)const{const auto it=values.find(key);return it==values.end()?fallback:it->second;}
bool NvsStore::put_bool(const char* key,bool value){if(fail_write)return false;values[key]=value;return true;}
}
namespace p=orcsdr::maps::preferences;
int main(){
 int failures=0;auto check=[&](bool ok,const char* name){if(!ok){++failures;std::printf("FAIL %s\n",name);}};
 p::load();check(p::labels(p::Consumer::picker)&&p::labels(p::Consumer::lora)&&p::labels(p::Consumer::adsb)&&p::labels(p::Consumer::ft8),"fresh defaults retain automatic labels");
 check(p::set_labels(p::Consumer::lora,false)&&!p::labels(p::Consumer::lora)&&p::labels(p::Consumer::adsb),"one dashboard change does not affect others");
 p::load();check(!p::labels(p::Consumer::lora)&&p::labels(p::Consumer::picker),"preferences reload independently");
 fail_write=true;check(!p::set_labels(p::Consumer::adsb,false)&&p::labels(p::Consumer::adsb),"failed save keeps active state");fail_write=false;
 fail_open=true;check(!p::set_labels(p::Consumer::picker,false)&&p::labels(p::Consumer::picker),"failed open keeps active state");fail_open=false;
 check(p::set_labels(p::Consumer::picker,false)&&p::set_labels(p::Consumer::ft8,false),"picker and FT8 have distinct saved choices");
 p::load();check(!p::labels(p::Consumer::picker)&&!p::labels(p::Consumer::ft8)&&p::labels(p::Consumer::adsb),"reboot keeps each choice");
 check(!p::set_labels(static_cast<p::Consumer>(99),false),"invalid consumer rejected");
 check(!wrong_namespace,"only dedicated map preference namespace used");
 std::printf("RESULT: map preferences %s\n",failures?"FAIL":"PASS");return failures?1:0;
}
