//******************************************************************************
/*!
  \file      src/MonteCarlo/MonteCarlo.h
  \author    J. Bakosi
  \date      Sat 25 Jan 2014 06:36:03 PM MST
  \copyright Copyright 2005-2012, Jozsef Bakosi, All rights reserved.
  \brief     Monte Carlo
  \details   Monte Carlo
*/
//******************************************************************************
#ifndef MonteCarlo_h
#define MonteCarlo_h

#include <Base.h>
#include <Statistics.h>
#include <GlobWriter.h>
#include <TxtStatWriter.h>

namespace quinoa {

//! MonteCarlo

class MonteCarlo {

  public:
    //! Destructor
    virtual ~MonteCarlo() = default;

    //! Run
    virtual void run() = 0;

  protected:
    //! Constructor: protected, designed to be base-only
    explicit MonteCarlo( const Base& base ) :
      m_base( base ),
      m_npar( base.control.get< tag::component, tag::npar >() ),
      m_term( base.control.get< tag::incpar, tag::term >() ),
      m_totalTime( base.timer.create("Total solution") ),
      m_particles( m_npar, base.control.nprop() ),
      m_statistics( base, m_particles ),
      m_glob( base.control.get< tag::cmd, tag::io, tag::glob >() ),
      m_stat( base.control.get< tag::cmd, tag::io, tag::stat >(), m_statistics )
    {}

    //! Constant accessor to control object
    //! \return Control object
    const ctr::InputDeck& control() const noexcept { return m_base.control; }

    //! Constant accessor to print object
    //! \return Print object
    const QuinoaPrint& print() const noexcept { return m_base.print; }

    //! Constant accessor to timer object pointer
    //! \return Pointer to timer object
    const tk::Timer& timer() const noexcept { return m_base.timer; }

    //! Constant accessor to base
    //! \return Pointer to base
    const Base& base() const noexcept { return m_base; }

    //! Accessor to statistics estimator
    //! \return Pointer to statistics estimator
    Statistics& statistics() noexcept { return m_statistics; }

    //! Accessor to glob file writer
    //! \return Pointer to glob file writer
    GlobWriter& globWriter() noexcept { return m_glob; }

    //! Accessor to statistics file writer
    //! \return Pointer to statistics file writer
    TxtStatWriter& statWriter() noexcept { return m_stat; }

    //! Accessor to particle properties pointer
    //! \return Particle properties array
    const ParProps& particles() const noexcept { return m_particles; }

    //! Accessor to max run time
    //! \return Max run time
    const tk::real& term() const noexcept { return m_term; }

    const Base& m_base;                             //!< Essentials
    const uint64_t m_npar;                          //!< Number of particles
    const tk::real m_term;                          //!< Maximum run time
    const tk::TimerIdx m_totalTime;                 //!< Timer for total run    
    const ParProps m_particles;                     //!< Particle properties

  private:
    //! Don't permit copy constructor
    MonteCarlo(const MonteCarlo&) = delete;
    //! Don't permit copy assigment
    MonteCarlo& operator=(const MonteCarlo&) = delete;
    //! Don't permit move constructor
    MonteCarlo(MonteCarlo&&) = delete;
    //! Don't permit move assigment
    MonteCarlo& operator=(MonteCarlo&&) = delete;

    Statistics m_statistics;                        //!< Statistics estimator
    GlobWriter m_glob;                              //!< Glob file writer
    TxtStatWriter m_stat;                           //!< Statistics file writer
};

} // quinoa::

#endif // MonteCarlo_h