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
 * File:    config.cpp
 * Author:  raymond@burkholder.net
 * Project: viessmann
 * Created: 2026/10/15 04:34:11
 */

#include <fstream>
#include <exception>
//#include <type_traits>

#include <boost/log/trivial.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "config.hpp"

namespace {


  static const std::string sValue_Authentication_FQDN( "auth_fqdn" );
  static const std::string sValue_Authentication_Port( "auth_port" );
  static const std::string sValue_Authentication_Path( "auth_path" );

  static const std::string sValue_Port_Management(  "port_mgmt" );
  static const std::string sValue_Port_Redirect( "port_redirect" );

  static const std::string sValue_Url_Api( "url_api" );
  static const std::string sValue_Url_Redirect( "url_redirect" );

  //static const std::string sValue_listen_address( "listen_address" );

  //template<typename T>
  //void log( const std::string& name, typename std::enable_if<std::is_pod<T&>::value>::type& dest ) {
  //  BOOST_LOG_TRIVIAL(info) << name << " = " << dest;
  //}

  void log( const std::string& name, uint16_t dest ) {
    BOOST_LOG_TRIVIAL(info) << name << " = " << dest;
  }

  void log( const std::string& name, const std::string& dest ) {
    BOOST_LOG_TRIVIAL(info) << name << " = " << dest;
  }

  template<typename T>
  bool parse( const std::string& sFileName, po::variables_map& vm, const std::string& name, T& dest ) {
    bool bOk = true;
    if ( 0 < vm.count( name ) ) {
      dest = vm[name].as<T>();
      //BOOST_LOG_TRIVIAL(info) << name << " = " << dest;
      log( name, dest );
    }
    else {
      BOOST_LOG_TRIVIAL(error) << sFileName << " missing '" << name << "='";
      bOk = false;
    }
    return bOk;
  }
}

namespace config {

bool Load( const std::string& sFileName, Values& values ) {

  bool bOk( true );

  try {

    po::options_description config( "web.boost config file" );
    config.add_options()

      ( sValue_Authentication_FQDN.c_str(), po::value<std::string>( &values.sAuthFQDN ), "authentication fqdn" )
      ( sValue_Authentication_Port.c_str(), po::value<std::string>( &values.sAuthPort ), "authentication port" )
      ( sValue_Authentication_Path.c_str(), po::value<std::string>( &values.sAuthPath ), "authentication path" )

      ( sValue_Port_Management.c_str(), po::value<uint16_t>( &values.nPortManagement )->default_value( 8087 ), "management port" )
      ( sValue_Port_Redirect.c_str(), po::value<uint16_t>( &values.nPortRedirect )->default_value( 4200 ), "redirect port" )

      ( sValue_Url_Api.c_str(), po::value<std::string>( &values.sUrlApi ), "access to api" ) // don't know if this is diferent from previous
      ( sValue_Url_Redirect.c_str(), po::value<std::string>( &values.sUrlRedirect ), "redirect with token availbale" )

      ;
    po::variables_map vm;
    //po::store( po::parse_command_line( argc, argv, config ), vm );

    std::ifstream ifs( sFileName.c_str() );

    if ( !ifs ) {
      BOOST_LOG_TRIVIAL(error) << "config file " << sFileName << " does not exist";
      bOk = false;
    }
    else {
      po::store( po::parse_config_file( ifs, config), vm );

      bOk &= parse<std::string>( sFileName, vm, sValue_Authentication_FQDN, values.sAuthFQDN );
      bOk &= parse<std::string>( sFileName, vm, sValue_Authentication_Port, values.sAuthPort );
      bOk &= parse<std::string>( sFileName, vm, sValue_Authentication_Path, values.sAuthPath );

      bOk &= parse<uint16_t>( sFileName, vm, sValue_Port_Management, values.nPortManagement );
      bOk &= parse<uint16_t>( sFileName, vm, sValue_Port_Redirect, values.nPortRedirect );

      bOk &= parse<std::string>( sFileName, vm, sValue_Url_Api, values.sUrlApi );
      bOk &= parse<std::string>( sFileName, vm, sValue_Url_Redirect, values.sUrlRedirect );

      //bOk &= parse<std::string>( sFileName, vm, sValue_listen_address, values.sListenAddress );

    }
  }
  catch( std::exception& e ) {
    BOOST_LOG_TRIVIAL(error) << sFileName << " parse error: " << e.what();
    bOk = false;
  }

  return bOk;
}

} // namespace config