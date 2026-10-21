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

#include <boost/json.hpp>

#include <sol/sol.hpp>

#include <ou/rest/one_shot.hpp>

#include <ou/anomalon/listen.hpp>
#include <ou/anomalon/task_group.hpp>
#include <ou/anomalon/handle_methods_lua.hpp>
#include <ou/anomalon/handle_signals.hpp>

#include "pkce.hpp"
#include "config.hpp"

namespace asio = boost::asio; // from <boost/asio/context.hpp>
namespace ssl  = asio::ssl;   // from <boost/asio/ssl.hpp>

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

  boost::asio::io_context m_io;

  // The SSL context is required, and holds certificates
  // 1. Initialize with the generic 'tls' method (enables all TLS versions supported by OpenSSL)
  ssl::context ssl_ctx{ ssl::context::tls };

  // 2. Clear options to make sure no defaults are conflicting
  ssl_ctx.clear_options( 0 );

  // 3. Explicitly disable older protocols, leaving only TLSv1.2 and TLSv1.3 active
  ssl_ctx.set_options(
      ssl::context::default_workarounds
    | ssl::context::no_sslv2
    | ssl::context::no_sslv3
    | ssl::context::no_tlsv1
    | ssl::context::no_tlsv1_1
  );

  // Track coroutines
  task_group task_group{ m_io.get_executor() };

  auto const listen_address  = asio::ip::make_address( choices.sListenAddress );
  auto const listen_endpoint = asio::ip::tcp::endpoint{ listen_address, choices.nListenPort };

  // Create and launch a listening coroutine
  net::co_spawn(
    net::make_strand( m_io ),  // the strand can be removed to allow access to all threads
    listen( task_group, ssl_ctx, listen_endpoint, choices.sContentDirectory ),
    task_group.adapt(
      []( std::exception_ptr e ) {
        if( e ) {
          try {
              std::rethrow_exception(e);
          }
          catch ( std::exception& e ) {
            BOOST_LOG_TRIVIAL(error) << "Error in listener: " << e.what();
          }
        }
      })
    );

  // Create and launch a signal handler coroutine
  net::co_spawn(
    net::make_strand( m_io ),
    handle_signals( task_group ), net::detached
  );

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
      { "client_id", choices.sAuthClientId },
      { "redirect_uri", choices.sUrlRedirect },
      { "scope", "IoT openid" },
      { "response_type", "code" },
      { "code_challenge_method", "S256" },
      { "code_challenge", pkce.code_challenge }
    }
  );

  BOOST_LOG_TRIVIAL(trace) << "path: " << url.path() + '?' + url.query();

  std::string viessmann_response;
  viessmann_response = "https://" + choices.sAuthFQDN + url.path() + "authorize" + '?' + url.query();

  //return EXIT_SUCCESS;

  ssl::context m_ssl_context( ssl::context::tlsv12_client );
/*
  auto request = std::make_shared<ou::rest::one_shot>( asio::make_strand( m_io ), m_ssl_context );
  request->post(
      choices.sAuthFQDN
    , choices.sAuthPort
    , url.path() + '?' + url.query()
    , ""
    , [&viessmann_response]( bool bStatus, int ec, const std::string& message ){
        BOOST_LOG_TRIVIAL(info) << "status=" << bStatus << ", message=" << message;
        if ( bStatus ) {
          viessmann_response = std::move( message );
        }
        else {
          // fail
        }
      }
    );
*/

  bool bAuthorizationDone( false );

  LoadCustomCode(
    [&viessmann_response,&choices,&pkce,&m_io,&bAuthorizationDone]( sol::state& ss ){
      ss.set_function(
        "get_viessmann_body",
        [&viessmann_response]()->std::string {
          return viessmann_response;
        }
      );
      ss.set_function(
        "authorization_done",
        [&bAuthorizationDone]()->bool {
          return bAuthorizationDone;
        }
      );
      ss.set_function(
        "post_authorization",
        [&choices,&pkce,&m_io,&bAuthorizationDone]( std::string_view svCode ){
          bAuthorizationDone = true;
          boost::urls::url url;
          url.set_encoded_path( choices.sAuthPath );
          boost::urls::params_ref params( url.params() );
          params.append(
            {
              { "client_id", choices.sAuthClientId },
              { "redirect_uri", choices.sUrlRedirect },
              { "grant_type", "authorization_code" },
              { "code_verifier", pkce.code_verifier },
              { "code", svCode }
            }
          );

          BOOST_LOG_TRIVIAL(trace) << "step 2 path: " << url.path() + "token?" + url.query();

          //return EXIT_SUCCESS;

          ssl::context m_ssl_context( ssl::context::tlsv12_client );
          auto request = std::make_shared<ou::rest::one_shot>( asio::make_strand( m_io ), m_ssl_context );
          request->post(
              choices.sAuthFQDN
            , choices.sAuthPort
            , [&url]( ou::rest::handler::pRequestStringBody_t& pRequest ){
                pRequest->target( url.path() + "token" );
                pRequest->body() = url.query();
                pRequest->prepare_payload();
                pRequest->set( http::field::content_type, "application/x-www-form-urlencoded" );
              }
            , [&choices,&m_io]( bool bStatus, int ec, const std::string& message ){
                if ( bStatus ) {
                  boost::json::value jv = boost::json::parse( message );
                  boost::json::object const& obj = jv.as_object();
                  std::string access_token;
                  std::string token_type;
                  uint32_t expires_in;
                  access_token = boost::json::value_to<std::string>( obj.at( "access_token" ) );
                  token_type = boost::json::value_to<std::string>( obj.at( "token_type" ) );
                  expires_in = boost::json::value_to<uint32_t>( obj.at( "expires_in" ) );
                  ssl::context m_ssl_context( ssl::context::tlsv12_client );

                  auto request = std::make_shared<ou::rest::one_shot>( asio::make_strand( m_io ), m_ssl_context );
                  request->get(
                    "api.viessmann-climatesolutions.com"
                  , choices.sAuthPort
                  , [&token_type,&access_token]( ou::rest::handler::pRequestEmptyBody_t& pRequest ){
                      //pRequest->target( "/iot/v2/equipment/gateways" );
                      pRequest->target( "/iot/v2/equipment/installations" );
                      pRequest->set( "Authorization", token_type + " " + access_token );
                    }
                  , []( bool bStatus, int ec, const std::string& message ){
                      BOOST_LOG_TRIVIAL(trace) << "step 3: " << bStatus << ',' << ec << ',' << message;
                    }
                  );
                }
                else {
                  // fail
                  BOOST_LOG_TRIVIAL(info) << "step 2 status=" << bStatus << ", message=" << message;
                }
              }
            );
        }
      );
    } );

  std::thread m_thread;

  m_io.run();

  return EXIT_SUCCESS;

  //https://iam.viessmann-climatesolutions.com/idp/v2/authorize?client_id=c86acbaaa2e7ff2377d36ae972fad2e6&code_challenge=J99I0OPUX-dvDr3l37EME6WB_6YZyRihFDEAUP5RGjY&code_challenge_method=S256&scope=IoT+openid&response_type=code&redirect_uri=https%3A%2F%2Fapp.developer.viessmann-climatesolutions.com
}