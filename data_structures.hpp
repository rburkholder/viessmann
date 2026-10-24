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
 * File:    data_structures.hpp
 * Project: viessmann
 * Author:  raymond@burkholder.net
 * Created: October 23, 2026 17:50:36
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <variant>

#include <boost/date_time/posix_time/ptime.hpp>

struct iam_result_t {
  std::string sTokenType;
  std::string sAccessToken;
  std::string sRefreshToken;
  uint64_t nSecondsToExpiry;
};

struct geolocation_t {
  double latitude;
  double longitude;
  std::string sTimeZone;
};

struct address_t {
  std::string city;
  std::string country;
  geolocation_t geo;
};

struct installation_t {
  uint64_t id;
  std::string sDescription;
  address_t addr;
  boost::posix_time::ptime dtUpdated;
  std::string sStatus;
};

using vInstallation_t = std::vector<installation_t>;

struct gateway_t {
  std::string serial;
  std::string version;
  boost::posix_time::ptime dtStatusChanged;
  std::string sStatus;
  std::string sType;
  uint64_t idInstallation;
  bool bOtaOngoing;
};

using vGateway_t = std::vector<gateway_t>;

struct device_t {
  std::string id;
  std::string serialGateway;
  boost::posix_time::ptime dtEdited;
  std::string idModel;
  std::string sStatus;
  std::string sType;
  std::vector<std::string> vRole;
};

using vDevice_t = std::vector<device_t>;

using variant_t = std::variant<bool, int64_t, double, std::string>;

struct value_t {
  std::string sType; // convert to enum
  variant_t value;
  std::string sUnit;
};

struct status_t {
  std::string sType; // convert ot enum
  variant_t value;
};

struct property_t {
  value_t value;
  status_t status;
};

struct feature_t {
  std::string sName;
  std::string sIdDevice;
  boost::posix_time::ptime dtTimeStamp;
  bool bEnabled;
  bool bReady;
  property_t property;
};

using vFeature_t = std::vector<feature_t>;

