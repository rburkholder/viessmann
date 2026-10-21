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

#include "pkce.hpp"
#include "config.hpp"

namespace asio = boost::asio; // from <boost/asio/context.hpp>

class viessmann {
public:

  viessmann( const config::Values&, asio::io_context& ioc );
  ~viessmann();

  std::string initial_url();
  void iam( const std::string_view );
  void api();

protected:
private:
  const config::Values& m_choices;
  PkcePair m_pkce;
  asio::io_context& m_ioc;
};
