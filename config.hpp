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
 * File:    config.hpp
 * Author:  raymond@burkholder.net
 * Project: viessmann
 * Created: 2026/10/15 04:34:11
 */

#pragma once

#include <cstdint>

#include <string>

#include <ou/mqtt/config.hpp>

namespace config {

struct Values {

  std::string sContentDirectory;

  uint16_t nListenPort;
  std::string sListenAddress;

  std::string sAuthFQDN;
  std::string sAuthPort;
  std::string sAuthPath;
  std::string sAuthClientId;

  std::string sAuthRedirect;

  std::string sApiFQDN;
  std::string sApiPort;

  ou::mqtt::Config mqtt;

};

bool Load( const std::string& sFileName, Values& );

} // namespace config