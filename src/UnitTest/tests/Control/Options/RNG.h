//******************************************************************************
/*!
  \file      src/UnitTest/tests/Control/Options/RNG.h
  \author    J. Bakosi
  \date      Wed 06 Aug 2014 11:45:48 AM MDT
  \copyright 2012-2014, Jozsef Bakosi.
  \brief     Unit tests for Control/Options/RNG
  \details   Unit tests for Control/Options/RNG
*/
//******************************************************************************
#ifndef test_RNG_h
#define test_RNG_h

#include <tut/tut.hpp>
#include <Options/RNG.h>
#include <tkTypes.h>

namespace tut {

//! All tests in group inherited from this base
struct RNG_common {
  const tk::ctr::RNG m;
};

//! Test group shortcuts
using RNG_group = test_group< RNG_common >;
using RNG_object = RNG_group::object;

//! Define test group
RNG_group RNG( "Control/Options/RNG" );

//! Test definitions for group

//! Test that member function param() finds MKL parameter for method type
template<> template<>
void RNG_object::test< 1 >() {
  set_test_name( "param() finds MKL parameter" );
  ensure( "cannot find parameter",
          m.param( tk::ctr::RNGType::RNGSSE_GM19 ) == 0 );
}

//! Test that member function param() throws in DEBUG mode if can't find param
template<> template<>
void RNG_object::test< 2 >() {
  set_test_name( "param() throws if can't find" );

  try {

    m.param( static_cast< tk::ctr::RNGType >( -123433325 ) );
    #ifndef NDEBUG
    fail( "should throw exception in DEBUG mode" );
    #endif

  } catch ( tk::Exception& e ) {
    // exception thrown in DEBUG mode, test ok, Assert skipped in RELEASE mode
    // if any other type of exception is thrown, test fails with except
    ensure( std::string("wrong exception thrown: ") + e.what(),
            std::string( e.what() ).find( "Cannot find parameter" ) !=
              std::string::npos );
  }
}

//! Test copy constructor
template<> template<>
void RNG_object::test< 3 >() {
  set_test_name( "copy constructor" );

  tk::ctr::RNG c( m );
  std::vector< tk::ctr::RNG > v;
  v.push_back( c );
  ensure( "copy constructor used to push_back a RNG object to a std::vector",
          v[0].param( tk::ctr::RNGType::RNGSSE_GM55 ) == 3 );
}

//! Test move constructor
template<> template<>
void RNG_object::test< 4 >() {
  set_test_name( "move constructor" );

  tk::ctr::RNG c( m );
  std::vector< tk::ctr::RNG > v;
  v.emplace_back( std::move(c) );
  ensure( "move constructor used to emplace_back a RNG object to a std::vector",
           v[0].param( tk::ctr::RNGType::MKL_SOBOL ) == VSL_BRNG_SOBOL );
}

//! Test copy assignment
template<> template<>
void RNG_object::test< 5 >() {
  set_test_name( "copy assignment" );

  tk::ctr::RNG c;
  c = m;
  ensure( "find param of copy-assigned RNG",
          c.param( tk::ctr::RNGType::MKL_MCG59 ) == VSL_BRNG_MCG59 );
}

//! Test move assignment
template<> template<>
void RNG_object::test< 6 >() {
  set_test_name( "move assignment" );

  tk::ctr::RNG c;
  c = std::move( m );
  ensure( "find param of move-assigned RNG",
          c.param( tk::ctr::RNGType::MKL_MT2203 ) == VSL_BRNG_MT2203 );
}

//! Test that member function lib() finds MKL library type for a MKL rng
template<> template<>
void RNG_object::test< 7 >() {
  set_test_name( "lib() finds MKL library type" );
  ensure( "cannot find library type",
          m.lib( tk::ctr::RNGType::MKL_R250 ) == tk::ctr::RNGLibType::MKL );
}

//! Test that member function lib() finds RNGSSE library type for a RNGSSE rng
template<> template<>
void RNG_object::test< 8 >() {
  set_test_name( "lib() finds RNGSSE library type" );
  ensure( "cannot find library type",
          m.lib( tk::ctr::RNGType::RNGSSE_GM29 ) ==
            tk::ctr::RNGLibType::RNGSSE );
}

//! Test that member function supportSeq() returns true for an RNGSSE rng
template<> template<>
void RNG_object::test< 9 >() {
  set_test_name( "supportsSeq() true for RNGSSE" );
  ensure_equals( "cannot find RNGSSE rng in support map",
                 m.supportsSeq( tk::ctr::RNGType::RNGSSE_GM29 ), true );
}

//! Test that member function supportSeq() returns false for an non-RNGSSE rng
template<> template<>
void RNG_object::test< 10 >() {
  set_test_name( "supportsSeq() false for non-RNGSSE" );
  ensure_equals( "cannot find non-RNGSSE rng in support map",
                 m.supportsSeq( tk::ctr::RNGType::MKL_SFMT19937 ), false );
}

//! Test that member function param<>() returns default for non-specified
//! parameter
template<> template<>
void RNG_object::test< 11 >() {
  set_test_name( "param() correctly returns default" );

  // empty bundle: no parameter specified
  std::map< tk::ctr::RNGType, tk::ctr::RNGMKLParam > b;
  ensure_equals( "does not return default seed for no parameters",
                 m.param< tk::tag::seed >
                        ( tk::ctr::RNGType::MKL_MRG32K3A, 0, b ), 0 );
}

//! Test that member function param<>() returns parameter for specified
//! parameter
template<> template<>
void RNG_object::test< 12 >() {
  set_test_name( "param() returns specified param" );

  // specify sequence length parameter for RNGSSE rng
  std::map< tk::ctr::RNGType, tk::ctr::RNGSSEParam > b {
    { tk::ctr::RNGType::RNGSSE_GQ581, { 12, tk::ctr::RNGSSESeqLenType::LONG } }
  };
  ensure( "does not return specified sequence length for RNGSSE rng",
          m.param< tk::tag::seqlen >                    // query this field
                 ( tk::ctr::RNGType::RNGSSE_GQ581,      // query this rng
                   tk::ctr::RNGSSESeqLenType::SHORT,    // default if not spec'd
                   b ) ==                               // query this bundle
            tk::ctr::RNGSSESeqLenType::LONG );
}

} // tut::

#endif // test_RNG_h