#include "map_catalog_session.hpp"
#include <new>
namespace orcsdr::maps::delivery {
CatalogAcceptance accept_catalog(Store& store,std::string_view payload,std::string_view signature,
    std::string_view public_key,uint64_t utc_now,bool clock_trusted,catalog::Catalog* output) noexcept {
  if(!output)return {Error::input,catalog::Error::none};
  try {
    State state;const auto loaded=load(store,&state);
    if(loaded!=Error::none)return {loaded,catalog::Error::none};
    catalog::Catalog next;
    const catalog::Policy policy{state.accepted_revision,utc_now,state.catalog_sha256,clock_trusted};
    const auto verified=catalog::read_signed(payload,signature,public_key,policy,&next);
    if(verified!=catalog::Error::none)return {Error::none,verified};
    if(next.revision!=state.accepted_revision||next.sha256!=state.catalog_sha256) {
      state.accepted_revision=next.revision;state.catalog_sha256=next.sha256;
      const auto saved=save(store,state);
      if(saved!=Error::none)return {saved,catalog::Error::none};
    }
    *output=std::move(next);return {};
  }catch(const std::bad_alloc&){return {Error::allocation,catalog::Error::none};}
}
}
