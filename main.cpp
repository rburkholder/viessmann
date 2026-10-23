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
#include <fstream>

#include <boost/log/trivial.hpp>

#include <boost/asio/io_context.hpp>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <sol/sol.hpp>

#include <ou/anomalon/listen.hpp>
#include <ou/anomalon/task_group.hpp>
#include <ou/anomalon/handle_methods_lua.hpp>
#include <ou/anomalon/handle_signals.hpp>

#include "config.hpp"
#include "viessmann.hpp"

namespace asio = boost::asio; // from <boost/asio/context.hpp>
namespace ssl  = asio::ssl;   // from <boost/asio/ssl.hpp>

int main(int argc, char **argv) {

  std::string sConfigFilename( "viessmann.cfg" );
  std::string sStateFileName( "viessmann.state" );

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

  boost::asio::io_context m_ioc;

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
  task_group task_group{ m_ioc.get_executor() };

  auto const listen_address  = asio::ip::make_address( choices.sListenAddress );
  auto const listen_endpoint = asio::ip::tcp::endpoint{ listen_address, choices.nListenPort };

  // Create and launch a listening coroutine
  net::co_spawn(
    net::make_strand( m_ioc ),  // the strand can be removed to allow access to all threads
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
    net::make_strand( m_ioc ),
    handle_signals( task_group ), net::detached
  );

  viessmann viessmann( choices, m_ioc );

  try {
    BOOST_LOG_TRIVIAL(info) << "loading config ...";
    std::ifstream ifs( sStateFileName );
    boost::archive::text_iarchive ia(ifs);
    ia & viessmann;
    BOOST_LOG_TRIVIAL(info) << "loading done";
  }
  catch( const std::runtime_error& e ) {
    BOOST_LOG_TRIVIAL(error) << "load exception " << e.what();
  }
  catch(...) {
    BOOST_LOG_TRIVIAL(error) << "load exception (generic)";
  }

  viessmann.set(
    [&sStateFileName,&viessmann](){
      BOOST_LOG_TRIVIAL(info) << "saving Config ...";
      std::ofstream ofs( sStateFileName );
      boost::archive::text_oarchive oa(ofs);
      oa & viessmann;
      BOOST_LOG_TRIVIAL(info) << "saving done";
    }
  );

  // TODO: refresh token
  //   step thorugh initial acquisition
  //   perform polling

  LoadCustomCode(
    [&viessmann]( sol::state& ss ){
      ss.set_function(
        "authz_required",
        [&viessmann]()->bool{
          return viessmann.authz_required();
        }
      );
      ss.set_function(
        "get_viessmann_body",
        [&viessmann]()->std::string {
          return viessmann.authz_url();
        }
      );
      ss.set_function(
        "post_authorization",
        [&viessmann]( std::string_view svCode ){
          viessmann.authz( svCode );
        }
      );
    } );

  std::thread m_thread;

  m_ioc.run();

  return EXIT_SUCCESS;

  //https://iam.viessmann-climatesolutions.com/idp/v2/authorize?client_id=c86acbaaa2e7ff2377d36ae972fad2e6&code_challenge=J99I0OPUX-dvDr3l37EME6WB_6YZyRihFDEAUP5RGjY&code_challenge_method=S256&scope=IoT+openid&response_type=code&redirect_uri=https%3A%2F%2Fapp.developer.viessmann-climatesolutions.com
}