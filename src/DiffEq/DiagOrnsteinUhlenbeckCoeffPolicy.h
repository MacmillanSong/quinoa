// *****************************************************************************
/*!
  \file      src/DiffEq/DiagOrnsteinUhlenbeckCoeffPolicy.h
  \copyright 2012-2015 J. Bakosi,
             2016-2018 Los Alamos National Security, LLC.,
             2019 Triad National Security, LLC.
             All rights reserved. See the LICENSE file for details.
  \brief     Diagonal Ornstein-Uhlenbeck coefficients policies
  \details   This file defines coefficients policy classes for the diagonal
    Ornstein-Uhlenbeck SDE, defined in DiffEq/DiagOrnsteinUhlenbeck.h.

    General requirements on the diagonal Ornstein-Uhlenbeck SDE coefficients
    policy classes:

    - Must define a _constructor_, which is used to initialize the SDE
      coefficients, sigmasq, theta, and mu. Required signature:
      \code{.cpp}
        CoeffPolicyName(
          tk::ctr::ncomp_type ncomp,
          const std::vector< kw::sde_sigmasq::info::expect::type >& sigmasq_,
          const std::vector< kw::sde_theta::info::expect::type >& theta_,
          const std::vector< kw::sde_mu::info::expect::type >& mu_,
          std::vector< kw::sde_sigmasq::info::expect::type >& sigmasq,
          std::vector< kw::sde_theta::info::expect::type >& theta,
          std::vector< kw::sde_mu::info::expect::type >& mu )
      \endcode
      where
      - ncomp denotes the number of scalar components of the system of the
        diagonal Ornstein-Uhlenbeck SDEs.
      - Constant references to sigmasq_, theta_, and mu_, which denote three
        vectors of real values used to initialize the parameter vectors of the
        system of diagonal Ornstein-Uhlenbeck SDEs. The length of the vectors
        must be equal to the number of components given by ncomp.
      - References to sigmasq, theta, and mu, which denote the parameter vectors
        to be initialized based on sigmasq_, theta_, and mu_.

    - Must define the static function _type()_, returning the enum value of the
      policy option. Example:
      \code{.cpp}
        static ctr::CoeffPolicyType type() noexcept {
          return ctr::CoeffPolicyType::CONST_COEFF;
        }
      \endcode
      which returns the enum value of the option from the underlying option
      class, collecting all possible options for coefficients policies.
*/
// *****************************************************************************
#ifndef DiagOrnsteinUhlenbeckCoeffPolicy_h
#define DiagOrnsteinUhlenbeckCoeffPolicy_h

#include <brigand/sequences/list.hpp>

#include "Types.h"
#include "Walker/Options/CoeffPolicy.h"
#include "SystemComponents.h"

namespace walker {

//! Diagonal Ornstein-Uhlenbeck constant coefficients policity: constants in time
class DiagOrnsteinUhlenbeckCoeffConst {

  public:
    //! Constructor: initialize coefficients
    DiagOrnsteinUhlenbeckCoeffConst(
      tk::ctr::ncomp_type ncomp,
      const std::vector< kw::sde_sigmasq::info::expect::type >& sigmasq_,
      const std::vector< kw::sde_theta::info::expect::type >& theta_,
      const std::vector< kw::sde_mu::info::expect::type >& mu_,
      std::vector< kw::sde_sigmasq::info::expect::type >& sigmasq,
      std::vector< kw::sde_theta::info::expect::type >& theta,
      std::vector< kw::sde_mu::info::expect::type >& mu )
    {
      ErrChk( sigmasq_.size() == ncomp,
       "Wrong number of diagonal Ornstein-Uhlenbeck SDE parameters 'sigmasq'");
      ErrChk( theta_.size() == ncomp,
       "Wrong number of diagonal Ornstein_uhlenbeck SDE parameters 'theta'");
      ErrChk( mu_.size() == ncomp,
       "Wrong number of diagonal Ornstein_uhlenbeck SDE parameters 'mu'");

      sigmasq = sigmasq_;
      theta = theta_;
      mu = mu_;
    }

    //! Coefficients policy type accessor
    static ctr::CoeffPolicyType type() noexcept
    { return ctr::CoeffPolicyType::CONST_COEFF; }
};

//! List of all Ornstein-Uhlenbeck's coefficients policies
using DiagOrnsteinUhlenbeckCoeffPolicies =
  brigand::list< DiagOrnsteinUhlenbeckCoeffConst >;

} // walker::

#endif // DiagOrnsteinUhlenbeckCoeffPolicy_h
