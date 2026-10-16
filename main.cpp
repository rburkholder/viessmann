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
 * Project: viesmann
 * Created: October 12, 2026 18:10:39
 */

#include <thread>

#include <boost/log/trivial.hpp>

#include <boost/asio/io_context.hpp>

#include <ou/rest/handler.hpp>
#include <ou/rest/one_shot.hpp>

#include <ou/anomalon/listen.hpp>
#include <ou/anomalon/task_group.hpp>
#include <ou/anomalon/handle_signals.hpp>

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

  std::thread m_thread;

  boost::asio::io_context m_io;
  ssl::context m_ssl_context( ssl::context::tlsv12_client );

  auto request = std::make_shared<ou::rest::one_shot>( asio::make_strand( m_io ), m_ssl_context );
  request->get(
      choices.sAuthFQDN
    , choices.sAuthPort
    , choices.sAuthPath
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
}