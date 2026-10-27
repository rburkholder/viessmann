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
 * File:    data_structures.cpp
 * Project: viessmann
 * Author:  raymond@burkholder.net
 * Created: October 23, 2026 20:26:22
 */

#include <boost/parser/parser.hpp>

#include <boost/date_time/posix_time/time_parsers.hpp>

#include "convert_json.hpp"
#include "data_structures.hpp"

//
namespace {

  char const* no_z( json::string js ) {
    js.pop_back();
    return js.c_str();
  }

  boost::parser::symbols<value_type_t> const lu_type = {
    { "boolean", value_type_t::boolean },
    { "number", value_type_t::number },
    { "string", value_type_t::string },
    { "array", value_type_t::array },
    { "Schedule", value_type_t::Schedule },
    { "VinEntry", value_type_t::VinEntry },
    { "MaterialIdEntry", value_type_t::MaterialIdEntry },
    // dbl_ not in source material
    // int_ not in source material
  };

  boost::parser::symbols<bool> const lu_bool = {
    { "true", true },
    { "false", false }
  };

}

iam_result_t tag_invoke( json::value_to_tag<iam_result_t>, json::value const& jv ) {
  json::object const& obj = jv.as_object();
  return iam_result_t {
    json::value_to<std::string>( obj.at( "token_type" ) ),
    json::value_to<std::string>( obj.at( "access_token" ) ),
    json::value_to<std::string>( obj.at( "refresh_token" ) ),
    json::value_to<uint64_t>( obj.at( "expires_in" ) ),
  };
}

geolocation_t  tag_invoke( json::value_to_tag<geolocation_t>, json::value const& jv ) {
  json::object const& obj = jv.as_object();
  return geolocation_t {
    json::value_to<double>( obj.at( "latitude" ) ),
    json::value_to<double>( obj.at( "longitude" ) ),
    json::value_to<std::string>( obj.at( "timeZone" ) ),
  };
}

address_t      tag_invoke( json::value_to_tag<address_t>, json::value const& jv ) {
  json::object const& obj = jv.as_object();
  return address_t {
    json::value_to<std::string>( obj.at( "city" ) ),
    json::value_to<std::string>( obj.at( "country" ) ),
    json::value_to<geolocation_t>( obj.at( "geolocation" ) ),
  };
}

installation_t tag_invoke( json::value_to_tag<installation_t>, json::value const& jv ) {
  json::object const& obj = jv.as_object();
  const boost::posix_time::ptime dt =
    boost::posix_time::from_iso_extended_string(
      no_z( json::value_to<json::string>( obj.at( "updatedAt" ) ) )
    );
  return installation_t {
    json::value_to<uint64_t>( obj.at( "id" ) ),
    json::value_to<std::string>( obj.at( "description" ) ),
    json::value_to<address_t>( obj.at( "address" ) ),
    dt,
    json::value_to<std::string>( obj.at( "aggregatedStatus" ) ),
  };
}

gateway_t      tag_invoke( json::value_to_tag<gateway_t>, json::value const& jv ) {
  json::object const& obj = jv.as_object();
  const boost::posix_time::ptime dt =
    boost::posix_time::from_iso_extended_string(
      no_z( json::value_to<json::string>( obj.at( "lastStatusChangedAt" ) ) )
    );
  return gateway_t {
    json::value_to<std::string>( obj.at( "serial" ) ),
    json::value_to<std::string>( obj.at( "version" ) ),
    dt,
    json::value_to<std::string>( obj.at( "aggregatedStatus" ) ),
    json::value_to<std::string>( obj.at( "gatewayType" ) ),
    json::value_to<uint64_t>( obj.at( "installationId" ) ),
    json::value_to<bool>( obj.at( "otaOngoing" ) ),
  };
}

device_t       tag_invoke( json::value_to_tag<device_t>, json::value const& jv ) {
  json::object const& obj = jv.as_object();
  const boost::posix_time::ptime dt =
    boost::posix_time::from_iso_extended_string(
      no_z( json::value_to<json::string>( obj.at( "editedAt" ) ) )
    );
  return device_t {
    json::value_to<std::string>( obj.at( "id" ) ),
    json::value_to<std::string>( obj.at( "gatewaySerial" ) ),
    dt,
    json::value_to<std::string>( obj.at( "modelId" ) ),
    json::value_to<std::string>( obj.at( "status" ) ),
    json::value_to<std::string>( obj.at( "deviceType" ) ),
    json::value_to<std::vector<std::string>>( obj.at( "roles" ) ),
  };
}
value_t        tag_invoke( json::value_to_tag<value_t>, json::value const& jv ) {

  json::object const& obj = jv.as_object();

  std::string sUnit;
  if ( obj.contains( "unit" ) ) {
    sUnit = json::value_to<std::string>( obj.at( "unit" ) );
  }

  const std::string sType( json::value_to<std::string_view>( obj.at( "type" ) ) );

  auto const vt_parser = lu_type >> boost::parser::eoi;
  auto const result = boost::parser::parse( sType, vt_parser );
  assert( result );
  const value_type_t vt( *result );

  value_t value;

  switch ( vt ) {
    case value_type_t::boolean:
      {
        bool b( json::value_to<bool>( obj.at( "value" ) ) );
        if ( false ) {
          const std::string sValue( json::value_to<std::string>( obj.at( "value" ) ) );
          auto const bool_parser = lu_bool >> boost::parser::eoi;
          auto const result = boost::parser::parse( sValue, bool_parser );
          assert( result );
          const bool b( *result );
        }
        value = value_t { vt, b, sUnit };
      }
      break;
    case value_type_t::number:
      {
        //const std::string sValue( json::value_to<std::string>( obj.at( "value" ) ) );
        try {
          int64_t num( json::value_to<int64_t>( obj.at( "value" ) ) );
          value = value_t { value_type_t::int_, num, sUnit };
        }
        catch (...) {
          double num( json::value_to<double>( obj.at( "value" ) ) );
          value = value_t { value_type_t::dbl_, num, sUnit };
        }
      }
      break;
    case value_type_t::string:
      {
        const std::string sValue( json::value_to<std::string>( obj.at( "value" ) ) );
        value = value_t {
          vt, std::move( sValue ), sUnit
        };
      }
      break;
    case value_type_t::array:
      //break;
    default:
      value = value_t {
        vt, 0, sUnit
      };
      break;
  }

  return value;
}

status_t       tag_invoke( json::value_to_tag<status_t>, json::value const& jv ) {
  json::object const& obj = jv.as_object();
  status_t status {
    json::value_to<std::string>( obj.at( "type" ) ),
    0
  };
  if ( obj.contains( "value" ) ) {
    if ( "string" == status.sType ) {
      status.value = json::value_to<std::string>( obj.at( "value" ) );
    }
    else {
      status.value = "unknown";
    }

  }
  return status;
}

property_t     tag_invoke( json::value_to_tag<property_t>, json::value const& jv ) {
  json::object const& obj = jv.as_object();
  property_t property;
  if ( obj.contains( "value" ) ) {
    property.value = json::value_to<value_t>( obj.at( "value" ) );
  } // maybe do explict else
  if ( obj.contains( "status" ) ) {
    property.status = json::value_to<status_t>( obj.at( "status" ) );
  } // maybe do explict else
  return property;
}

feature_t      tag_invoke( json::value_to_tag<feature_t>, json::value const& jv ) {
  json::object const& obj = jv.as_object();
  const boost::posix_time::ptime dt =
    boost::posix_time::from_iso_extended_string(
      no_z( json::value_to<json::string>( obj.at( "timestamp" ) ) )
    );
  return feature_t {
    json::value_to<std::string>( obj.at( "feature" ) ),
    json::value_to<std::string>( obj.at( "deviceId" ) ),
    dt,
    json::value_to<bool>( obj.at( "isEnabled" ) ),
    json::value_to<bool>( obj.at( "isReady" ) ),
    json::value_to<property_t>( obj.at( "properties" ) ),
  };
}

extended_payload_t tag_invoke( json::value_to_tag<extended_payload_t>, json::value const& jv ) {
  json::object const& obj = jv.as_object();
  return extended_payload_t {
    json::value_to<std::string>( obj.at( "httpStatusCode" ) ),
    json::value_to<std::string>( obj.at( "code" ) ),
    json::value_to<std::string>( obj.at( "reason" ) ),
    json::value_to<std::string>( obj.at( "message" ) )
  };
}

viessmann_error_t tag_invoke( json::value_to_tag<viessmann_error_t>, json::value const& jv ) {
  json::object const& obj = jv.as_object();
  return viessmann_error_t {
    json::value_to<std::string>( obj.at( "viErrorId" ) ),
    json::value_to<uint64_t>( obj.at( "statusCode" ) ),
    json::value_to<std::string>( obj.at( "errorType" ) ),
    json::value_to<std::string>( obj.at( "message" ) ),
    json::value_to<extended_payload_t>( obj.at( "extendedPayload" ) )
  };
}
