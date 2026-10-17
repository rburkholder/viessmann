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
 * File:    main.cpp
 * Author:  raymond@burkholder.net
 * Project: viessmann
 * Created: October 12, 2026 18:10:39
 */

#include <thread>

#include <boost/log/trivial.hpp>

#include <boost/asio/io_context.hpp>

#include <boost/url/url.hpp>
#include <boost/url/params_ref.hpp>

#include <sodium.h>

#include <ou/rest/handler.hpp>
#include <ou/rest/one_shot.hpp>

#include <ou/anomalon/listen.hpp>
#include <ou/anomalon/task_group.hpp>
#include <ou/anomalon/handle_signals.hpp>

#include "config.hpp"

namespace asio = boost::asio; // from <boost/asio/context.hpp>
//namespace ssl  = asio::ssl;   // from <boost/asio/ssl.hpp>

// Helper to encode raw binary data to Base64URL without padding
std::string to_base64url(const unsigned char* bin, size_t bin_len) {
  // 1. Get the required length using libsodium's URL-safe variant
  size_t max_encoded_len = sodium_base64_encoded_len(bin_len, sodium_base64_VARIANT_URLSAFE_NO_PADDING);

  std::string encoded(max_encoded_len, '\0');

  // 2. Perform the actual binary-to-base64 conversion
  sodium_bin2base64(&encoded[0], encoded.size(), bin, bin_len, sodium_base64_VARIANT_URLSAFE_NO_PADDING);

  // Trim the trailing null terminator inserted by libsodium
  if (!encoded.empty() && encoded.back() == '\0') {
      encoded.pop_back();
  }

  return encoded;
}

struct PkcePair {
  std::string code_verifier;
  std::string code_challenge;
};

PkcePair generate_pkce() {
  // PKCE code_verifier requires a high-entropy string (43 to 128 characters long)
  // 32 random bytes will produce a ~43 character Base64URL string.
  constexpr size_t VERIFIER_BYTES = 32;
  unsigned char raw_verifier[VERIFIER_BYTES];

  // 1. Securely generate random bytes using libsodium's hardware-backed CSPRNG
  randombytes_buf(raw_verifier, VERIFIER_BYTES);

  // 2. Base64URL encode the random bytes to get the code_verifier string
  std::string code_verifier = to_base64url(raw_verifier, VERIFIER_BYTES);

  // 3. Compute the SHA-256 hash of the code_verifier string (not the raw bytes!)
  unsigned char raw_challenge[crypto_hash_sha256_BYTES];
  crypto_hash_sha256(raw_challenge, reinterpret_cast<const unsigned char*>(code_verifier.data()), code_verifier.size());

  // 4. Base64URL encode the SHA-256 hash output to get the code_challenge
  std::string code_challenge = to_base64url(raw_challenge, crypto_hash_sha256_BYTES);

  return {code_verifier, code_challenge};
}


int main(int argc, char **argv) {

  std::string sConfigFilename( "viessmann.cfg" );

  BOOST_LOG_TRIVIAL(info) << "(c)2026 One Unified Net Limited";

  // Check command line arguments.
  if( 2 == argc ) {
    sConfigFilename = argv[ 1 ];
  }

  config::Values choices;

  if ( Load( sConfigFilename, choices ) ) {}
  else {
    return EXIT_FAILURE;
  }

  if ( sodium_init() < 0 ) {
    BOOST_LOG_TRIVIAL(error) << "Failed to initialize libsodium\n";
    return EXIT_FAILURE;
  }

  PkcePair pkce = generate_pkce();

  BOOST_LOG_TRIVIAL(trace) << "--- PKCE Credentials Generated ---";
  BOOST_LOG_TRIVIAL(trace) << "Code Verifier:  " << pkce.code_verifier;
  BOOST_LOG_TRIVIAL(trace) << "Code Challenge: " << pkce.code_challenge;
  BOOST_LOG_TRIVIAL(trace) << "Method:         S256";

  boost::urls::url url;
  url.set_encoded_path( choices.sAuthPath );
  boost::urls::params_ref params( url.params() );
  params.append(
    {
      { "client_id", choices.sClientId },
      { "redirect_uri", choices.sUrlRedirect },
      { "scope", "IoT openid" },
      { "response_type", "code" },
      { "code_challenge_method", "S256" },
      { "code_challenge", pkce.code_challenge }
    }
  );

  BOOST_LOG_TRIVIAL(trace) << "path: " << url.path() + '?' + url.query();

  //return EXIT_SUCCESS;

  std::thread m_thread;

  boost::asio::io_context m_io;
  ssl::context m_ssl_context( ssl::context::tlsv12_client );

  auto request = std::make_shared<ou::rest::one_shot>( asio::make_strand( m_io ), m_ssl_context );
  request->post(
      choices.sAuthFQDN
    , choices.sAuthPort
    , url.path() + '?' + url.query()
    , ""
    , []( bool bStatus, int ec, const std::string& message ){
        BOOST_LOG_TRIVIAL(info) << "status=" << bStatus << ", message=" << message;
        if ( bStatus ) {
          // next step
        }
        else {
          // fail
        }
      }
    );

  m_io.run();

  return EXIT_SUCCESS;

  //https://iam.viessmann-climatesolutions.com/idp/v2/authorize?client_id=c86acbaaa2e7ff2377d36ae972fad2e6&code_challenge=J99I0OPUX-dvDr3l37EME6WB_6YZyRihFDEAUP5RGjY&code_challenge_method=S256&scope=IoT+openid&response_type=code&redirect_uri=https%3A%2F%2Fapp.developer.viessmann-climatesolutions.com
}