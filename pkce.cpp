/************************************************************************
 * Copyright(c) 2026, One Unified. All rights reserved.                 *
 * email: info@oneunified.net                                           *
 *                                                                      *
 * This file is provided as is WITHOUT ANY WARRANTY                     *
 *  without even the implied warranty of                                *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                *
 *                                                                      *
 * This software may not be used nor distributed without proper license *
 * agreement.                                                           *
 *                                                                      *
 * See the file LICENSE.txt for redistribution information.             *
 ************************************************************************/

/*
 * File:    pkce.cpp
 * Author:  raymond@burkholder.net
 * Project: viessmann
 * Created: October 17, 2026 15:36:47
 */

#include <sodium.h>

#include <boost/log/trivial.hpp>

#include "pkce.hpp"

namespace {
  bool initialized( false );
}

// Helper to encode raw binary data to Base64URL without padding
std::string to_base64url( const unsigned char* bin, size_t bin_len ) {

  // 1. Get the required length using libsodium's URL-safe variant
  size_t max_encoded_len = sodium_base64_encoded_len( bin_len, sodium_base64_VARIANT_URLSAFE_NO_PADDING );

  std::string encoded(max_encoded_len, '\0');

  // 2. Perform the actual binary-to-base64 conversion
  sodium_bin2base64( &encoded[0], encoded.size(), bin, bin_len, sodium_base64_VARIANT_URLSAFE_NO_PADDING );

  // Trim the trailing null terminator inserted by libsodium
  if ( !encoded.empty() && encoded.back() == '\0' ) {
    encoded.pop_back();
  }

  return encoded;
}

PkcePair generate_pkce() {

  if ( !initialized ) {
    if ( sodium_init() < 0 ) {
      BOOST_LOG_TRIVIAL(error) << "Failed to initialize libsodium";
      //return EXIT_FAILURE;
    }
    else {
      initialized = true;
    }
  }

  // PKCE code_verifier requires a high-entropy string (43 to 128 characters long)
  // 32 random bytes will produce a ~43 character Base64URL string.
  constexpr size_t VERIFIER_BYTES = 32;
  unsigned char raw_verifier[ VERIFIER_BYTES ];

  // 1. Securely generate random bytes using libsodium's hardware-backed CSPRNG
  randombytes_buf( raw_verifier, VERIFIER_BYTES );

  // 2. Base64URL encode the random bytes to get the code_verifier string
  std::string code_verifier = to_base64url( raw_verifier, VERIFIER_BYTES );

  // 3. Compute the SHA-256 hash of the code_verifier string (not the raw bytes!)
  unsigned char raw_challenge[ crypto_hash_sha256_BYTES ];
  crypto_hash_sha256( raw_challenge, reinterpret_cast<const unsigned char*>( code_verifier.data() ), code_verifier.size() );

  // 4. Base64URL encode the SHA-256 hash output to get the code_challenge
  std::string code_challenge = to_base64url( raw_challenge, crypto_hash_sha256_BYTES );

  return { code_verifier, code_challenge };
}
