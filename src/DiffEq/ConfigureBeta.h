// *****************************************************************************
/*!
  \file      src/DiffEq/ConfigureBeta.h
  \copyright 2012-2015 J. Bakosi,
             2016-2018 Los Alamos National Security, LLC.,
             2019 Triad National Security, LLC.
             All rights reserved. See the LICENSE file for details.
  \brief     Register and compile configuration on the beta SDE
  \details   Register and compile configuration on the beta SDE.
*/
// *****************************************************************************
#ifndef ConfigureBeta_h
#define ConfigureBeta_h

#include <set>

#include "DiffEqFactory.h"
#include "Walker/Options/DiffEq.h"

namespace walker {

//! Register beta SDE into DiffEq factory
void registerBeta( DiffEqFactory& f, std::set< ctr::DiffEqType >& t );

//! Return information on the beta SDE
std::vector< std::pair< std::string, std::string > >
infoBeta( std::map< ctr::DiffEqType, tk::ctr::ncomp_type >& cnt );

} // walker::

#endif // ConfigureBeta_h
