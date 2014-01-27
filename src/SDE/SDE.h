//******************************************************************************
/*!
  \file      src/SDE/SDE.h
  \author    J. Bakosi
  \date      Sat 25 Jan 2014 05:25:03 PM MST
  \copyright Copyright 2005-2012, Jozsef Bakosi, All rights reserved.
  \brief     SDE
  \details   SDE
*/
//******************************************************************************
#ifndef SDE_h
#define SDE_h

#include <cstdint>

#include <Model.h>
#include <Base.h>
#include <InitPolicy.h>

namespace quinoa {

//! SDE
template< class Init >
class SDE : public Model {

  public:
    //! Return true indicating that SDE is stochastic
    bool stochastic() const noexcept override { return true; }

    //! Return RNG type if stochastic, NO_RNG if deterministic
    tk::ctr::RNGType rng() const noexcept override { return m_rngType; }

    //! Return number of components
    int ncomp() const noexcept override { return m_ncomp; }

    //! Return initialization policy
    const std::string& initPolicy() const noexcept override {
      return m_initPolicy;
    }

  protected:
    //! Constructor: protected, designed to be base-only
    explicit SDE( const Base& base,
                  tk::ctr::RNGType rngType,
                  const ParProps& particles,
                  int offset,
                  int ncomp ) :
      m_rngType( rngType ),
      m_particles( particles ),
      m_npar( base.control.get< tag::component, tag::npar >() ),
      m_nprop( base.control.nprop() ),
      m_offset( offset ),
      m_ncomp( ncomp )
    {
      Assert( m_rngType != tk::ctr::RNGType::NO_RNG, tk::ExceptType::FATAL,
              "Cannot instantiate class SDE without an RNG" );
      // Initialize particle properties (and throw away init policy)
      Init( m_initPolicy, m_particles, m_npar, m_nprop, m_offset, m_ncomp );
      // Instantiate RNG
      initRNG( base );
    }

    const tk::ctr::RNGType m_rngType;  //!< RNG used
    const ParProps& m_particles;       //!< Particle properties
    const uint64_t m_npar;             //!< Total number of particles
    const int m_nprop;                 //!< Total number of particle properties
    const int m_offset;                //!< Offset SDE operates from
    const int m_ncomp;                 //!< Number of components
    std::unique_ptr< tk::RNG > m_rng;  //!< Random number generator

  private:
    //! Don't permit copy constructor
    SDE(const SDE&) = delete;
    //! Don't permit copy assigment
    SDE& operator=(const SDE&) = delete;
    //! Don't permit move constructor
    SDE(SDE&&) = delete;
    //! Don't permit move assigment
    SDE& operator=(SDE&&) = delete;

    //! Instantiate random number genrator
    void initRNG( const Base& base ) {
      m_rng = std::unique_ptr< tk::RNG >( base.rng[ m_rngType ]() );
    }

    std::string m_initPolicy;                   //!< Initialization policy name
};

} // quinoa::

#endif // SDE_h