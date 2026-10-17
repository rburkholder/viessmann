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

#include <ou/rest/handler.hpp>
#include <ou/rest/one_shot.hpp>

#include <ou/anomalon/listen.hpp>
#include <ou/anomalon/task_group.hpp>
#include <ou/anomalon/handle_signals.hpp>

#include "pkce.hpp"
#include "config.hpp"

namespace asio = boost::asio; // from <boost/asio/context.hpp>
//namespace ssl  = asio::ssl;   // from <boost/asio/ssl.hpp>

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