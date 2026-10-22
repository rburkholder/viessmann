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

  static const std::string sValue_Listen_Address( "listen_address" );
  static const std::string sValue_Listen_Port( "listen_port" );

  static const std::string sValue_Content_Directory( "content_directory" );

  static const std::string sValue_Authentication_FQDN( "auth_fqdn" );
  static const std::string sValue_Authentication_Port( "auth_port" );
  static const std::string sValue_Authentication_Path( "auth_path" );
  static const std::string sValue_Authentication_Client_Id( "auth_client_id" );
  static const std::string sValue_Authentication_Redirect( "auth_redirect" );

  static const std::string sValue_Api_FQDN( "api_fqdn" );
  static const std::string sValue_Api_Port( "api_port" );

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

      ( sValue_Content_Directory.c_str(), po::value<std::string>( &values.sContentDirectory ), "content directory" )

      ( sValue_Listen_Port.c_str(), po::value<uint16_t>( &values.nListenPort )->default_value( 8087 ), "listen port" )
      ( sValue_Listen_Address.c_str(), po::value<std::string>( &values.sListenAddress )->default_value( "127.0.0.1" ), "listen address" )

      ( sValue_Authentication_FQDN.c_str(), po::value<std::string>( &values.sAuthFQDN ), "authentication fqdn" )
      ( sValue_Authentication_Port.c_str(), po::value<std::string>( &values.sAuthPort ), "authentication port" )
      ( sValue_Authentication_Path.c_str(), po::value<std::string>( &values.sAuthPath ), "authentication path" )
      ( sValue_Authentication_Client_Id.c_str(), po::value<std::string>( &values.sAuthClientId ), "viessmann client id" )
      ( sValue_Authentication_Redirect.c_str(), po::value<std::string>( &values.sAuthRedirect ), "redirect with token available" )

      ( sValue_Api_FQDN.c_str(), po::value<std::string>( &values.sApiFQDN ), "access to api host" )
      ( sValue_Api_Port.c_str(), po::value<std::string>( &values.sApiPort ), "access to api port" )
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

      bOk &= parse<std::string>( sFileName, vm, sValue_Content_Directory, values.sContentDirectory );

      bOk &= parse<uint16_t>( sFileName, vm, sValue_Listen_Port, values.nListenPort );
      bOk &= parse<std::string>( sFileName, vm, sValue_Listen_Address, values.sListenAddress );

      bOk &= parse<std::string>( sFileName, vm, sValue_Authentication_FQDN, values.sAuthFQDN );
      bOk &= parse<std::string>( sFileName, vm, sValue_Authentication_Port, values.sAuthPort );
      bOk &= parse<std::string>( sFileName, vm, sValue_Authentication_Path, values.sAuthPath );
      bOk &= parse<std::string>( sFileName, vm, sValue_Authentication_Client_Id, values.sAuthClientId );
      bOk &= parse<std::string>( sFileName, vm, sValue_Authentication_Redirect, values.sAuthRedirect );


      bOk &= parse<std::string>( sFileName, vm, sValue_Api_FQDN, values.sApiFQDN );
      bOk &= parse<std::string>( sFileName, vm, sValue_Api_Port, values.sApiPort );
    }
  }
  catch( std::exception& e ) {
    BOOST_LOG_TRIVIAL(error) << sFileName << " parse error: " << e.what();
    bOk = false;
  }

  return bOk;
}

} // namespace config