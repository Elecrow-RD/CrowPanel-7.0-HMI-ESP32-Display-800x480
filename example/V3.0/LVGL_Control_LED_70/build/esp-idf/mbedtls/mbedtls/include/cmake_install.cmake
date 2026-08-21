# Install script for directory: J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/CrowPanel_ESP32_7.0")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "C:/Espressif/tools/xtensa-esp-elf/esp-14.2.0_20260121/xtensa-esp-elf/bin/xtensa-esp32s3-elf-objdump.exe")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/aes.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/aria.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/asn1.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/asn1write.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/base64.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/bignum.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/block_cipher.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/build_info.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/camellia.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ccm.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/chacha20.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/chachapoly.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/check_config.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/cipher.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/cmac.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/compat-2.x.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_legacy_crypto.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_legacy_from_psa.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_psa_from_legacy.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_psa_superset_legacy.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_ssl.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_x509.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config_psa.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/constant_time.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ctr_drbg.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/debug.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/des.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/dhm.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecdh.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecdsa.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecjpake.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecp.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/entropy.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/error.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/gcm.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/hkdf.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/hmac_drbg.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/lms.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/mbedtls_config.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/md.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/md5.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/memory_buffer_alloc.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/net_sockets.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/nist_kw.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/oid.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pem.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pk.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pkcs12.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pkcs5.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pkcs7.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/platform.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/platform_time.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/platform_util.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/poly1305.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/private_access.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/psa_util.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ripemd160.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/rsa.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/sha1.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/sha256.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/sha3.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/sha512.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_cache.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_ciphersuites.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_cookie.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_ticket.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/threading.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/timing.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/version.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/x509.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/x509_crl.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/x509_crt.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/mbedtls/x509_csr.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/psa" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/build_info.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_adjust_auto_enabled.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_adjust_config_dependencies.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_adjust_config_key_pair_types.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_adjust_config_synonyms.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_builtin_composites.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_builtin_key_derivation.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_builtin_primitives.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_compat.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_config.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_driver_common.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_composites.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_key_derivation.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_primitives.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_extra.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_legacy.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_platform.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_se_driver.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_sizes.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_struct.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_types.h"
    "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/mbedtls/mbedtls/include/psa/crypto_values.h"
    )
endif()

