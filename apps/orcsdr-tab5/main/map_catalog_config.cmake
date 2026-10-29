set(ORCSDR_MAP_CATALOG_BASE_URL "" CACHE STRING "Approved HTTPS directory containing signed OrcMaps catalog assets")
set(ORCSDR_MAP_CATALOG_PUBLIC_KEY_FILE "" CACHE FILEPATH "Approved map catalog P-256 public key PEM (never private material)")
set(ORCSDR_MAP_CATALOG_PUBLIC_KEY "")
if(ORCSDR_MAP_CATALOG_BASE_URL)
    if(NOT ORCSDR_MAP_CATALOG_BASE_URL MATCHES "^https://[A-Za-z0-9.-]+(:443)?(/[A-Za-z0-9._~/-]*)?$" OR
       NOT ORCSDR_MAP_CATALOG_PUBLIC_KEY_FILE OR NOT EXISTS "${ORCSDR_MAP_CATALOG_PUBLIC_KEY_FILE}")
        message(FATAL_ERROR "Map downloads require an approved plain HTTPS base URL and existing public key file")
    endif()
    file(SIZE "${ORCSDR_MAP_CATALOG_PUBLIC_KEY_FILE}" key_size)
    if(key_size GREATER 2048 OR key_size LESS 64)
        message(FATAL_ERROR "Map catalog public key size is invalid")
    endif()
    file(READ "${ORCSDR_MAP_CATALOG_PUBLIC_KEY_FILE}" ORCSDR_MAP_CATALOG_PUBLIC_KEY)
    if(NOT ORCSDR_MAP_CATALOG_PUBLIC_KEY MATCHES "^-----BEGIN PUBLIC KEY-----" OR
       NOT ORCSDR_MAP_CATALOG_PUBLIC_KEY MATCHES "-----END PUBLIC KEY-----" OR
       ORCSDR_MAP_CATALOG_PUBLIC_KEY MATCHES "PRIVATE" OR
       ORCSDR_MAP_CATALOG_PUBLIC_KEY MATCHES "[^A-Za-z0-9+/= \r\n-]")
        message(FATAL_ERROR "Map catalog input must be a public-only PEM")
    endif()
elseif(ORCSDR_MAP_CATALOG_PUBLIC_KEY_FILE)
    message(FATAL_ERROR "Map catalog public key requires its approved HTTPS base URL")
endif()
configure_file("${CMAKE_CURRENT_LIST_DIR}/map_catalog_configuration.hpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/map_catalog_configuration.hpp" @ONLY)
