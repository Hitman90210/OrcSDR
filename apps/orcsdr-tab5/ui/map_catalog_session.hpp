#pragma once
#include "map_catalog.hpp"
#include "map_delivery_state.hpp"
namespace orcsdr::maps::delivery {
struct CatalogAcceptance {
  Error storage=Error::none;
  catalog::Error catalog=catalog::Error::none;
  bool accepted()const{return storage==Error::none&&catalog==catalog::Error::none;}
};
// Worker only. No accepted catalog is exposed until its revision/hash is
// durably recorded. Repeated identical catalogs do not rewrite NVS.
CatalogAcceptance accept_catalog(Store&,std::string_view payload,std::string_view signature,
    std::string_view public_key,uint64_t utc_now,bool clock_trusted,catalog::Catalog*) noexcept;
}
