// *****************************************************************************
/*!
  \file      src/Inciter/ALECG.C
  \copyright 2012-2015 J. Bakosi,
             2016-2018 Los Alamos National Security, LLC.,
             2019 Triad National Security, LLC.
             All rights reserved. See the LICENSE file for details.
  \brief     ALECG for a PDE system with continuous Galerkin + ALE + RK
  \details   ALECG advances a system of partial differential equations (PDEs)
    using a continuous Galerkin (CG) finite element (FE) spatial discretization
    (using linear shapefunctions on tetrahedron elements) combined with a
    Runge-Kutta (RK) time stepping scheme in the arbitrary Eulerian-Lagrangian
    reference frame.
  \see The documentation in ALECG.h.
*/
// *****************************************************************************

#include "QuinoaConfig.h"
#include "ALECG.h"
#include "Vector.h"
#include "Reader.h"
#include "ContainerUtil.h"
#include "UnsMesh.h"
#include "ExodusIIMeshWriter.h"
#include "Inciter/InputDeck/InputDeck.h"
#include "DerivedData.h"
#include "CGPDE.h"
#include "Discretization.h"
#include "DiagReducer.h"
#include "NodeBC.h"
#include "Refiner.h"
#include "Reorder.h"

#ifdef HAS_ROOT
  #include "RootMeshWriter.h"
#endif

namespace inciter {

extern ctr::InputDeck g_inputdeck;
extern ctr::InputDeck g_inputdeck_defaults;
extern std::vector< CGPDE > g_cgpde;

} // inciter::

using inciter::ALECG;

ALECG::ALECG( const CProxy_Discretization& disc,
              const std::map< int, std::vector< std::size_t > >& /* bface */,
              const std::map< int, std::vector< std::size_t > >& bnode,
              const std::vector< std::size_t >& /* triinpoel */ ) :
  m_disc( disc ),
  m_initial( 1 ),
  m_nsol( 0 ),
  m_nlhs( 0 ),
  m_nrhs( 0 ),
  m_bnode( bnode ),
  m_u( m_disc[thisIndex].ckLocal()->Gid().size(),
       g_inputdeck.get< tag::component >().nprop() ),
  m_du( m_u.nunk(), m_u.nprop() ),
  m_lhs( m_u.nunk(), m_u.nprop() ),
  m_rhs( m_u.nunk(), m_u.nprop() ),
  m_lhsc(),
  m_rhsc(),
  m_vol( 0.0 ),
  m_diag()
// *****************************************************************************
//  Constructor
//! \param[in] disc Discretization proxy
//! \param[in] bface Boundary-faces mapped to side set ids
//! \param[in] bnode Boundary-node lists mapped to side set ids
//! \param[in] triinpoel Boundary-face connectivity
// *****************************************************************************
//! [Constructor]
{
  usesAtSync = true;    // enable migration at AtSync

  // Size communication buffers
  resizeComm();

  // Activate SDAG wait for initially computing the left-hand side
  thisProxy[ thisIndex ].wait4lhs();

  // Signal the runtime system that the workers have been created
  contribute( sizeof(int), &m_initial, CkReduction::sum_int,
    CkCallback(CkReductionTarget(Transporter,comfinal), Disc()->Tr()) );
}
//! [Constructor]

void
ALECG::resizeComm()
// *****************************************************************************
//  Size communication buffers
//! \details The size of the communication buffers are determined based on
//!    Disc()->Bid.size() and m_u.nprop().
// *****************************************************************************
{
  auto d = Disc();

  auto np = m_u.nprop();
  auto nb = d->Bid().size();
  m_lhsc.resize( nb );
  for (auto& b : m_lhsc) b.resize( np );
  m_rhsc.resize( nb );
  for (auto& b : m_rhsc) b.resize( np );

  // Zero communication buffers
  for (auto& b : m_lhsc) std::fill( begin(b), end(b), 0.0 );
}

void
ALECG::registerReducers()
// *****************************************************************************
//  Configure Charm++ reduction types initiated from this chare array
//! \details Since this is a [nodeinit] routine, the runtime system executes the
//!   routine exactly once on every logical node early on in the Charm++ init
//!   sequence. Must be static as it is called without an object. See also:
//!   Section "Initializations at Program Startup" at in the Charm++ manual
//!   http://charm.cs.illinois.edu/manuals/html/charm++/manual.html.
// *****************************************************************************
{
  NodeDiagnostics::registerReducers();
}

void
ALECG::ResumeFromSync()
// *****************************************************************************
//  Return from migration
//! \details This is called when load balancing (LB) completes. The presence of
//!   this function does not affect whether or not we block on LB.
// *****************************************************************************
{
  if (Disc()->It() == 0) Throw( "it = 0 in ResumeFromSync()" );

  if (!g_inputdeck.get< tag::cmd, tag::nonblocking >()) dt();
}

void
ALECG::setup( tk::real v )
// *****************************************************************************
// Setup rows, query boundary conditions, output mesh, etc.
//! \param[in] v Total mesh volume
// *****************************************************************************
{
  auto d = Disc();

  // Store total mesh volume
  m_vol = v;

  // Set initial conditions for all PDEs
  for (const auto& eq : g_cgpde) eq.initialize( d->Coord(), m_u, d->T() );

  // Output initial conditions to file (regardless of whether it was requested)
  writeFields( CkCallback(CkIndex_ALECG::init(), thisProxy[thisIndex]) );
}

//! [init and lhs]
void
ALECG::init()
// *****************************************************************************
// Initially compute left hand side diagonal matrix
// *****************************************************************************
{
  // Compute left-hand side of PDEs
  lhs();
}
//! [init and lhs]

//! [Merge lhs and continue]
void
ALECG::lhsmerge()
// *****************************************************************************
// The own and communication portion of the left-hand side is complete
// *****************************************************************************
{
  // Combine own and communicated contributions to left hand side
  auto d = Disc();

  // Combine own and communicated contributions to LHS and ICs
  for (const auto& b : d->Bid()) {
    auto lid = tk::cref_find( d->Lid(), b.first );
    const auto& blhsc = m_lhsc[ b.second ];
    for (ncomp_t c=0; c<m_lhs.nprop(); ++c) m_lhs(lid,c,0) += blhsc[c];
  }

  // Zero communication buffers for next time step
  for (auto& b : m_rhsc) std::fill( begin(b), end(b), 0.0 );

  // Continue after lhs is complete
  if (m_initial) start(); else lhs_complete();
}
//! [Merge lhs and continue]

void
ALECG::resized()
// *****************************************************************************
// Resizing data sutrctures after mesh refinement has been completed
// *****************************************************************************
{
  resize_complete();
}

void
ALECG::start()
// *****************************************************************************
//  Start time stepping
// *****************************************************************************
{
  // Start timer measuring time stepping wall clock time
  Disc()->Timer().zero();

  // Start time stepping by computing the size of the next time step)
  dt();
}

//! [Compute own and send lhs on chare-boundary]
void
ALECG::lhs()
// *****************************************************************************
// Compute the left-hand side of transport equations
// *****************************************************************************
{
  auto d = Disc();

  // Compute own portion of the lhs
  // m_lhs = ...

  if (d->Msum().empty())        // in serial we are done
    comlhs_complete();
  else // send contributions of lhs to chare-boundary nodes to fellow chares
    for (const auto& n : d->Msum()) {
      std::vector< std::vector< tk::real > > l( n.second.size() );
      std::size_t j = 0;
      for (auto i : n.second) l[ j++ ] = m_lhs[ tk::cref_find(d->Lid(),i) ];
      thisProxy[ n.first ].comlhs( n.second, l );
    }

  ownlhs_complete();
}
//! [Compute own and send lhs on chare-boundary]

//! [Receive lhs on chare-boundary]
void
ALECG::comlhs( const std::vector< std::size_t >& gid,
               const std::vector< std::vector< tk::real > >& L )
// *****************************************************************************
//  Receive contributions to left-hand side diagonal matrix on chare-boundaries
//! \param[in] gid Global mesh node IDs at which we receive LHS contributions
//! \param[in] L Partial contributions of LHS to chare-boundary nodes
//! \details This function receives contributions to m_lhs, which stores the
//!   diagonal (lumped) mass matrix at mesh nodes. While m_lhs stores
//!   own contributions, m_lhsc collects the neighbor chare contributions during
//!   communication. This way work on m_lhs and m_lhsc is overlapped. The two
//!   are combined in lhsmerge().
// *****************************************************************************
{
  Assert( L.size() == gid.size(), "Size mismatch" );

  using tk::operator+=;

  auto d = Disc();

  for (std::size_t i=0; i<gid.size(); ++i) {
    auto bid = tk::cref_find( d->Bid(), gid[i] );
    Assert( bid < m_lhsc.size(), "Indexing out of bounds" );
    m_lhsc[ bid ] += L[i];
  }

  // When we have heard from all chares we communicate with, this chare is done
  if (++m_nlhs == d->Msum().size()) {
    m_nlhs = 0;
    comlhs_complete();
  }
}
//! [Receive lhs on chare-boundary]

void
ALECG::dt()
// *****************************************************************************
// Compute time step size
// *****************************************************************************
{
  tk::real mindt = std::numeric_limits< tk::real >::max();

  auto const_dt = g_inputdeck.get< tag::discr, tag::dt >();
  auto def_const_dt = g_inputdeck_defaults.get< tag::discr, tag::dt >();
  auto eps = std::numeric_limits< tk::real >::epsilon();

  auto d = Disc();

  // use constant dt if configured
  if (std::abs(const_dt - def_const_dt) > eps) {

    mindt = const_dt;

  } else {      // compute dt based on CFL

    //! [Find the minimum dt across all PDEs integrated]
    for (const auto& eq : g_cgpde) {
      auto eqdt = eq.dt( d->Coord(), d->Inpoel(), m_u );
      if (eqdt < mindt) mindt = eqdt;
    }

    // Scale smallest dt with CFL coefficient
    mindt *= g_inputdeck.get< tag::discr, tag::cfl >();
    //! [Find the minimum dt across all PDEs integrated]

  }

  //! [Advance]
  // Actiavate SDAG waits for time step
  thisProxy[ thisIndex ].wait4rhs();
  thisProxy[ thisIndex ].wait4out();

  // Contribute to minimum dt across all chares the advance to next step
  contribute( sizeof(tk::real), &mindt, CkReduction::min_double,
              CkCallback(CkReductionTarget(Transporter,advance), d->Tr()) );
  //! [Advance]
}

void
ALECG::rhs()
// *****************************************************************************
// Compute right-hand side of transport equations
// *****************************************************************************
{
  auto d = Disc();

  // Compute own portion of the right-hand side
  // ...

  // Communicate rhs to other chares on chare-boundary
  if (d->Msum().empty())        // in serial we are done
    comrhs_complete();
  else // send contributions of rhs to chare-boundary nodes to fellow chares
    for (const auto& n : d->Msum()) {
      std::vector< std::vector< tk::real > > r( n.second.size() );
      std::size_t j = 0;
      for (auto i : n.second) r[ j++ ] = m_rhs[ tk::cref_find(d->Lid(),i) ];
      thisProxy[ n.first ].comrhs( n.second, r );
    }

  ownrhs_complete();
}

void
ALECG::comrhs( const std::vector< std::size_t >& gid,
                const std::vector< std::vector< tk::real > >& R )
// *****************************************************************************
//  Receive contributions to right-hand side vector on chare-boundaries
//! \param[in] gid Global mesh node IDs at which we receive RHS contributions
//! \param[in] R Partial contributions of RHS to chare-boundary nodes
//! \details This function receives contributions to m_rhs, which stores the
//!   right hand side vector at mesh nodes. While m_rhs stores own
//!   contributions, m_rhsc collects the neighbor chare contributions during
//!   communication. This way work on m_rhs and m_rhsc is overlapped. The two
//!   are combined in solve().
// *****************************************************************************
{
  Assert( R.size() == gid.size(), "Size mismatch" );

  using tk::operator+=;

  auto d = Disc();

  for (std::size_t i=0; i<gid.size(); ++i) {
    auto bid = tk::cref_find( d->Bid(), gid[i] );
    Assert( bid < m_rhsc.size(), "Indexing out of bounds" );
    m_rhsc[ bid ] += R[i];
  }

  // When we have heard from all chares we communicate with, this chare is done
  if (++m_nrhs == d->Msum().size()) {
    m_nrhs = 0;
    comrhs_complete();
  }
}

void
ALECG::solve()
// *****************************************************************************
//  Solve low and high order diagonal systems
// *****************************************************************************
{
  const auto ncomp = m_rhs.nprop();

  auto d = Disc();

  // Combine own and communicated contributions to rhs
  for (const auto& b : d->Bid()) {
    auto lid = tk::cref_find( d->Lid(), b.first );
    const auto& brhsc = m_rhsc[ b.second ];
    for (ncomp_t c=0; c<ncomp; ++c) m_rhs(lid,c,0) += brhsc[c];
  }

  // Zero communication buffers for next time step
  for (auto& b : m_rhsc) std::fill( begin(b), end(b), 0.0 );

  // Solve sytem
  // m_du = m_rhs / m_lhs;

  //! [Continue after solve]
  // Compute diagnostics, e.g., residuals
  auto diag_computed = m_diag.compute( *d, m_u );
  // Increase number of iterations and physical time
  d->next();
  // Signal that diagnostics have been computed (or in this case, skipped)
  if (!diag_computed) diag();
  // Optionally refine mesh
  refine();
  //! [Continue after solve]
}

void
ALECG::writeFields( CkCallback c ) const
// *****************************************************************************
// Output mesh-based fields to file
//! \param[in] c Function to continue with after the write
// *****************************************************************************
{
  auto d = Disc();

  // Query and collect field names from PDEs integrated
  std::vector< std::string > nodefieldnames;
  for (const auto& eq : g_cgpde) {
    auto n = eq.fieldNames();
    nodefieldnames.insert( end(nodefieldnames), begin(n), end(n) );
  }

  // Collect node field solution
  auto u = m_u;
  std::vector< std::vector< tk::real > > nodefields;
  for (const auto& eq : g_cgpde) {
    auto o = eq.fieldOutput( d->T(), m_vol, d->Coord(), d->V(), u );
    nodefields.insert( end(nodefields), begin(o), end(o) );
  }

  // Send mesh and fields data (solution dump) for output to file
  d->write( d->Inpoel(), d->Coord(), {}, tk::remap(m_bnode,d->Lid()), {}, {},
            nodefieldnames, {}, nodefields, c );
}

void
ALECG::advance( tk::real newdt )
// *****************************************************************************
// Advance equations to next time step
//! \param[in] newdt Size of this new time step
// *****************************************************************************
{
  auto d = Disc();

  // Set new time step size
  d->setdt( newdt );

  // Compute rhs for next time step
  rhs();
}

void
ALECG::diag()
// *****************************************************************************
// Signal the runtime system that diagnostics have been computed
// *****************************************************************************
{
  diag_complete();
}

void
ALECG::refine()
// *****************************************************************************
// Optionally refine/derefine mesh
// *****************************************************************************
{
  //! [Refine]
  auto d = Disc();

  auto dtref = g_inputdeck.get< tag::amr, tag::dtref >();
  auto dtfreq = g_inputdeck.get< tag::amr, tag::dtfreq >();

  // if t>0 refinement enabled and we hit the frequency
  if (dtref && !(d->It() % dtfreq)) {   // refine

    d->Ref()->dtref( {}, m_bnode, {} );

  } else {      // do not refine

    ref_complete();
    lhs_complete();
    resize_complete();

  }
  //! [Refine]
}

//! [Resize]
void
ALECG::resizeAfterRefined(
  const std::vector< std::size_t >& /*ginpoel*/,
  const tk::UnsMesh::Chunk& chunk,
  const tk::UnsMesh::Coords& coord,
  const std::unordered_map< std::size_t, tk::UnsMesh::Edge >& addedNodes,
  const std::unordered_map< std::size_t, std::size_t >& /*addedTets*/,
  const std::unordered_map< int, std::vector< std::size_t > >& msum,
  const std::map< int, std::vector< std::size_t > >& /*bface*/,
  const std::map< int, std::vector< std::size_t > >& bnode,
  const std::vector< std::size_t >& /*triinpoel*/ )
// *****************************************************************************
//  Receive new mesh from Refiner
//! \param[in] ginpoel Mesh connectivity with global node ids
//! \param[in] chunk New mesh chunk (connectivity and global<->local id maps)
//! \param[in] coord New mesh node coordinates
//! \param[in] addedNodes Newly added mesh nodes and their parents (local ids)
//! \param[in] addedTets Newly added mesh cells and their parents (local ids)
//! \param[in] msum New node communication map
//! \param[in] bface Boundary-faces mapped to side set ids
//! \param[in] bnode Boundary-node lists mapped to side set ids
//! \param[in] triinpoel Boundary-face connectivity
// *****************************************************************************
{
  auto d = Disc();

  // Set flag that indicates that we are during time stepping
  m_initial = 0;

  // Zero field output iteration count between two mesh refinement steps
  d->Itf() = 0;

  // Increase number of iterations with mesh refinement
  ++d->Itr();

  // Resize mesh data structures
  d->resize( chunk, coord, msum );

  // Resize auxiliary solution vectors
  auto npoin = coord[0].size();
  auto nprop = m_u.nprop();
  m_u.resize( npoin, nprop );
  m_du.resize( npoin, nprop );
  m_lhs.resize( npoin, nprop );
  m_rhs.resize( npoin, nprop );

  // Update solution on new mesh
  for (const auto& n : addedNodes)
    for (std::size_t c=0; c<nprop; ++c)
      m_u(n.first,c,0) = (m_u(n.second[0],c,0) + m_u(n.second[1],c,0))/2.0;

  // Update physical-boundary node lists
  m_bnode = bnode;

  // Resize communication buffers
  resizeComm();

  // Activate SDAG waits for re-computing the left-hand side
  thisProxy[ thisIndex ].wait4lhs();

  ref_complete();

  contribute( CkCallback(CkReductionTarget(Transporter,workresized), d->Tr()) );
}
//! [Resize]

void
ALECG::out()
// *****************************************************************************
// Output mesh field data
// *****************************************************************************
{
  auto d = Disc();

  const auto term = g_inputdeck.get< tag::discr, tag::term >();
  const auto nstep = g_inputdeck.get< tag::discr, tag::nstep >();
  const auto eps = std::numeric_limits< tk::real >::epsilon();
  const auto fieldfreq = g_inputdeck.get< tag::interval, tag::field >();

  // output field data if field iteration count is reached or in the last time
  // step
  if ( !((d->It()) % fieldfreq) ||
       (std::fabs(d->T()-term) < eps || d->It() >= nstep) )
    writeFields( CkCallback(CkIndex_ALECG::step(), thisProxy[thisIndex]) );
  else
    step();
}

void
ALECG::step()
// *****************************************************************************
// Evaluate whether to continue with next step
// *****************************************************************************
{
  auto d = Disc();

  // Output one-liner status report to screen
  d->status();

  const auto term = g_inputdeck.get< tag::discr, tag::term >();
  const auto nstep = g_inputdeck.get< tag::discr, tag::nstep >();
  const auto eps = std::numeric_limits< tk::real >::epsilon();
  const auto lbfreq = g_inputdeck.get< tag::cmd, tag::lbfreq >();
  const auto nonblocking = g_inputdeck.get< tag::cmd, tag::nonblocking >();

  // If neither max iterations nor max time reached, continue, otherwise finish
  if (std::fabs(d->T()-term) > eps && d->It() < nstep) {

    if ( (d->It()) % lbfreq == 0 ) {
      AtSync();
      if (nonblocking) dt();
    }
    else {
      dt();
    }

  } else {
    d->contribute( CkCallback( CkReductionTarget(Transporter,finish), d->Tr() ) );
  }
}

#include "NoWarning/alecg.def.h"
