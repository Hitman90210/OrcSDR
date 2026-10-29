#pragma once
#include "map_transfer.hpp"

namespace orcsdr::maps::activation {
enum class Error {none,input,storage,conflict,corrupt,cancelled,allocation};
bool valid_directory(std::string_view);
std::string target_manifest(const catalog::Pack&);
// Serialized worker only. Caller explicitly supplies the previous local
// manifest for this product, or empty for first install. Never infer ownership
// from display names. Quiesce discovery during this metadata transaction.
Error activate(storage::FileSystem&,const catalog::Pack&,const std::string& previous_manifest,
               uint8_t*,size_t,const transfer::Hooks&);
// Before discovery: incomplete work rolls back; committed work is checked.
// A corrupt/conflicting journal requires attention and never guesses paths.
Error recover(storage::FileSystem&,const std::string& staging_directory);
// Only after the caller durably records ownership/selection for the new pack.
// Keep previous metadata/archive; stop monitoring the completed transaction.
Error finalize(storage::FileSystem&,const std::string& staging_directory);
} // namespace orcsdr::maps::activation
