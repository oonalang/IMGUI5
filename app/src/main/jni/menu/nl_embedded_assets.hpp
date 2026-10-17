#pragma once
#include <cstddef>

// Same assets as app/src/main/assets, compiled into the library. Use this when the
// code runs inside another process and cannot open APK assets:
//   nl_ui::SetAssetReader(nl_embedded_asset_reader);
void* nl_embedded_asset_reader(const char* name, std::size_t* out_size);
