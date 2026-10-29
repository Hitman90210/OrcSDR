#pragma once
#include "map_transfer.hpp"
namespace orcsdr::maps::catalog {
enum class FetchError {none,input,network,response,limit,cancelled,allocation};
// HTTPS-only metadata, with an independent byte cap even for chunked replies.
// Caller-owned worker buffer; output unchanged on failure; stream always closed.
FetchError fetch(transfer::Stream&,std::string_view endpoint,size_t maximum,uint8_t*,size_t,
                 const transfer::Hooks&,std::string*) noexcept;
}
