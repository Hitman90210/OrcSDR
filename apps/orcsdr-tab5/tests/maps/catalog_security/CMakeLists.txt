cmake_minimum_required(VERSION 3.20)
project(orcsdr_map_catalog_security LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
foreach(required ORCMAPS_ROOT MBEDTLS_ROOT MBEDCRYPTO_LIBRARY CJSON_ROOT)
  if(NOT ${required})
    message(FATAL_ERROR "Set ${required} to the resolved dependency")
  endif()
endforeach()
set(ui "${CMAKE_CURRENT_SOURCE_DIR}/../../../ui")
add_executable(map_catalog_security_test catalog_security_test.cpp
  "${ui}/map_delivery_state.cpp" "${ui}/map_catalog_session.cpp"
  "${ui}/map_catalog.cpp" "${CJSON_ROOT}/cJSON.c"
  "${ORCMAPS_ROOT}/src/core/pack.cpp" "${ORCMAPS_ROOT}/src/core/pack_json.cpp"
  "${ORCMAPS_ROOT}/src/core/geo.cpp")
target_include_directories(map_catalog_security_test PRIVATE "${ui}" "${CJSON_ROOT}"
  "${MBEDTLS_ROOT}/include" "${ORCMAPS_ROOT}/include")
target_link_libraries(map_catalog_security_test PRIVATE "${MBEDCRYPTO_LIBRARY}")
if(WIN32)
  target_link_libraries(map_catalog_security_test PRIVATE bcrypt)
endif()
add_executable(map_transfer_test transfer_test.cpp "${ui}/map_transfer.cpp" "${ui}/map_activation.cpp"
  "${ui}/map_install.cpp" "${ui}/map_delivery_state.cpp"
  "${ORCMAPS_ROOT}/src/tiles/pmtiles_reader.cpp"
  "${ORCMAPS_ROOT}/src/tiles/compression.cpp"
  "${ORCMAPS_ROOT}/src/tiles/mvt_stream.cpp"
  "${ORCMAPS_ROOT}/src/tiles/mvt_decoder.cpp"
  "${ORCMAPS_ROOT}/third_party/miniz/miniz.c"
  "${ORCMAPS_ROOT}/third_party/miniz/miniz_tinfl.c"
  "${ORCMAPS_ROOT}/src/core/pack.cpp" "${ORCMAPS_ROOT}/src/core/pack_json.cpp"
  "${ORCMAPS_ROOT}/src/core/geo.cpp" "${CJSON_ROOT}/cJSON.c")
target_include_directories(map_transfer_test PRIVATE "${ui}" "${CJSON_ROOT}"
  "${MBEDTLS_ROOT}/include" "${ORCMAPS_ROOT}/include" "${ORCMAPS_ROOT}/third_party/miniz")
target_compile_definitions(map_transfer_test PRIVATE MINIZ_NO_DEFLATE_APIS MINIZ_NO_ZLIB_APIS MINIZ_NO_ARCHIVE_APIS)
target_link_libraries(map_transfer_test PRIVATE "${MBEDCRYPTO_LIBRARY}")
if(WIN32)
  target_link_libraries(map_transfer_test PRIVATE bcrypt)
endif()
if(MSVC)
  target_compile_options(map_catalog_security_test PRIVATE /EHsc)
endif()
enable_testing()
add_executable(map_catalog_fetch_test catalog_fetch_test.cpp "${ui}/map_catalog_fetch.cpp")
target_include_directories(map_catalog_fetch_test PRIVATE "${ui}" "${ORCMAPS_ROOT}/include")
add_test(NAME map_catalog_fetch COMMAND map_catalog_fetch_test)
add_executable(map_delivery_state_test delivery_state_test.cpp "${ui}/map_delivery_state.cpp")
target_include_directories(map_delivery_state_test PRIVATE "${ui}" "${MBEDTLS_ROOT}/include")
target_link_libraries(map_delivery_state_test PRIVATE "${MBEDCRYPTO_LIBRARY}")
if(WIN32)
  target_link_libraries(map_delivery_state_test PRIVATE bcrypt)
endif()
add_test(NAME map_delivery_state COMMAND map_delivery_state_test)
add_executable(map_delivery_nvs_test delivery_nvs_test.cpp "${ui}/map_delivery_state.cpp" "${ui}/map_delivery_nvs.cpp")
target_include_directories(map_delivery_nvs_test PRIVATE "${ui}" "${MBEDTLS_ROOT}/include" "${CMAKE_CURRENT_SOURCE_DIR}/nvs_stub")
target_link_libraries(map_delivery_nvs_test PRIVATE "${MBEDCRYPTO_LIBRARY}")
if(WIN32)
  target_link_libraries(map_delivery_nvs_test PRIVATE bcrypt)
endif()
add_test(NAME map_delivery_nvs COMMAND map_delivery_nvs_test)
get_filename_component(workspace "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
add_test(NAME map_transfer COMMAND map_transfer_test
  "${ORCMAPS_ROOT}/tests/fixtures/tiny-gzip.pmtiles"
  "${ORCMAPS_ROOT}/examples/m5stack-tab5/test-pack/springfield-97477.manifest.json"
  "${workspace}/.local/maps/map-transfer-fixture-20261019")
set_tests_properties(map_transfer PROPERTIES WORKING_DIRECTORY "${workspace}")

add_executable(map_download_actions_test download_actions_test.cpp)
target_include_directories(map_download_actions_test PRIVATE "${ui}")
add_test(NAME map_download_actions COMMAND map_download_actions_test)
