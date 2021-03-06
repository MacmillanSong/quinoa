// *****************************************************************************
/*!
  \file      src/DiffEq/ConfigureVelocity.C
  \copyright 2012-2015 J. Bakosi,
             2016-2018 Los Alamos National Security, LLC.,
             2019 Triad National Security, LLC.
             All rights reserved. See the LICENSE file for details.
  \brief     Register and compile configuration on the velocity SDE
  \details   Register and compile configuration on the velocity SDE.
*/
// *****************************************************************************

#include <set>
#include <map>
#include <vector>
#include <string>
#include <utility>

#include <brigand/algorithms/for_each.hpp>

#include "Tags.h"
#include "CartesianProduct.h"
#include "DiffEqFactory.h"
#include "Walker/Options/DiffEq.h"
#include "Walker/Options/InitPolicy.h"

#include "ConfigureVelocity.h"
#include "Velocity.h"
#include "VelocityCoeffPolicy.h"

#include "CoupledEq.h"

namespace walker {

void
registerVelocity( DiffEqFactory& f, std::set< ctr::DiffEqType >& t )
// *****************************************************************************
// Register velocity SDE into DiffEq factory
//! \param[in,out] f Differential equation factory to register to
//! \param[in,out] t Counters for equation types registered
// *****************************************************************************
{
  // Construct vector of vectors for all possible policies for SDE
  using VelocityPolicies =
    tk::cartesian_product< InitPolicies, VelocityCoeffPolicies >;
  // Register SDE for all combinations of policies
  brigand::for_each< VelocityPolicies >(
    registerDiffEq< Velocity >( f, ctr::DiffEqType::VELOCITY, t ) );
}

std::vector< std::pair< std::string, std::string > >
infoVelocity( std::map< ctr::DiffEqType, tk::ctr::ncomp_type >& cnt )
// *****************************************************************************
//  Return information on the velocity SDE
//! \param[inout] cnt std::map of counters for all differential equation types
//! \return vector of string pairs describing the SDE configuration
// *****************************************************************************
{
  using eq = tag::velocity;

  auto c = ++cnt[ ctr::DiffEqType::VELOCITY ];       // count eqs
  --c;  // used to index vectors starting with 0

  std::vector< std::pair< std::string, std::string > > nfo;

  nfo.emplace_back( ctr::DiffEq().name( ctr::DiffEqType::VELOCITY ), "" );

  nfo.emplace_back( "start offset in particle array", std::to_string(
    g_inputdeck.get< tag::component >().offset< eq >(c) ) );
  auto ncomp = g_inputdeck.get< tag::component, eq >()[c];
  nfo.emplace_back( "number of components", std::to_string( ncomp ) );

  coupledInfo< eq, tag::position, tag::position_id >
             ( c, "position", nfo );
  coupledInfo< eq, tag::dissipation, tag::dissipation_id >
             ( c, "dissipation", nfo );
  coupledInfo< eq, tag::mixmassfracbeta, tag::mixmassfracbeta_id >
             ( c, "mixmassfracbeta", nfo );

  nfo.emplace_back( "kind", "stochastic" );
  nfo.emplace_back( "dependent variable", std::string( 1,
    g_inputdeck.get< tag::param, eq, tag::depvar >()[c] ) );
  nfo.emplace_back( "initialization policy", ctr::InitPolicy().name(
    g_inputdeck.get< tag::param, eq, tag::initpolicy >()[c] ) );
  auto cp = g_inputdeck.get< tag::param, eq, tag::coeffpolicy >()[c];
  nfo.emplace_back( "coefficients policy", ctr::CoeffPolicy().name( cp ) );

  auto solve = g_inputdeck.get< tag::param, eq, tag::solve >()[c];
  auto dv = ctr::Depvar();
  nfo.emplace_back( dv.group(), dv.name(solve) );

  auto variant = g_inputdeck.get< tag::param, eq, tag::variant >()[c];
  auto velocity = ctr::VelocityVariant();
  nfo.emplace_back( velocity.group(), velocity.name(variant) );

  if (cp == ctr::CoeffPolicyType::HYDROTIMESCALE) {
    nfo.emplace_back(
      "inverse hydro time scale",
      options( ctr::HydroTimeScales(),
               g_inputdeck.get< tag::param,
                                eq,
                                tag::hydrotimescales >().at(c) ) );
    nfo.emplace_back(
      "production/dissipation",
      options( ctr::HydroProductions(),
               g_inputdeck.get< tag::param,
                                eq,
                                tag::hydroproductions >().at(c) ) );
  }

  nfo.emplace_back( "random number generator", tk::ctr::RNG().name(
    g_inputdeck.get< tag::param, eq, tag::rng >()[c] ) );
  nfo.emplace_back( "coeff C0", std::to_string(
    g_inputdeck.get< tag::param, eq, tag::c0 >().at(c) ) );

  return nfo;
}

}  // walker::
