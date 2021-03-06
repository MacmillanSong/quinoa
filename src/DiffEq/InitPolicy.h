// *****************************************************************************
/*!
  \file      src/DiffEq/InitPolicy.h
  \copyright 2012-2015 J. Bakosi,
             2016-2018 Los Alamos National Security, LLC.,
             2019 Triad National Security, LLC.
             All rights reserved. See the LICENSE file for details.
  \brief     Initialization policies
  \details   This file defines initialization policy classes. As opposed to
    coefficients policies, see, e.g., DiffEq/BetaCoeffPolicy.h, initialization
    policies are not SDE-specific -- at least at this time.

    General requirements on initialization policy classes:

    - Must define the member function _init_, which is used to do the
      initialization. Required signature:
      \code{.cpp}
        template< class eq >
        static void init( const ctr::InputDeck& deck,
                          const tk::RNG& rng,
                          int stream,
                          tk::Particles& particles,
                          tk::ctr::ncomp_type e,
                          tk::ctr::ncomp_type ncomp,
                          tk::ctr::ncomp_type offset );
      \endcode
      where _deck_ is the input deck from which configuration is read, _rng_ is
      a reference to a random number generator to use, _stream_ is the thread
      (or stream) id, _particles_ denotes the particle properties array to be
      initialized, _e_ is the component index selecting which equation is to be
      initialized in the system, _ncomp_ is the total number of equations in the
      system, and _offset_ is the offset in the particle array at which
      initialization should be done.

    - Must define the static function _type()_, returning the enum value of the
      policy option. Example:
      \code{.cpp}
        static ctr::InitPolicyType type() noexcept {
          return ctr::InitPolicyType::RAW;
        }
      \endcode
      which returns the enum value of the option from the underlying option
      class, collecting all possible options for initialization policies.
*/
// *****************************************************************************
#ifndef InitPolicy_h
#define InitPolicy_h

#include <algorithm>

#include <brigand/sequences/list.hpp>

#ifdef HAS_MKL
  #include <mkl_lapacke.h>
#else
  #include <lapacke.h>
#endif

#include "Macro.h"
#include "Types.h"
#include "Particles.h"
#include "Walker/Options/InitPolicy.h"
#include "Walker/InputDeck/InputDeck.h"
#include "RNG.h"

namespace walker {

//! Raw initialization policy: leave memory uninitialized
struct InitRaw {

  //! Initialize particle properties
  template< class eq >
  static void init( const ctr::InputDeck&,
                    const tk::RNG&,
                    int,
                    tk::Particles&,
                    tk::ctr::ncomp_type,
                    tk::ctr::ncomp_type,
                    tk::ctr::ncomp_type ) {}

  static ctr::InitPolicyType type() noexcept
  { return ctr::InitPolicyType::RAW; }
};

//! Zero initialization policy: zero particle properties
struct InitZero {

  //! Initialize particle properties
  template< class eq >
  static void init( const ctr::InputDeck&,
                    const tk::RNG&,
                    int,
                    tk::Particles& particles,
                    tk::ctr::ncomp_type,
                    tk::ctr::ncomp_type,
                    tk::ctr::ncomp_type )
  {
    particles.fill( 0.0 );
  }

  static ctr::InitPolicyType type() noexcept
  { return ctr::InitPolicyType::ZERO; }
};

//! Delta initialization policy: put in delta-spikes as the joint PDF
struct InitDelta {

  //! Initialize particle properties
  template< class eq >
  static void init( const ctr::InputDeck& deck,
                    const tk::RNG&,
                    int,
                    tk::Particles& particles,
                    tk::ctr::ncomp_type e,
                    tk::ctr::ncomp_type ncomp,
                    tk::ctr::ncomp_type offset )
  {
    using ncomp_t = kw::ncomp::info::expect::type;

    const auto& spike = deck.template get< tag::param, eq, tag::spike >().at(e);

    // use only the first ncomp spikes if there are more than the equation is
    // configured for
    const ncomp_t size = std::min( ncomp, spike.size() );

    for (ncomp_t c=0; c<size; ++c) {
      const auto& sc = spike[c];        // vector of spikes for component c

      ncomp_t i = 0;
      for (ncomp_t s=0; s<sc.size(); s+=2) {
        // compute number of samples to be set at relative probability height
        const auto npar =
          static_cast< ncomp_t >(
            static_cast< tk::real >( particles.nunk() ) * sc[s+1] );
        // assign sample values
        for (ncomp_t p=0; p<npar; ++p) particles( i+p, c, offset ) = sc[s];
        i += npar;
      }
    }
  }

  static ctr::InitPolicyType type() noexcept
  { return ctr::InitPolicyType::JOINTDELTA; }
};

//! Beta initialization policy: generate samples from a joint beta PDF
struct InitBeta {

  //! Initialize particle properties (zero)
  template< class eq >
  static void init( const ctr::InputDeck& deck,
                    const tk::RNG& rng,
                    int stream,
                    tk::Particles& particles,
                    tk::ctr::ncomp_type e,
                    tk::ctr::ncomp_type ncomp,
                    tk::ctr::ncomp_type offset )
  {
    using ncomp_t = kw::ncomp::info::expect::type;

    const auto& betapdf =
      deck.template get< tag::param, eq, tag::betapdf >().at(e);

    // use only the first ncomp betapdfs if there are more than the equation is
    // configured for
    const ncomp_t size = std::min( ncomp, betapdf.size() );

    for (ncomp_t c=0; c<size; ++c) {
      // get vector of betapdf parameters for component c
      const auto& bc = betapdf[c];

      for (ncomp_t s=0; s<bc.size(); s+=4) {
        // generate beta random numbers for all particles using parameters in bc
        for (ncomp_t p=0; p<particles.nunk(); ++p)
          rng.beta( stream, 1, bc[s], bc[s+1], bc[s+2], bc[s+3],
                    &particles( p, c, offset ) );
      }
    }

  }

  static ctr::InitPolicyType type() noexcept
  { return ctr::InitPolicyType::JOINTBETA; }
};

//! Gaussian initialization policy: generate samples from a joint Gaussian PDF
//! \note No correlations supported. For correlations, see jointCorrGaussian
struct InitGaussian {

  //! Initialize particle properties (zero)
  template< class eq >
  static void init( const ctr::InputDeck& deck,
                    const tk::RNG& rng,
                    int stream,
                    tk::Particles& particles,
                    tk::ctr::ncomp_type e,
                    tk::ctr::ncomp_type ncomp,
                    tk::ctr::ncomp_type offset )
  {
    using ncomp_t = kw::ncomp::info::expect::type;

    const auto& gaussian =
      deck.template get< tag::param, eq, tag::gaussian >().at(e);

    // use only the first ncomp gaussian if there are more than the equation is
    // configured for
    const ncomp_t size = std::min( ncomp, gaussian.size() );

    for (ncomp_t c=0; c<size; ++c) {
      // get vector of gaussian pdf parameters for component c
      const auto& gc = gaussian[c];

      for (ncomp_t s=0; s<gc.size(); s+=2) {
        // generate Gaussian random numbers for all particles using parameters
        for (ncomp_t p=0; p<particles.nunk(); ++p) {
          auto& par = particles( p, c, offset );
          // sample from Gaussian with zero mean and unit variance
          rng.gaussian( stream, 1, &par );
          // scale to given mean and variance
          par = par * sqrt(gc[s+1]) + gc[s];
        }
      }
    }

  }

  static ctr::InitPolicyType type() noexcept
  { return ctr::InitPolicyType::JOINTGAUSSIAN; }
};

//! \brief Gaussian initialization policy: generate samples from a joint
//!   correlated Gaussian PDF
struct InitCorrGaussian {

  //! Initialize particle properties (zero)
  template< class eq >
  static void init( const ctr::InputDeck& deck,
                    const tk::RNG& rng,
                    int stream,
                    tk::Particles& particles,
                    tk::ctr::ncomp_type e,
                    tk::ctr::ncomp_type ncomp,
                    tk::ctr::ncomp_type offset )
  {
    using ncomp_t = kw::ncomp::info::expect::type;

    const auto& mean = deck.template get< tag::param, eq, tag::mean >().at(e);
    Assert( mean.size() == ncomp, "Size mismatch" );
    const auto& cov_ = deck.template get< tag::param, eq, tag::cov >().at(e);
    Assert( cov_.size() == ncomp*(ncomp+1)/2, "Size mismatch" );

    // Compute covariance matrix using Cholesky-decompositionm, see Intel MKL
    // example: vdrnggaussianmv.c, and UnitTest/tests/RNG/TestRNG.h
    auto cov = cov_;
    lapack_int n = static_cast< lapack_int >( ncomp );
    #ifndef NDEBUG
    lapack_int info =
    #endif
      LAPACKE_dpptrf( LAPACK_ROW_MAJOR, 'U', n, cov.data() );
    Assert( info == 0, "Error in Cholesky-decomposition" );

    // Generate multi-variate Gaussian random numbers for all particles with
    // means and covariance matrix given by user
    for (ncomp_t p=0; p<particles.nunk(); ++p) {
      std::vector< double > r( ncomp );
      rng.gaussianmv( stream, 1, ncomp, mean.data(), cov.data(), r.data() );
      for (ncomp_t c=0; c<ncomp; ++c)
        particles( p, c, offset ) = r[c];
    }
  }

  static ctr::InitPolicyType type() noexcept
  { return ctr::InitPolicyType::JOINTCORRGAUSSIAN; }
};


//! Gamma initialization policy: generate samples from a joint gamma PDF
struct InitGamma {

  //! Initialize particle properties (zero)
  template< class eq >
  static void init( const ctr::InputDeck& deck,
                    const tk::RNG& rng,
                    int stream,
                    tk::Particles& particles,
                    tk::ctr::ncomp_type e,
                    tk::ctr::ncomp_type ncomp,
                    tk::ctr::ncomp_type offset )
  {
    using ncomp_t = kw::ncomp::info::expect::type;

    const auto& gamma =
      deck.template get< tag::param, eq, tag::gamma >().at(e);

    // use only the first ncomp gamma if there are more than the equation is
    // configured for
    const ncomp_t size = std::min( ncomp, gamma.size() );

    for (ncomp_t c=0; c<size; ++c) {
      // get vector of gamma pdf parameters for component c
      const auto& gc = gamma[c];
      // generate gamma random numbers for all particles using parameters in gc
      for (ncomp_t s=0; s<gc.size(); s+=2)
        for (ncomp_t p=0; p<particles.nunk(); ++p)
          rng.gamma( stream, 1, gc[s], gc[s+1], &particles( p, c, offset ) );
    }

  }

  static ctr::InitPolicyType type() noexcept
  { return ctr::InitPolicyType::JOINTGAMMA; }
};

//! List of all initialization policies
using InitPolicies = brigand::list< InitRaw
                                  , InitZero
                                  , InitDelta
                                  , InitBeta
                                  , InitGaussian
                                  , InitCorrGaussian
                                  , InitGamma
                                  >;

} // walker::

#endif // InitPolicy_h
