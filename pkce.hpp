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
 * File:    pkce.hpp
 * Author:  raymond@burkholder.net
 * Project: viessmann
 * Created: October 17, 2026 15:36:47
 */

#include <string>

struct PkcePair {
  std::string code_verifier;
  std::string code_challenge;
};

PkcePair generate_pkce();
