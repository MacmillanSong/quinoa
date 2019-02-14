// *****************************************************************************
/*!
  \file      src/PDE/MultiMat/Physics/DG.h
  \copyright 2016-2018, Los Alamos National Security, LLC.
  \brief     Physics configurations for multi-material compressible flow using
    discontinuous Galerkin finite element methods
  \details   This file configures all Physics policy classes for multi-material
    compressible flow implementied using discontinuous Galerkin finite element
    discretizations, defined in PDE/MultiMat/DGMultiMat.h.

    General requirements on MultiMat Physics policy classes:

    - Must define the static function _type()_, returning the enum value of the
      policy option. Example:
      \code{.cpp}
        static ctr::PhysicsType type() noexcept {
          return ctr::PhysicsType::VELEQ;
        }
      \endcode
      which returns the enum value of the option from the underlying option
      class, collecting all possible options for Physics policies.
*/
// *****************************************************************************
#ifndef MultiMatPhysicsDG_h
#define MultiMatPhysicsDG_h

#include <brigand/sequences/list.hpp>

#include "DGVelEq.h"

namespace inciter {
namespace dg {

//! MultiMat Physics policies implemented using discontinuous Galerkin
using MultiMatPhysics = brigand::list< MultiMatPhysicsVelEq >;

} // dg::
} // inciter::

#endif // MultiMatPhysicsDG_h