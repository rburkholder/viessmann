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

//#include <typeinfo>
#include <vector>

#include <boost/log/trivial.hpp>

#include <boost/asio/strand.hpp>
#include <boost/asio/ssl/context.hpp>

#include <boost/url/url.hpp>
#include <boost/url/params_ref.hpp>

#include <boost/json.hpp>

#include <ou/rest/one_shot.hpp>

#include "viessmann.hpp"

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
{

}

viessmann::~viessmann() {};

std::string viessmann::initial_url() {

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

void viessmann::iam( const std::string_view svCode ) {
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
        if ( bStatus ) {
          json::value jv = boost::json::parse( message );
          json::object const& obj = jv.as_object();

          m_sTokenType = json::value_to<std::string>( obj.at( "token_type" ) );
          m_sAccessToken = json::value_to<std::string>( obj.at( "access_token" ) );
          m_sRefreshToken = json::value_to<std::string>( obj.at( "refresh_token" ) );
          m_nExpires = json::value_to<uint32_t>( obj.at( "expires_in" ) );

          // https://api.viessmann-climatesolutions.com/documentation/static/authentication
          // For refresh tokens, the TTL is 15552000 seconds (180 days).
          BOOST_LOG_TRIVIAL(info) << "step 2 refresh=" << m_sRefreshToken;

          get_installations();
        }
        else {
          // fail
          BOOST_LOG_TRIVIAL(info) << "step 2 status=" << bStatus << ", message=" << message;
        }
      }
    );
}

// https://api.viessmann-climatesolutions.com/documentation/static/iot-overview
void viessmann::get_installations() {
  auto request = std::make_shared<ou::rest::one_shot>( asio::make_strand( m_ioc ), ssl_context );
  request->get(
    m_choices.sApiFQDN
  , m_choices.sApiPort
  , [this]( ou::rest::handler::pRequestEmptyBody_t& pRequest ){
      //pRequest->target( "/iot/v2/equipment/gateways" );
      pRequest->target( "/iot/v2/equipment/installations" );
      pRequest->set( "Authorization", m_sTokenType + " " + m_sAccessToken );
    }
  , [this]( bool bStatus, int ec, const std::string& message ){
      BOOST_LOG_TRIVIAL(trace) << "step 3: " << bStatus << ',' << ec << ',' << message;
      json::value jv = boost::json::parse( message );
      json::object const& obj = jv.as_object();
      using vObject_t = std::vector<json::object>;
      vObject_t vData;
      vData = json::value_to<vObject_t>( obj.at( "data" ) );
//      BOOST_LOG_TRIVIAL(debug) << "data is type: " << vData.size() << ',' << typeid( vData ).name();
      assert( 1 <= vData.size() );
      json::object site = vData[ 0 ];
      m_nSiteId = json::value_to<uint32_t>( site.at( "id" ) );
      m_sStatus = json::value_to<std::string>( site.at( "aggregatedStatus") );
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
      { "refresh_token", m_sRefreshToken }
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

          boost::json::value jv = boost::json::parse( message );
          boost::json::object const& obj = jv.as_object();

          // how many of these are present?
          m_sTokenType = boost::json::value_to<std::string>( obj.at( "token_type" ) );
          m_sAccessToken = boost::json::value_to<std::string>( obj.at( "access_token" ) );
          m_sRefreshToken = boost::json::value_to<std::string>( obj.at( "refresh_token" ) );
          m_nExpires = boost::json::value_to<uint32_t>( obj.at( "expires_in" ) );

          // https://api.viessmann-climatesolutions.com/documentation/static/authentication
          // For refresh tokens, the TTL is 15552000 seconds (180 days).
          BOOST_LOG_TRIVIAL(info) << "token refresh=" << m_sRefreshToken;

        }
        else {
          // fail
          BOOST_LOG_TRIVIAL(info) << "token refresh status=" << bStatus << ", message=" << message;
        }
      }
    );
}

