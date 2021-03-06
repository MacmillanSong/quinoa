// *****************************************************************************
/*!
  \file      src/Base/Vector.h
  \copyright 2012-2015 J. Bakosi,
             2016-2018 Los Alamos National Security, LLC.,
             2019 Triad National Security, LLC.
             All rights reserved. See the LICENSE file for details.
  \brief     Vector algebra
  \details   Vector algebra.
*/
// *****************************************************************************
#ifndef Vector_h
#define Vector_h

#include <array>

#include "Types.h"

namespace tk {

//! Compute the cross-product of two vectors
std::array< tk::real, 3 >
cross( const std::array< real, 3 >& v1, const std::array< real, 3 >& v2 );

//! Compute the cross-product of two vectors divided by a scalar
std::array< tk::real, 3 >
crossdiv( const std::array< real, 3 >& v1,
          const std::array< real, 3 >& v2,
          real j );

//! Compute the dot-product of two vectors
tk::real
dot( const std::array< real, 3 >& v1, const std::array< real, 3 >& v2 );

//! Compute the triple-product of three vectors
tk::real
triple( const std::array< real, 3 >& v1,
        const std::array< real, 3 >& v2,
        const std::array< real, 3 >& v3 );

//! \brief Compute the determinant of the Jacobian of a coordinate
//!  transformation over a tetrahedron
tk::real
Jacobian( const std::array< real, 3 >& v1,
          const std::array< real, 3 >& v2,
          const std::array< real, 3 >& v3,
          const std::array< real, 3 >& v4 );

//! \brief Compute the inverse of the Jacobian of a coordinate transformation
//!   over a tetrahedron
std::array< std::array< tk::real, 3 >, 3 >
inverseJacobian( const std::array< tk::real, 3 >& v1,
                 const std::array< tk::real, 3 >& v2,
                 const std::array< tk::real, 3 >& v3,
                 const std::array< tk::real, 3 >& v4 );

} // tk::

#endif // Vector_h
