// *****************************************************************************
/*!
  \file      src/Inciter/Discretization.C
  \copyright 2012-2015 J. Bakosi,
             2016-2018 Los Alamos National Security, LLC.,
             2019 Triad National Security, LLC.
             All rights reserved. See the LICENSE file for details.
  \details   Data and functionality common to all discretization schemes
  \see       Discretization.h and Discretization.C for more info.
*/
// *****************************************************************************

#include "Tags.h"
#include "Reorder.h"
#include "Vector.h"
#include "DerivedData.h"
#include "Discretization.h"
#include "MeshWriter.h"
#include "Inciter/InputDeck/InputDeck.h"
#include "Inciter/Options/Scheme.h"
#include "Print.h"

namespace inciter {

static CkReduction::reducerType PDFMerger;
extern ctr::InputDeck g_inputdeck;

} // inciter::

using inciter::Discretization;

Discretization::Discretization(
  const CProxy_DistFCT& fctproxy,
  const CProxy_Transporter& transporter,
  const tk::CProxy_MeshWriter& meshwriter,
  const std::vector< std::size_t >& ginpoel,
  const tk::UnsMesh::CoordMap& coordmap,
  const std::map< int, std::unordered_set< std::size_t > >& msum,
  int nc ) :
  m_nchare( nc ),
  m_it( 0 ),
  m_itr( 0 ),
  m_itf( 0 ),
  m_initial( 1.0 ),
  m_t( g_inputdeck.get< tag::discr, tag::t0 >() ),
  m_lastDumpTime( -std::numeric_limits< tk::real >::max() ),  
  m_dt( g_inputdeck.get< tag::discr, tag::dt >() ),
  m_nvol( 0 ),
  m_fct( fctproxy ),
  m_transporter( transporter ),
  m_meshwriter( meshwriter ),
  m_el( tk::global2local( ginpoel ) ),     // fills m_inpoel, m_gid, m_lid
  m_coord( setCoord( coordmap ) ),
  m_psup( tk::genPsup( m_inpoel, 4, tk::genEsup(m_inpoel,4) ) ),
  m_v( m_gid.size(), 0.0 ),
  m_vol( m_gid.size(), 0.0 ),
  m_volc(),
  m_bid(),
  m_timer(),
  m_refined( 0 )
// *****************************************************************************
//  Constructor
//! \param[in] fctproxy Distributed FCT proxy
//! \param[in] transporter Host (Transporter) proxy
//! \param[in] meshwriter Mesh writer proxy
//! \param[in] ginpoel Vector of mesh element connectivity owned (global IDs)
//! \param[in] coordmap Coordinates of mesh nodes and their global IDs
//! \param[in] msum Global mesh node IDs associated to chare IDs bordering the
//!   mesh chunk we operate on
//! \param[in] nc Total number of Discretization chares
// *****************************************************************************
{
  Assert( m_psup.second.size()-1 == m_gid.size(),
          "Number of mesh points and number of global IDs unequal" );

  // Convert neighbor nodes to vectors from sets
  for (const auto& n : msum) {
    auto& v = m_msum[ n.first ];
    v.insert( end(v), begin(n.second), end(n.second) );
  }

  // Count the number of mesh nodes at which we receive data from other chares
  // and compute map associating boundary-chare node ID associated to global ID
  std::vector< std::size_t > c( tk::sumvalsize( m_msum ) );
  std::size_t j = 0;
  for (const auto& n : m_msum) for (auto i : n.second) c[ j++ ] = i;
  tk::unique( c );
  m_bid = tk::assignLid( c );

  // Allocate receive buffer for nodal volumes
  m_volc.resize( m_bid.size(), 0.0 );

  // Insert DistFCT chare array element if FCT is needed. Note that even if FCT
  // is configured false in the input deck, at this point, we still need the FCT
  // object as FCT is still being performed, only its results are ignored.
  const auto sch = g_inputdeck.get< tag::discr, tag::scheme >();
  const auto nprop = g_inputdeck.get< tag::component >().nprop();
  if (sch == ctr::SchemeType::DiagCG)
    m_fct[ thisIndex ].insert( m_nchare, m_gid.size(), nprop,
                               m_msum, m_bid, m_lid, m_inpoel );

  contribute( CkCallback(CkReductionTarget(Transporter,disccreated),
              m_transporter) );
}

void
Discretization::resize(
  const tk::UnsMesh::Chunk& chunk,
  const tk::UnsMesh::Coords& coord,
  const std::unordered_map< int, std::vector< std::size_t > >& msum )
// *****************************************************************************
//  Resize mesh data structures (e.g., after mesh refinement)
//! \param[in] chunk New mesh chunk (connectivity and global<->local id maps)
//! \param[in] coord New mesh node coordinates
//! \param[in] msum New node communication map
// *****************************************************************************
{
  m_el = chunk;         // updates m_inpoel, m_gid, m_lid
  m_coord = coord;      // update mesh node coordinates
  m_msum = msum;        // update node communication map

  // Generate local ids for new chare boundary global ids
  std::size_t lid = m_bid.size();
  for (const auto& c : m_msum)
    for (auto g : c.second)
      if (m_bid.find( g ) == end(m_bid))
        m_bid[ g ] = lid++;

  // Resize receive buffer for nodal volumes
  std::fill( begin(m_volc), end(m_volc), 0.0 );
  m_volc.resize( m_bid.size(), 0.0 );

  // Set flag that indicates that we are during time stepping
  m_initial = 0.0;

  // Update mesh volume
  std::fill( begin(m_vol), end(m_vol), 0.0 );
  m_vol.resize( m_gid.size(), 0.0 );

  m_nvol = 0;

  contribute( CkCallback(CkReductionTarget(Transporter,discresized),
              m_transporter) );
}

void
Discretization::registerReducers()
// *****************************************************************************
//  Configure Charm++ reduction types
//!  \details Since this is a [nodeinit] routine, see cg.ci, the
//!   Charm++ runtime system executes the routine exactly once on every
//!   logical node early on in the Charm++ init sequence. Must be static as
//!   it is called without an object. See also: Section "Initializations at
//!   Program Startup" at in the Charm++ manual
//!   http://charm.cs.illinois.edu/manuals/html/charm++/manual.html.
// *****************************************************************************
{
  PDFMerger = CkReduction::addReducer( tk::mergeUniPDFs );
}

tk::UnsMesh::Coords
Discretization::setCoord( const tk::UnsMesh::CoordMap& coordmap )
// *****************************************************************************
// Set mesh coordinates based on coordinates map
// *****************************************************************************
{
  Assert( coordmap.size() == m_gid.size(), "Size mismatch" );
  Assert( coordmap.size() == m_lid.size(), "Size mismatch" );

  tk::UnsMesh::Coords coord;
  coord[0].resize( coordmap.size() );
  coord[1].resize( coordmap.size() );
  coord[2].resize( coordmap.size() );

  for (const auto& p : coordmap) {
    auto i = tk::cref_find( m_lid, p.first );
    coord[0][i] = p.second[0];
    coord[1][i] = p.second[1];
    coord[2][i] = p.second[2];
  }

  return coord;
}

void
Discretization::setRefiner( const CProxy_Refiner& ref )
// *****************************************************************************
//  Set Refiner Charm++ proxy
//! \param[in] ref Incoming refiner proxy to store
// *****************************************************************************
{
  m_refiner = ref;
}

void
Discretization::vol()
// *****************************************************************************
// Sum mesh volumes to nodes, start communicating them on chare-boundaries
// *****************************************************************************
{
  const auto& x = m_coord[0];
  const auto& y = m_coord[1];
  const auto& z = m_coord[2];

  // Compute nodal volumes on our chunk of the mesh
  for (std::size_t e=0; e<m_inpoel.size()/4; ++e) {
    const std::array< std::size_t, 4 > N{{ m_inpoel[e*4+0], m_inpoel[e*4+1],
                                           m_inpoel[e*4+2], m_inpoel[e*4+3] }};
    // compute element Jacobi determinant * 5/120 = element volume / 4
    const std::array< tk::real, 3 >
      ba{{ x[N[1]]-x[N[0]], y[N[1]]-y[N[0]], z[N[1]]-z[N[0]] }},
      ca{{ x[N[2]]-x[N[0]], y[N[2]]-y[N[0]], z[N[2]]-z[N[0]] }},
      da{{ x[N[3]]-x[N[0]], y[N[3]]-y[N[0]], z[N[3]]-z[N[0]] }};
    const auto J = tk::triple( ba, ca, da ) * 5.0 / 120.0;
    ErrChk( J > 0, "Element Jacobian non-positive: PE:" +
                   std::to_string(CkMyPe()) + ", node IDs: " +
                   std::to_string(m_gid[N[0]]) + ',' +
                   std::to_string(m_gid[N[1]]) + ',' +
                   std::to_string(m_gid[N[2]]) + ',' +
                   std::to_string(m_gid[N[3]]) + ", coords: (" +
                   std::to_string(x[N[0]]) + ", " +
                   std::to_string(y[N[0]]) + ", " +
                   std::to_string(z[N[0]]) + "), (" +
                   std::to_string(x[N[1]]) + ", " +
                   std::to_string(y[N[1]]) + ", " +
                   std::to_string(z[N[1]]) + "), (" +
                   std::to_string(x[N[2]]) + ", " +
                   std::to_string(y[N[2]]) + ", " +
                   std::to_string(z[N[2]]) + "), (" +
                   std::to_string(x[N[3]]) + ", " +
                   std::to_string(y[N[3]]) + ", " +
                   std::to_string(z[N[3]]) + ')' );
    // scatter add V/4 to nodes
    for (std::size_t j=0; j<4; ++j) m_vol[N[j]] += J;
  }

  // Store nodal volumes without contributions from other chares on
  // chare-boundaries
  m_v = m_vol;

  // Send our nodal volume contributions to neighbor chares
  if (m_msum.empty())
    contribute( CkCallback(CkReductionTarget(Transporter,vol), m_transporter) );
  else
    for (const auto& n : m_msum) {
      std::vector< tk::real > v( n.second.size() );
      std::size_t j = 0;
      for (auto i : n.second) v[ j++ ] = m_vol[ tk::cref_find(m_lid,i) ];
      thisProxy[ n.first ].comvol( n.second, v );
    }
}

void
Discretization::comvol( const std::vector< std::size_t >& gid,
                        const std::vector< tk::real >& nodevol )
// *****************************************************************************
//  Receive nodal volumes on chare-boundaries
//! \param[in] gid Global mesh node IDs at which we receive volume contributions
//! \param[in] nodevol Partial sums of nodal volume contributions to
//!    chare-boundary nodes
//! \details This function receives contributions to m_vol, which stores the
//!   nodal volumes. While m_vol stores own contributions, m_volc collects the
//!   neighbor chare contributions during communication. This way work on m_vol
//!   and m_volc is overlapped. The two are combined in totalvol().
// *****************************************************************************
{
  Assert( nodevol.size() == gid.size(), "Size mismatch" );

  for (std::size_t i=0; i<gid.size(); ++i) {
    auto bid = tk::cref_find( m_bid, gid[i] );
    Assert( bid < m_volc.size(), "Indexing out of bounds" );
    m_volc[ bid ] += nodevol[i];
  }

  if (++m_nvol == m_msum.size()) {
    m_nvol = 0;
    contribute( CkCallback(CkReductionTarget(Transporter,vol), m_transporter) );
  }
}

void
Discretization::totalvol()
// *****************************************************************************
// Sum mesh volumes and contribute own mesh volume to total volume
// *****************************************************************************
{
  // Combine own and communicated contributions of nodal volumes
  for (const auto& b : m_bid) {
    auto lid = tk::cref_find( m_lid, b.first );
    m_vol[ lid ] += m_volc[ b.second ];
  }

  // Sum mesh volume to host
  std::vector< tk::real > tvol{ 0.0, m_initial };
  for (auto v : m_v) tvol[0] += v;
  contribute( tvol, CkReduction::sum_double,
    CkCallback(CkReductionTarget(Transporter,totalvol), m_transporter) );
}

void
Discretization::stat()
// *****************************************************************************
// Compute mesh cell statistics
// *****************************************************************************
{
  const auto& x = m_coord[0];
  const auto& y = m_coord[1];
  const auto& z = m_coord[2];

  auto MIN = -std::numeric_limits< tk::real >::max();
  auto MAX = std::numeric_limits< tk::real >::max();
  std::vector< tk::real > min{ MAX, MAX, MAX };
  std::vector< tk::real > max{ MIN, MIN, MIN };
  std::vector< tk::real > sum{ 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
  tk::UniPDF edgePDF( 1e-4 );
  tk::UniPDF volPDF( 1e-4 );
  tk::UniPDF ntetPDF( 1e-4 );

  // Compute edge length statistics
  // Note that while the min and max edge lengths are independent of the number
  // of CPUs (by the time they are aggregated across all chares), the sum of
  // the edge lengths and the edge length PDF are not. This is because the
  // edges on the chare-boundary are counted multiple times and we
  // conscientiously do not make an effort to precisely compute this, because
  // that would require communication and more complex logic. Since these
  // statistics are intended as simple average diagnostics, we ignore these
  // small differences. For reproducible average edge lengths and edge length
  // PDFs, run the mesh in serial.
  for (std::size_t p=0; p<m_gid.size(); ++p)
    for (auto i=m_psup.second[p]+1; i<=m_psup.second[p+1]; ++i) {
       const auto dx = x[ m_psup.first[i] ] - x[ p ];
       const auto dy = y[ m_psup.first[i] ] - y[ p ];
       const auto dz = z[ m_psup.first[i] ] - z[ p ];
       const auto length = std::sqrt( dx*dx + dy*dy + dz*dz );
       if (length < min[0]) min[0] = length;
       if (length > max[0]) max[0] = length;
       sum[0] += 1.0;
       sum[1] += length;
       edgePDF.add( length );
    }

  // Compute mesh cell volume statistics
  for (std::size_t e=0; e<m_inpoel.size()/4; ++e) {
    const std::array< std::size_t, 4 > N{{ m_inpoel[e*4+0], m_inpoel[e*4+1],
                                           m_inpoel[e*4+2], m_inpoel[e*4+3] }};
    const std::array< tk::real, 3 >
      ba{{ x[N[1]]-x[N[0]], y[N[1]]-y[N[0]], z[N[1]]-z[N[0]] }},
      ca{{ x[N[2]]-x[N[0]], y[N[2]]-y[N[0]], z[N[2]]-z[N[0]] }},
      da{{ x[N[3]]-x[N[0]], y[N[3]]-y[N[0]], z[N[3]]-z[N[0]] }};
    const auto L = std::cbrt( tk::triple( ba, ca, da ) / 6.0 );
    if (L < min[1]) min[1] = L;
    if (L > max[1]) max[1] = L;
    sum[2] += 1.0;
    sum[3] += L;
    volPDF.add( L );
  }

  // Contribute stats of number of tetrahedra (ntets)
  sum[4] = 1.0;
  min[2] = max[2] = sum[5] = m_inpoel.size() / 4;
  ntetPDF.add( min[2] );

  // Contribute to mesh statistics across all Discretization chares
  contribute( min, CkReduction::min_double,
    CkCallback(CkReductionTarget(Transporter,minstat), m_transporter) );
  contribute( max, CkReduction::max_double,
    CkCallback(CkReductionTarget(Transporter,maxstat), m_transporter) );
  contribute( sum, CkReduction::sum_double,
    CkCallback(CkReductionTarget(Transporter,sumstat), m_transporter) );

  // Serialize PDFs to raw stream
  auto stream = tk::serialize( { edgePDF, volPDF, ntetPDF } );
  // Create Charm++ callback function for reduction of PDFs with
  // Transporter::pdfstat() as the final target where the results will appear.
  CkCallback cb( CkIndex_Transporter::pdfstat(nullptr), m_transporter );
  // Contribute serialized PDF of partial sums to host via Charm++ reduction
  contribute( stream.first, stream.second.get(), PDFMerger, cb );
}

void
Discretization::write(
  const std::vector< std::size_t >& inpoel,
  const tk::UnsMesh::Coords& coord,
  const std::map< int, std::vector< std::size_t > >& bface,
  const std::map< int, std::vector< std::size_t > >& bnode,
  const std::vector< std::size_t >& triinpoel,
  const std::vector< std::string>& elemfieldnames,
  const std::vector< std::string>& nodefieldnames,
  const std::vector< std::vector< tk::real > >& elemfields,
  const std::vector< std::vector< tk::real > >& nodefields,
  CkCallback c )
// *****************************************************************************
//  Output mesh and fields data (solution dump) to file(s)
//! \param[in] inpoel Mesh connectivity for the mesh chunk to be written
//! \param[in] coord Node coordinates of the mesh chunk to be written
//! \param[in] bface Map of boundary-face lists mapped to corresponding side set
//!   ids for this mesh chunk
//! \param[in] bnode Map of boundary-node lists mapped to corresponding side set
//!   ids for this mesh chunk
//! \param[in] triinpoel Interconnectivity of points and boundary-face in this
//!   mesh chunk
//! \param[in] elemfieldnames Names of element fields to be output to file
//! \param[in] nodefieldnames Names of node fields to be output to file
//! \param[in] elemfields Field data in mesh elements to output to file
//! \param[in] nodefields Field data in mesh node to output to file
//! \param[in] c Function to continue with after the write
//! \details Since m_meshwriter is a Charm++ chare group, it never migrates and
//!   an instance is guaranteed on every PE. We index the first PE on every
//!   logical compute node. In Charm++'s non-SMP mode, a node is the same as a
//!   PE, so the index is the same as CkMyPe(). In SMP mode the index is the
//!   first PE on every logical node. In non-SMP mode this yields one or more
//!   output files per PE with zero or non-zero virtualization, respectively. If
//!   there are multiple chares on a PE, the writes are serialized per PE, since
//!   only a single entry method call can be executed at any given time. In SMP
//!   mode, still the same number of files are output (one per chare), but the
//!   output is serialized through the first PE of each compute node. In SMP
//!   mode, channeling multiple files via a single PE on each node is required
//!   by NetCDF and HDF5, as well as ExodusII, since none of these libraries are
//!   thread-safe.
// *****************************************************************************
{
  // If the previous iteration refined (or moved) the mesh or this is called
  // before the first time step, we also output the mesh.
  bool meshoutput = m_itf == 0 ? true : false;

  auto eps = std::numeric_limits< tk::real >::epsilon();
  bool fieldoutput = false;

  // Output field data only if there is no dump at this physical time yet
  if (std::abs(m_lastDumpTime - m_t) > eps ) {
    m_lastDumpTime = m_t;
    ++m_itf;
    fieldoutput = true;
  }

  m_meshwriter[ CkNodeFirst( CkMyNode() ) ].
    write( meshoutput, fieldoutput, m_itr, m_itf, m_t, thisIndex,
           g_inputdeck.get< tag::cmd, tag::io, tag::output >(),
           inpoel, coord, bface, bnode, triinpoel, elemfieldnames,
           nodefieldnames, elemfields, nodefields, c );
}

std::unordered_map< int, std::unordered_set< std::size_t > >
Discretization::msumset() const
// *****************************************************************************
// Return chare-node adjacency map as sets
//! \return Chare-node adjacency map that holds sets instead of vectors
// *****************************************************************************
{
  std::unordered_map< int, std::unordered_set< std::size_t > > m;
  for (const auto& n : m_msum)
    m[ n.first ].insert( n.second.cbegin(), n.second.cend() );

  Assert( m.find( thisIndex ) == m.cend(),
          "Chare-node adjacency map should not contain data for own chare ID" );

  return m;
}

void
Discretization::setdt( tk::real newdt )
// *****************************************************************************
// Set time step size
//! \param[in] newdt Size of the new time step
// *****************************************************************************
{
  m_dt = newdt;

  // Truncate the size of last time step
  const auto term = g_inputdeck.get< tag::discr, tag::term >();
  if (m_t+m_dt > term) m_dt = term - m_t;
}

void
Discretization::next()
// *****************************************************************************
// Prepare for next step
// *****************************************************************************
{
  ++m_it;
  m_t += m_dt;
}

void
Discretization::status()
// *****************************************************************************
// Output one-liner status report
// *****************************************************************************
{
  // Query after how many time steps user wants TTY dump
  const auto tty = g_inputdeck.get< tag::interval, tag::tty >();

  if (thisIndex==0 && !(m_it%tty)) {

    const auto term = g_inputdeck.get< tag::discr, tag::term >();
    const auto t0 = g_inputdeck.get< tag::discr, tag::t0 >();
    const auto nstep = g_inputdeck.get< tag::discr, tag::nstep >();
    const auto field = g_inputdeck.get< tag::interval,tag::field >();
    const auto diag = g_inputdeck.get< tag::interval, tag::diag >();
    const auto verbose = g_inputdeck.get< tag::cmd, tag::verbose >();

    // estimate time elapsed and time for accomplishment
    tk::Timer::Watch ete, eta;
    m_timer.eta( term-t0, m_t-t0, nstep, m_it, ete, eta );
 
    tk::Print print( verbose ? std::cout : std::clog );
 
    // Output one-liner
    print << std::setfill(' ') << std::setw(8) << m_it << "  "
          << std::scientific << std::setprecision(6)
          << std::setw(12) << m_t << "  "
          << m_dt << "  "
          << std::setfill('0')
          << std::setw(3) << ete.hrs.count() << ":"
          << std::setw(2) << ete.min.count() << ":"
          << std::setw(2) << ete.sec.count() << "  "
          << std::setw(3) << eta.hrs.count() << ":"
          << std::setw(2) << eta.min.count() << ":"
          << std::setw(2) << eta.sec.count() << "  ";
  
    // Augment one-liner with output indicators
    if (!(m_it % field)) print << 'f';
    if (!(m_it % diag)) print << 'd';
    if (m_refined) print << 'h';
  
    print << std::endl;
  }
}

#include "NoWarning/discretization.def.h"
