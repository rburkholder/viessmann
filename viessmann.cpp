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
 * File:    viessmann.cpp
 * Project: viessmann
 * Author:  raymond@burkholder.net
 * Created: October 20, 2026 19:11:59
 */

// todo
//   transmit to mqtt
//   dynamically update gui with websocket
//   switch from m_ioc to task_group, to handle cancellation signal?

// really need local access.  the api went under maintenance starting at 19:00 2026/10/24

//#include <typeinfo>
#include <fstream>
#include <exception>

#include <boost/log/trivial.hpp>

#include <boost/lexical_cast.hpp>

#include <boost/asio/strand.hpp>
#include <boost/asio/ssl/context.hpp>

#include <boost/url/url.hpp>
#include <boost/url/params_ref.hpp>

#include <boost/date_time/gregorian/greg_duration.hpp>

#include <boost/json.hpp>

#include <fmt/format.h>

#include <ou/mqtt/mqtt.hpp>
#include <ou/rest/one_shot.hpp>

#include "viessmann.hpp"
#include "convert_json.hpp"

namespace asio = boost::asio; // from <boost/asio/context.hpp>
namespace ssl  = asio::ssl;   // from <boost/asio/ssl.hpp>
namespace json = boost::json;

void bypass_browser() {

/*
  auto request = std::make_shared<ou::rest::one_shot>( asio::make_strand( m_io ), ssl_context );
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
}

namespace {
  ssl::context ssl_context( ssl::context::tlsv12_client );
}

viessmann::viessmann( const config::Values& choices, asio::io_context& ioc )
: m_choices( choices )
, m_ioc( ioc )
, m_timerFeature( ioc )
, m_timerTokenRefresh( ioc )
, m_fSave( nullptr )
, m_dtAccessTokenExpiry( boost::date_time::not_a_date_time )
, m_dtRefreshTokenExpiry( boost::date_time::not_a_date_time )
{
  try {
    m_pMqtt = std::make_unique<ou::Mqtt>( choices.mqtt );
  }
  catch ( const ou::Mqtt::runtime_error& e ) {
    BOOST_LOG_TRIVIAL(error) << "mqtt error: " << e.what() << '(' << e.rc << ')';
    throw e;
  }
}

viessmann::~viessmann() {
  m_pMqtt.reset();
};

void viessmann::set( fSave_t && f ) {
  m_fSave = std::move( f );
}

bool viessmann::authz_required() const {
  if ( boost::date_time::not_a_date_time == m_dtAccessTokenExpiry ) {
    return true;
  }
  const boost::posix_time::ptime now( boost::posix_time::microsec_clock::universal_time() );
  return ( now >= m_dtAccessTokenExpiry );
}

std::string viessmann::authz_url() {

  m_pkce = generate_pkce();

  BOOST_LOG_TRIVIAL(trace) << "--- PKCE Credentials Generated ---";
  BOOST_LOG_TRIVIAL(trace) << "Code Verifier:  " << m_pkce.code_verifier;
  BOOST_LOG_TRIVIAL(trace) << "Code Challenge: " << m_pkce.code_challenge;
  BOOST_LOG_TRIVIAL(trace) << "Method:         S256";

  boost::urls::url url;
  url.set_encoded_path( m_choices.sAuthPath );
  boost::urls::params_ref params( url.params() );
  params.append(
    {
      { "client_id", m_choices.sAuthClientId },
      { "redirect_uri", m_choices.sAuthRedirect },
      { "scope", "IoT User offline_access" },
      { "response_type", "code" },
      { "code_challenge_method", "S256" },
      { "code_challenge", m_pkce.code_challenge }
    }
  );

  BOOST_LOG_TRIVIAL(trace) << "path: " << url.path() + '?' + url.query();

  return "https://" + m_choices.sAuthFQDN + url.path() + "authorize" + '?' + url.query();

}

void viessmann::authz( const std::string_view svCode ) {

  boost::urls::url url;
  url.set_encoded_path( m_choices.sAuthPath );
  boost::urls::params_ref params( url.params() );
  params.append(
    {
      { "client_id", m_choices.sAuthClientId },
      { "redirect_uri", m_choices.sAuthRedirect },
      { "grant_type", "authorization_code" },
      { "code_verifier", m_pkce.code_verifier },
      { "code", svCode }
    }
  );

  BOOST_LOG_TRIVIAL(trace) << "iam path: " << url.path() + "token?" + url.query();

  auto request = std::make_shared<ou::rest::one_shot>( asio::make_strand( m_ioc ), ssl_context );
  request->post(
      m_choices.sAuthFQDN
    , m_choices.sAuthPort
    , [&url]( ou::rest::handler::pRequestStringBody_t& pRequest ){
        pRequest->target( url.path() + "token" );
        pRequest->body() = url.query();
        pRequest->prepare_payload();
        pRequest->set( http::field::content_type, "application/x-www-form-urlencoded" );
      }
    , [this]( bool bStatus, int ec, const std::string& message ){
        //BOOST_LOG_TRIVIAL(error) << "iam request=" << bStatus << ", message=" << message;
        if ( bStatus ) {
          boost::system::error_code jec;
          json::value jv = boost::json::parse( message, jec );

          BOOST_LOG_TRIVIAL(info) << "refresh token=" << m_iam.sRefreshToken;

          m_iam = json::value_to<iam_result_t>( jv );

          set_refresh_access_token_full_duration();

          // For refresh tokens, the TTL is 15552000 seconds (180 days).
          const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
          m_dtRefreshTokenExpiry = now + boost::gregorian::date_duration( 180 - 1 );

          save();

          start();
        }
        else {
          // fail
          BOOST_LOG_TRIVIAL(error) << "iam request=" << bStatus << ", message=" << message;
        }
      }
    );
}

void viessmann::start() {
  if ( authz_required() ) {
    BOOST_LOG_TRIVIAL(error) << "authorization required";
  }
  else {
    set_timer_refresh_access_token();
    get_installations();
  }
}

void viessmann::stop() {
  auto count = m_timerFeature.cancel();
  BOOST_LOG_TRIVIAL(info) << "feature timer operations cancelled: " << count;
}

// https://api.viessmann-climatesolutions.com/documentation/static/iot-overview

void viessmann::get_installations() {
  auto request = std::make_shared<ou::rest::one_shot>( asio::make_strand( m_ioc ), ssl_context );
  request->get(
    m_choices.sApiFQDN
  , m_choices.sApiPort
  , [this]( ou::rest::handler::pRequestEmptyBody_t& pRequest ){
      pRequest->target( "/iot/v2/equipment/installations" );
      pRequest->set( "Authorization", m_iam.sTokenType + " " + m_iam.sAccessToken );
    }
  , [this]( bool bStatus, int ec, const std::string& message ){
      if ( bStatus ) {
        try {
          //BOOST_LOG_TRIVIAL(trace) << "installations: " << bStatus << ',' << ec << ',' << message;
          boost::system::error_code jec;
          json::value jv = boost::json::parse( message, jec );
          json::object const& obj = jv.as_object();
          m_vInstallation = json::value_to<vInstallation_t>( obj.at( "data" ) );
          assert( 1 <= m_vInstallation.size() );
          //BOOST_LOG_TRIVIAL(info) << "site " << m_nSiteId << ": " << m_sSiteStatus;
          m_sInstallation = boost::lexical_cast<std::string>( m_vInstallation[ 0 ].id );

          get_gateways();
        }
        catch ( const std::exception& e ) {
          BOOST_LOG_TRIVIAL(error) << "get installations status: " << bStatus << ',' << ec << ',' << message;
          BOOST_LOG_TRIVIAL(error) << "get_installations error: " << e.what();
        }
      }
      else {
        BOOST_LOG_TRIVIAL(error) << "get installations error: " << bStatus << ',' << ec << ',' << message;
      }
    }
  );
}

void viessmann::get_gateways() {
  auto request = std::make_shared<ou::rest::one_shot>( asio::make_strand( m_ioc ), ssl_context );
  request->get(
    m_choices.sApiFQDN
  , m_choices.sApiPort
  , [this]( ou::rest::handler::pRequestEmptyBody_t& pRequest ){
      pRequest->target( "/iot/v2/equipment/gateways" );
      pRequest->set( "Authorization", m_iam.sTokenType + " " + m_iam.sAccessToken );
    }
  , [this]( bool bStatus, int ec, const std::string& message ){
      if ( bStatus ) {
        //BOOST_LOG_TRIVIAL(trace) << "get gateways: " << bStatus << ',' << ec << ',' << message;
        boost::system::error_code jec;
        json::value jv = boost::json::parse( message, jec );
        json::object const& obj = jv.as_object();
        m_vGateway = json::value_to<vGateway_t>( obj.at( "data" ) );
        assert( 1 <= m_vGateway.size() );
        //BOOST_LOG_TRIVIAL(info) << "gateway '" << m_sGatewaySerial << "': " << m_sGatewayStatus << ", changed:" << m_sGatewayLastStatusChanged;
        //assert( m_nGatewaySiteId == m_nSiteId );

        //save();

        get_devices();
      }
      else {
        BOOST_LOG_TRIVIAL(error) << "get gateways error: " << bStatus << ',' << ec << ',' << message;
      }
    }
  );
}

void viessmann::save() {
  if ( m_fSave ) m_fSave();
}

void viessmann::get_devices() {
  auto request = std::make_shared<ou::rest::one_shot>( asio::make_strand( m_ioc ), ssl_context );
  request->get(
    m_choices.sApiFQDN
  , m_choices.sApiPort
  , [this]( ou::rest::handler::pRequestEmptyBody_t& pRequest ){
      const std::string target(
        fmt::format(
          "/iot/v2/equipment/installations/{}/gateways/{}/devices",
          m_vInstallation[0].id, m_vGateway[ 0 ].serial
        )
      );
      BOOST_LOG_TRIVIAL(trace) << "devices request: '" << target << "'";
      pRequest->target( target );
      pRequest->set( "Authorization", m_iam.sTokenType + " " + m_iam.sAccessToken );
    }
  , [this]( bool bStatus, int ec, const std::string& message ){
      if ( bStatus ) {
        //BOOST_LOG_TRIVIAL(trace) << "get devices: " << bStatus << ',' << ec << ',' << message;
        boost::system::error_code jec;
        json::value jv = boost::json::parse( message, jec );
        json::object const& obj = jv.as_object();
        m_vDevice = json::value_to<vDevice_t>( obj.at( "data" ) );
        assert( 0 < m_vDevice.size() );
        m_sDevice = m_vDevice[ 0 ].id;

        get_features();
      }
      else {
        BOOST_LOG_TRIVIAL(error) << "get devices error: " << bStatus << ',' << ec << ',' << message;
      }
    }
  );
}

void viessmann::get_features() {
  auto request = std::make_shared<ou::rest::one_shot>( asio::make_strand( m_ioc ), ssl_context );
  request->get(
    m_choices.sApiFQDN
  , m_choices.sApiPort
  , [this]( ou::rest::handler::pRequestEmptyBody_t& pRequest ){
      const std::string target(
        fmt::format(
          "/iot/v2/features/installations/{}/gateways/{}/devices/{}/features",
          m_vInstallation[0].id, m_vGateway[ 0 ].serial, "0" )
      );
      BOOST_LOG_TRIVIAL(trace) << "features request: '" << target << "'";
      pRequest->target( target );
      pRequest->set( "Authorization", m_iam.sTokenType + " " + m_iam.sAccessToken );
    }
  , [this]( bool bStatus, int ec, const std::string& message ){
      if ( bStatus ) {
        //BOOST_LOG_TRIVIAL(trace) << "get features: " << bStatus << ',' << ec << ',' << message;
        try {
          boost::system::error_code jec;
          json::value jv = boost::json::parse( message, jec );
          json::object const& obj = jv.as_object();

          if ( obj.contains( "data" ) ) {

            if ( true ) {
              BOOST_LOG_TRIVIAL(info) << "saving feature";
              std::ofstream ofs( "viessmann.feature.json" );
              ofs << message;
            }

            const std::string sPrefix = m_choices.mqtt.sTopic + '/' + m_sInstallation + '/' + m_sDevice + '/';

            try {
              const json::value jvData = obj.at( "data" ); // this works
              const json::array objData = jvData.as_array();
              //auto const begin( objData.begin() );
              for ( const auto& feature: objData ) {
                const auto result = feature.try_at( "properties" );
                if ( result ) {
                  std::string name( json::value_to<std::string_view>( feature.at( "feature" ) ) );
                  std::replace( name.begin(), name.end(), '.', '/' );
                  const json::value valResult = *result;
                  if ( valResult.as_object().empty() ) {
                    //BOOST_LOG_TRIVIAL(trace) << "feature: " << name << " is null";
                  }
                  else {
                    //BOOST_LOG_TRIVIAL(trace)
                    //  << sPrefix
                    //  << name
                    //  << ": '"
                    //  << *result
                    //  << "'"
                    //  ;
                    m_pMqtt->Publish(
                      sPrefix + name,
                      json::serialize( *result ),
                      []( bool bStatus, int code ){
                        if ( bStatus ); // ok
                        else {
                          BOOST_LOG_TRIVIAL(error) << "mqtt publish error: " << code;
                        }
                      } );
                  }
                }
                else {
                  BOOST_LOG_TRIVIAL(trace) << "feature: " << json::value_to<std::string_view>( feature.at( "feature" ) ) << " no properties";
                }

              }
            }
            catch ( const std::exception& e ) {
              BOOST_LOG_TRIVIAL(debug) << "decoding error: " << e.what();
            }
/*
            m_vFeature = json::value_to<vFeature_t>( obj.at( "data" ) );
            json::object objMqtt;

            for ( const feature_t& feature: m_vFeature ) {
              try {
                switch ( feature.property.value.type ) {
                  case value_type_t::boolean:
                    {
                      bool b( std::get<bool>( feature.property.value.value ) );
                      objMqtt[ feature.sName ] = { { "value", b } };
                      BOOST_LOG_TRIVIAL(trace) << feature.sName << ": " << b;
                    }
                    break;
                  case value_type_t::int_:
                    {
                      int64_t int_( std::get<int64_t>( feature.property.value.value ) );
                      objMqtt[ feature.sName ] = { { "value", int_ }, { "units", feature.property.value.unit } };
                      BOOST_LOG_TRIVIAL(trace) << feature.sName << ": " << int_ << ' ' << feature.property.value.unit;
                    }
                    break;
                  case value_type_t::dbl_:
                    {
                      double dbl( std::get<double>( feature.property.value.value ) );
                      objMqtt[ feature.sName ] = { { "value", dbl }, { "units", feature.property.value.unit } };
                      BOOST_LOG_TRIVIAL(trace) << feature.sName << ": " << dbl << ' ' << feature.property.value.unit;
                    }
                    break;
                  case value_type_t::string:
                    {
                      const std::string& s( std::get<std::string>( feature.property.value.value ) );
                      objMqtt[ feature.sName ] = { { "value", s } };
                      BOOST_LOG_TRIVIAL(trace) << feature.sName << ": \"" << s << '"';
                    }
                    break;
                  default:
                    //BOOST_LOG_TRIVIAL(trace) << feature.sName << "='" << (int)feature.property.value.type << "' " << feature.property.value.unit;
                    break;
                }
              }
              catch ( const std::exception& e ) {
                BOOST_LOG_TRIVIAL(error) << "output error (" << feature.sName << "): " << e.what();
              }
            }

            BOOST_LOG_TRIVIAL(trace) << "json: " << json::serialize( objMqtt );

            m_pMqtt->Publish(
              m_choices.mqtt.sTopic,
              json::serialize( objMqtt ),
              []( bool bStatus, int code ){
                if ( bStatus ); // ok
                else {
                  BOOST_LOG_TRIVIAL(error) << "mqtt publish error: " << code;
                }
              } );
*/
          }
          else {
            //viessmann_error_t = json::value_to<vFeature_t>( obj );
            const viessmann_error_t error( json::value_to<viessmann_error_t>( jv ) );
            BOOST_LOG_TRIVIAL(warning) << "feature request error" << error.sErrorType << ',' << error.payload.sReason << ',' << error.payload.sMessage;
          }

          m_timerFeature.expires_after( boost::asio::chrono::seconds( 60 ) );
          m_timerFeature.async_wait(
            [this]( const boost::system::error_code& error ){
              if ( error ) {
                BOOST_LOG_TRIVIAL(error) << "feature timer error (no continue): " << error.message();
              }
              else {
                get_features();
              }
            }
          );
        }
        catch ( const std::exception& e ) {
          BOOST_LOG_TRIVIAL(error) << "get features std::exception: " << bStatus << ',' << ec << ',' << message;
        }

      }
      else {
        BOOST_LOG_TRIVIAL(error) << "get features error: " << bStatus << ',' << ec << ',' << message;
      }
    }
  );
}

void viessmann::set_timer_refresh_access_token() {

  // starting from load file
  //   no file, therefore no time -> refresh
  //   file, no time remaining -> refresh
  //   file, time remaining > 1 sec, refresh after difference in seconds
  // refresh
  //   refresh after full time delay

  BOOST_LOG_TRIVIAL(trace) << "set_timer_refresh_access_token()";

  const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
  if ( now > ( m_dtAccessTokenExpiry - boost::posix_time::time_duration( 0, 0, 1 ) ) ) {
    refresh_access_token();
  }
  else {
    const auto delta = m_dtAccessTokenExpiry - now;
    // https://api.viessmann-climatesolutions.com/documentation/static/authentication
    //m_dtAccessTokenExpiry = now + delta;
    start_renewal_delay( delta );

  }

}

void viessmann::set_refresh_access_token_full_duration() {
  //BOOST_LOG_TRIVIAL(trace) << "set_refresh_access_token_full_duration()";

  // https://api.viessmann-climatesolutions.com/documentation/static/authentication
  const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
  const auto delta = boost::posix_time::time_duration( 0, 0, m_iam.nSecondsToExpiry - 11 );
  m_dtAccessTokenExpiry = now + delta;
  start_renewal_delay( delta );
}

void viessmann::start_renewal_delay( const boost::posix_time::time_duration delay ) {

  BOOST_LOG_TRIVIAL(trace) << "start_renewal_delay " << delay.total_seconds() << " seconds";

  // todo: check if timer active, shouldn't be
  m_timerTokenRefresh.expires_after( boost::asio::chrono::seconds( delay.total_seconds() ) );
  m_timerTokenRefresh.async_wait(
    [this]( const boost::system::error_code& error ){
      if ( error ) {
        BOOST_LOG_TRIVIAL(error) << "feature timer error (no continue): " << error.message();
      }
      else {
        refresh_access_token();
      }
    }
  );
}

/*
[2026-10-26 05:37:55.627561] [0x00007ffff7c47b00] [error]   get features std::exception: true,0,
{
  "viErrorId": "00-d56090f4d794f3d48146d9c7df0edbbe-c40744408ad47a1c-00",
  "statusCode": 400,
  "errorType": "DEVICE_COMMUNICATION_ERROR",
  "message": "Device communication error",
  "extendedPayload": {
    "httpStatusCode": "NotFound",
    "code": "404",
    "reason": "GATEWAY_OFFLINE",
    "message": "Not found"
  }
}



[2026-10-26 09:41:22.758152] [0x00007ffff7c47b00] [trace]   set_timer_refresh_access_token()
[2026-10-26 09:41:22.758157] [0x00007ffff7c47b00] [trace]   start_renewal_delay 3588 seconds
[2026-10-26 09:41:22.758305] [0x00007ffff7c47b00] [error]   feature timer error (no continue): Operation canceled
[2026-10-26 09:41:25.542722] [0x00007ffff7c47b00] [trace]   devices request: '/iot/v2/equipment/installations/3284872/gateways/7633301008871250/devices'
[2026-10-26 09:41:26.631240] [0x00007ffff7c47b00] [trace]   features request: '/iot/v2/features/installations/3284872/gateways/7633301008871250/devices/0/features'
[2026-10-26 09:41:28.627179] [0x00007ffff7c47b00] [info]    saving feature
[2026-10-26 09:41:47.360175] [0x00007ffff7c47b00] [info]    127.0.0.1:34650 unknown request
[2026-10-26 09:42:28.627816] [0x00007ffff7c47b00] [trace]   features request: '/iot/v2/features/installations/3284872/gateways/7633301008871250/devices/0/features'
[2026-10-26 09:42:31.243946] [0x00007ffff7c47b00] [info]    saving feature


{
  "viErrorId": "00-e3dea55fa9edcbafbc7163df8732be1f-cff128404b94efb9-00",
  "statusCode": 400,
  "errorType": "DEVICE_COMMUNICATION_ERROR",
  "message": "Device communication error",
  "extendedPayload": {
    "httpStatusCode": "NotFound",
    "code": "404",
    "reason": "GATEWAY_OFFLINE",
    "message": "Not found"
  }
}

*/

void viessmann::refresh_access_token() {

  boost::urls::url url;
  url.set_encoded_path( m_choices.sAuthPath );
  boost::urls::params_ref params( url.params() );
  params.append(
    {
      { "client_id", m_choices.sAuthClientId },
      { "grant_type", "refresh_token" },
      { "refresh_token", m_iam.sRefreshToken }
    }
  );

  BOOST_LOG_TRIVIAL(trace) << "refresh_token: " << url.path() + "token?" + url.query();

  auto request = std::make_shared<ou::rest::one_shot>( asio::make_strand( m_ioc ), ssl_context );
  request->post(
      m_choices.sAuthFQDN
    , m_choices.sAuthPort
    , [this,&url]( ou::rest::handler::pRequestStringBody_t& pRequest ){
        pRequest->target( url.path() + "token" );
        pRequest->body() = url.query();
        pRequest->prepare_payload();
        pRequest->set( http::field::content_type, "application/x-www-form-urlencoded" );
        // is this header required for iam?
        //pRequest->set( "Authorization", m_sTokenType + " " + m_sAccessToken );
      }
    , [this]( bool bStatus, int ec, const std::string& message ){
        if ( bStatus ) {

          boost::system::error_code jec;
          json::value jv = boost::json::parse( message, jec );

          m_iam = json::value_to<iam_result_t>( jv );

          set_refresh_access_token_full_duration();

          // For refresh tokens, the TTL is 15552000 seconds (180 days).
          //m_dtRefreshTokenExpiry = now + boost::gregorian::date_duration( 180 - 1 );

          // https://api.viessmann-climatesolutions.com/documentation/static/authentication
          // For refresh tokens, the TTL is 15552000 seconds (180 days).
          //BOOST_LOG_TRIVIAL(info) << "refresh token=" << m_iam.sRefreshToken;

        }
        else {
          // fail
          BOOST_LOG_TRIVIAL(error) << "refresh token status=" << bStatus << ", message=" << message;
        }
      }
    );
}

