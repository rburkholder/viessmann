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
 * Created: October 23, 2026 20:26:22
 */

#pragma once

#include <boost/json.hpp>

#include "data_structures.hpp"

namespace json = boost::json;

iam_result_t   tag_invoke( json::value_to_tag<iam_result_t>, json::value const& jv );
geolocation_t  tag_invoke( json::value_to_tag<geolocation_t>, json::value const& jv );
address_t      tag_invoke( json::value_to_tag<address_t>, json::value const& jv );
installation_t tag_invoke( json::value_to_tag<installation_t>, json::value const& jv );
gateway_t      tag_invoke( json::value_to_tag<gateway_t>, json::value const& jv );
device_t       tag_invoke( json::value_to_tag<device_t>, json::value const& jv );
value_t        tag_invoke( json::value_to_tag<value_t>, json::value const& jv );
status_t       tag_invoke( json::value_to_tag<status_t>, json::value const& jv );
property_t     tag_invoke( json::value_to_tag<property_t>, json::value const& jv );
feature_t      tag_invoke( json::value_to_tag<feature_t>, json::value const& jv );
