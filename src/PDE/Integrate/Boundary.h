// *****************************************************************************
/*!
  \file      src/PDE/Integrate/Boundary.h
  \copyright 2012-2015 J. Bakosi,
             2016-2018 Los Alamos National Security, LLC.,
             2019 Triad National Security, LLC.
             All rights reserved. See the LICENSE file for details.
  \brief     Functions for computing physical boundary surface integrals of a
     system of PDEs in DG methods
  \details   This file contains functionality for computing physical boundary
     surface integrals of a system of PDEs used in discontinuous Galerkin
     methods for various orders of numerical representation.
*/
// *****************************************************************************
#ifndef Boundary_h
#define Boundary_h

#include "Basis.h"
#include "Surface.h"
#include "Types.h"
#include "Fields.h"
#include "FaceData.h"
#include "UnsMesh.h"
#include "FunctionPrototypes.h"

namespace tk {

using ncomp_t = kw::ncomp::info::expect::type;
using bcconf_t = kw::sideset::info::expect::type;

//! Compute boundary surface flux integrals for a given boundary type for DG
void
bndSurfInt( ncomp_t system,
            ncomp_t ncomp,
            ncomp_t offset,
            const std::vector< bcconf_t >& bcconfig,
            const inciter::FaceData& fd,
            const Fields& geoFace,
            const std::vector< std::size_t >& inpoel,
            const UnsMesh::Coords& coord,
            real t,
            const RiemannFluxFn& flux,
            const VelFn& vel,
            const StateFn& state,
            const Fields& U,
            const Fields& limFunc,
            Fields& R );

//! Update the rhs by adding the boundary surface integration term
void
update_rhs_bc ( ncomp_t ncomp,
                ncomp_t offset,
                const std::size_t ndof,
                const tk::real wt,
                const std::size_t el,
                const std::vector< tk::real >& fl,
                const std::vector< tk::real >& B_l,
                Fields& R );
} // tk::

#endif // Boundary_h
