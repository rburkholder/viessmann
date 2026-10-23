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
 * File:    viessmann.hpp
 * Project: viessmann
 * Author:  raymond@burkholder.net
 * Created: October 20, 2026 19:11:59
 */

#pragma once

#include <string>

#include <boost/asio/io_context.hpp>

#include <boost/serialization/version.hpp>
#include <boost/serialization/split_member.hpp>

#include <boost/date_time/posix_time/time_parsers.hpp>
#include <boost/date_time/posix_time/time_formatters.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "pkce.hpp"
#include "config.hpp"

namespace asio = boost::asio; // from <boost/asio/context.hpp>

class viessmann {
  friend class boost::serialization::access;
public:

  viessmann( const config::Values&, asio::io_context& ioc );
  ~viessmann();

  std::string authz_url();
  void authz( const std::string_view /* code */ );
  bool authz_required() const;

  using fSave_t = std::function<void()>;
  void set( fSave_t&& );

  void start();

protected:
private:

  const config::Values& m_choices;
  asio::io_context& m_ioc;

  PkcePair m_pkce;

  std::string m_sTokenType;
  std::string m_sAccessToken;
  std::string m_sRefreshToken;
  uint32_t m_nExpires;

  uint32_t m_nSiteId;
  std::string m_sSiteStatus;

  std::string m_sGatewaySerial;
  std::string m_sGatewayLastStatusChanged;
  std::string m_sGatewayStatus;
  uint32_t m_nGatewaySiteId;

  boost::posix_time::ptime m_dtAccessTokenExpiry;
  boost::posix_time::ptime m_dtRefreshTokenExpiry;

  void refresh_access_token();
  void get_installations();
  void get_gateways();
  void get_devices();
  void get_features();

  fSave_t m_fSave;
  void save();

  template<typename Archive>
  void save( Archive& ar, const unsigned int version ) const {

    ar & m_sTokenType;
    ar & m_sAccessToken;
    ar & m_sRefreshToken;
    ar & m_nSiteId;
    ar & m_sGatewaySerial;

    std::string dtAccessTokenExpiry( boost::posix_time::to_iso_string( m_dtAccessTokenExpiry ) );
    ar & dtAccessTokenExpiry;
    std::string dtRefreshTokenExpiry( boost::posix_time::to_iso_string( m_dtRefreshTokenExpiry ) );
    ar & dtRefreshTokenExpiry;

  }

  template<typename Archive>
  void load( Archive& ar, const unsigned int version ) {

    ar & m_sTokenType;
    ar & m_sAccessToken;
    ar & m_sRefreshToken;
    ar & m_nSiteId;
    ar & m_sGatewaySerial;

    std::string dtAccessTokenExpiry;
    ar & dtAccessTokenExpiry;
    m_dtAccessTokenExpiry = boost::posix_time::from_iso_string( dtAccessTokenExpiry );

    std::string dtRefreshTokenExpiry;
    ar & dtRefreshTokenExpiry;
    m_dtRefreshTokenExpiry = boost::posix_time::from_iso_string( dtRefreshTokenExpiry );
  }

  BOOST_SERIALIZATION_SPLIT_MEMBER()
};

BOOST_CLASS_VERSION(viessmann, 1)
