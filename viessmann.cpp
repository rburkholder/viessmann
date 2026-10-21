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
      { "redirect_uri", m_choices.sUrlRedirect },
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
      { "redirect_uri", m_choices.sUrlRedirect },
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
          boost::json::value jv = boost::json::parse( message );
          boost::json::object const& obj = jv.as_object();

          m_sTokenType = boost::json::value_to<std::string>( obj.at( "token_type" ) );
          m_sAccessToken = boost::json::value_to<std::string>( obj.at( "access_token" ) );
          m_sRefreshToken = boost::json::value_to<std::string>( obj.at( "refresh_token" ) );
          m_nExpires = boost::json::value_to<uint32_t>( obj.at( "expires_in" ) );

          api();
        }
        else {
          // fail
          BOOST_LOG_TRIVIAL(info) << "step 2 status=" << bStatus << ", message=" << message;
        }
      }
    );
}

void viessmann::api() {
  auto request = std::make_shared<ou::rest::one_shot>( asio::make_strand( m_ioc ), ssl_context );
  request->get(
    "api.viessmann-climatesolutions.com"
  , m_choices.sAuthPort
  , [this]( ou::rest::handler::pRequestEmptyBody_t& pRequest ){
      //pRequest->target( "/iot/v2/equipment/gateways" );
      pRequest->target( "/iot/v2/equipment/installations" );
      pRequest->set( "Authorization", m_sTokenType + " " + m_sAccessToken );
    }
  , []( bool bStatus, int ec, const std::string& message ){
      BOOST_LOG_TRIVIAL(trace) << "step 3: " << bStatus << ',' << ec << ',' << message;
    }
  );
}
