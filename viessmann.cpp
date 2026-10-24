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
//   acquire features in loop of 1 minute
//   dynamically update gui with websocket

//#include <typeinfo>
#include <vector>
#include <fstream>
#include <exception>

#include <boost/log/trivial.hpp>

#include <boost/asio/strand.hpp>
#include <boost/asio/ssl/context.hpp>

#include <boost/url/url.hpp>
#include <boost/url/params_ref.hpp>

#include <boost/date_time/gregorian/greg_duration.hpp>

#include <boost/json.hpp>

#include <fmt/format.h>

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
, m_fSave( nullptr )
, m_dtAccessTokenExpiry( boost::date_time::not_a_date_time )
, m_dtRefreshTokenExpiry( boost::date_time::not_a_date_time )
{

}

viessmann::~viessmann() {};

void viessmann::set( fSave_t && f ) {
  m_fSave = std::move( f );
}

bool viessmann::authz_required() const {
  if ( boost::date_time::not_a_date_time == m_dtAccessTokenExpiry ) {
    return true;
  }
  const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
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

          m_iam = json::value_to<iam_result_t>( jv );

          // https://api.viessmann-climatesolutions.com/documentation/static/authentication
          const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
          m_dtAccessTokenExpiry = now + boost::posix_time::time_duration( 0, 0, m_iam.nSecondsToExpiry - 10 );

          // For refresh tokens, the TTL is 15552000 seconds (180 days).
          m_dtRefreshTokenExpiry = now + boost::gregorian::date_duration( 180 - 1 );

          //BOOST_LOG_TRIVIAL(info) << "refresh token=" << m_sRefreshToken;

          get_installations();
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
    get_installations();
  }
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
          //BOOST_LOG_TRIVIAL(trace) << "step 3: " << bStatus << ',' << ec << ',' << message;
          boost::system::error_code jec;
          json::value jv = boost::json::parse( message, jec );
          json::object const& obj = jv.as_object();
          using vObject_t = std::vector<json::object>;
          vObject_t vData;
          vData = json::value_to<vObject_t>( obj.at( "data" ) );
          assert( 1 <= vData.size() );
          json::object site = vData[ 0 ];
          m_nSiteId = json::value_to<uint64_t>( site.at( "id" ) );
          m_sSiteStatus = json::value_to<std::string>( site.at( "aggregatedStatus") );
          BOOST_LOG_TRIVIAL(info) << "site " << m_nSiteId << ": " << m_sSiteStatus;

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
        using vObject_t = std::vector<json::object>;
        vObject_t vData;
        vData = json::value_to<vObject_t>( obj.at( "data" ) );
        assert( 1 <= vData.size() );
        json::object site = vData[ 0 ];
        m_nGatewaySiteId = json::value_to<uint64_t>( site.at( "installationId" ) );
        m_sGatewaySerial = json::value_to<std::string>( site.at( "serial") );
        m_sGatewayLastStatusChanged = json::value_to<std::string>( site.at( "lastStatusChangedAt") );
        m_sGatewayStatus = json::value_to<std::string>( site.at( "aggregatedStatus") );
        BOOST_LOG_TRIVIAL(info) << "gateway '" << m_sGatewaySerial << "': " << m_sGatewayStatus << ", changed:" << m_sGatewayLastStatusChanged;
        assert( m_nGatewaySiteId == m_nSiteId );

        save();

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
      const std::string target( fmt::format( "/iot/v2/equipment/installations/{}/gateways/{}/devices", m_nSiteId, m_sGatewaySerial ) );
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
        using vObject_t = std::vector<json::object>;
        vObject_t vData;

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
      const std::string target( fmt::format( "/iot/v2/features/installations/{}/gateways/{}/devices/{}/features", m_nSiteId, m_sGatewaySerial, "0" ) );
      BOOST_LOG_TRIVIAL(trace) << "features request: '" << target << "'";
      pRequest->target( target );
      pRequest->set( "Authorization", m_iam.sTokenType + " " + m_iam.sAccessToken );
    }
  , [this]( bool bStatus, int ec, const std::string& message ){
      if ( bStatus ) {
        //BOOST_LOG_TRIVIAL(trace) << "get features: " << bStatus << ',' << ec << ',' << message;
        boost::system::error_code jec;
        json::value jv = boost::json::parse( message, jec );
        json::object const& obj = jv.as_object();
        using vObject_t = std::vector<json::object>;
        vObject_t vData;

        {
          BOOST_LOG_TRIVIAL(info) << "saving feature";
          std::ofstream ofs( "viessmann.feature" );
          ofs << message;
        }
      }
      else {
        BOOST_LOG_TRIVIAL(error) << "get features error: " << bStatus << ',' << ec << ',' << message;
      }
    }
  );
}

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

          // https://api.viessmann-climatesolutions.com/documentation/static/authentication
          const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
          m_dtAccessTokenExpiry = now + boost::posix_time::time_duration( 0, 0, m_iam.nSecondsToExpiry - 10 );

          // For refresh tokens, the TTL is 15552000 seconds (180 days).
          m_dtRefreshTokenExpiry = now + boost::gregorian::date_duration( 180 - 1 );

          // https://api.viessmann-climatesolutions.com/documentation/static/authentication
          // For refresh tokens, the TTL is 15552000 seconds (180 days).
          BOOST_LOG_TRIVIAL(info) << "token refresh=" << m_iam.sRefreshToken;

        }
        else {
          // fail
          BOOST_LOG_TRIVIAL(error) << "token refresh status=" << bStatus << ", message=" << message;
        }
      }
    );
}

