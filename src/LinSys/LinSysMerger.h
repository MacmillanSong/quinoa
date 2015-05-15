//******************************************************************************
/*!
  \file      src/LinSys/LinSysMerger.h
  \author    J. Bakosi
  \date      Thu 14 May 2015 06:36:38 AM MDT
  \copyright 2012-2015, Jozsef Bakosi.
  \brief     Linear system merger
  \details   Linear system merger.
*/
//******************************************************************************
#ifndef LinSysMerger_h
#define LinSysMerger_h

#include <iostream>     // NOT REALLY NEEDED
#include <numeric>
#include <limits>

#if defined(__clang__) || defined(__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wconversion"
#endif

#include <linsysmerger.decl.h>

#if defined(__clang__) || defined(__GNUC__)
  #pragma GCC diagnostic pop
#endif

#include <Types.h>
#include <Timer.h>
#include <HypreMatrix.h>
#include <HypreVector.h>
#include <Exception.h>
#include <ContainerUtil.h>

namespace tk {

//! Linear system merger Charm++ chare group class
//! \details Instantiations of LinSysMerger comprise a processor aware Charm++
//!   chare group. When instantiated, a new object is created on each PE and not
//!   more (as opposed to individual chares or chare array object elements). The
//!   group's elements are used to collect information from all chare objects
//!   that happen to be on a given PE. See also the Charm++ interface file
//!   linsysmerger.ci. The class is templated so that the same code
//!   (parameterized by the template arguments) can be generated for interacting
//!   with different types of Charm++ proxies.
//! \see http://charm.cs.illinois.edu/manuals/html/charm++/manual.html
//! \author J. Bakosi
template< class HostProxy  >
class LinSysMerger : public CBase_LinSysMerger< HostProxy > {

  // Include Charm++ SDAG code. See http://charm.cs.illinois.edu/manuals/html/
  // charm++/manual.html, Sec. "Structured Control Flow: Structured Dagger".
  LinSysMerger_SDAG_CODE

  private:
   using Group = CBase_LinSysMerger< HostProxy >;

  public:
    //! Constructor
    //! \param[in] host Charm++ host proxy
    //! \param[in] npoin Total number of mesh points
    LinSysMerger( HostProxy& host, std::size_t npoin ) :
      m_host( host ),
      m_chunksize( npoin / static_cast<std::size_t>(CkNumPes()) ),
      m_lower( static_cast<std::size_t>(CkMyPe()) * m_chunksize ),
      m_upper( m_lower + m_chunksize )
    {
      auto remainder = npoin % static_cast<std::size_t>(CkNumPes());
      if (remainder && CkMyPe() == CkNumPes()-1) m_upper += remainder;
      std::cout << CkMyPe() << ": [" << m_lower << "..." << m_upper << ")\n";
      // Create my PE's lhs matrix distributed across all PEs
      m_A.create( m_lower, m_upper );
      // Create my PE's rhs and unknown vectors distributed across all PEs
      m_b.create( m_lower, m_upper );
      m_x.create( m_lower, m_upper );
      // Activate SDAG waits
      wait4lhs();
      wait4hypremat();
      wait4fill();
      wait4asm();
    }

    //! Chares contribute their matrix nonzero values
    //! \note This function does not have to be declared as a Charm++ entry
    //!   method since it is always called by chares on the same PE.
    void charelhs( const std::map< std::size_t,
                                   std::map< std::size_t, tk::real > >& lhs )
    {
      // Store matrix nonzero values owned and pack those to be exported
      std::map< std::size_t,
                std::map< std::size_t,
                          std::map< std::size_t, tk::real > > > exp;
      for (const auto& r : lhs) {
        auto gid = r.first;
        if (gid >= m_lower && gid < m_upper)    // if own
          m_lhs[gid] = r.second;
        else {
          auto pe = gid / m_chunksize;
          if (pe == CkNumPes()) --pe;
          exp[pe][gid] = r.second;
        }
      }
// 
//       std::cout << CkMyPe() << ": nrows=" << m_lhs.size() << ", ";
//       for (const auto& r : m_lhs) {
//         std::cout << "(" << r.first << ":" << r.second.size() << ") ";
//         for (const auto& c : r.second) std::cout << c.first << " ";
//       }
//       for (const auto& p : exp) {
//         std::cout << "e:" << p.first << " ";
//         for (const auto& r : p.second) {
//           std::cout << "(" << r.first << ":" << r.second.size() << ") ";
//           for (const auto& c : r.second) std::cout << c.first << " ";
//         }
//       }
//       std::cout << std::endl;

      // Export non-owned matrix rows values to fellow branches that own them
      for (const auto& p : exp)
        Group::thisProxy[ static_cast<int>(p.first) ].addlhs( p.second );
      // If our portion is complete, we are done
      if (lhscomplete()) trigger_lhs_complete();
    }

    //! Receive matrix nonzeros from fellow group branches
    void addlhs( const std::map< std::size_t,
                                 std::map< std::size_t, tk::real > >& lhs ) {
//       std::cout << "import on " << CkMyPe() << ": nrows=" << lhs.size() << ", ";
//       for (const auto& r : lhs) {
//         std::cout << "(" << r.first << ":" << r.second.size() << ") ";
//         for (const auto& c : r.second) std::cout << c.first << " ";
//       }
//       std::cout << std::endl;
      for (const auto& r : lhs) m_lhs[ r.first ] = r.second;
      if (lhscomplete()) trigger_lhs_complete();
    }

  private:
    HostProxy m_host;           //!< Host proxy
    std::size_t m_chunksize;    //!< Number of rows the first npe-1 PE own
    std::size_t m_lower;        //!< Lower index of the global rows for my PE
    std::size_t m_upper;        //!< Upper index of the global rows for my PE
    tk::hypre::HypreMatrix m_A; //!< Hypre matrix to store the lhs
    tk::hypre::HypreVector m_b; //!< Hypre vector to store the rhs
    tk::hypre::HypreVector m_x; //!< Hypre vector to store the unknowns
    std::map< std::size_t, std::map< std::size_t, tk::real > > m_lhs;
    std::vector< int > m_rows;  //!< Row indices for my PE
    std::vector< int > m_ncols; //!< Number of matrix columns/rows for my PE
    std::vector< int > m_cols;  //!< Matrix column indices for rows for my PE
    std::vector< tk::real > m_vals;  //!< Matrix nonzero values for my PE
    std::vector< std::pair< std::string, tk::Timer::Watch > > m_timestamp;

    //! Check if our portion of the matrix values is complete
    bool lhscomplete() const {
      return m_lhs.size() == m_upper-m_lower &&
             m_lhs.cbegin()->first == m_lower &&
             (--m_lhs.cend())->first == m_upper-1;
    }

    //! Build Hypre data for our portion of the matrix
    void hyprelhs() {
      Assert( lhscomplete(),
              "Nonzero values of distributed matrix on PE " +
              std::to_string( CkMyPe() ) + " is incomplete" );
      for (const auto& r : m_lhs) {
        m_rows.push_back( static_cast< int >( r.first ) );
        m_ncols.push_back( static_cast< int >( r.second.size() ) );
        for (const auto& c : r.second) {
           m_cols.push_back( static_cast< int >( c.first ) );
           m_vals.push_back( c.second );
         }
      }

//       std::cout << CkMyPe() << ": ";
//       for (const auto& r : m_lhs) {
//         std::cout << "(" << r.first << ") ";
//         for (const auto& c : r.second)
//           std::cout << c.first << " ";
//       }
//       std::cout << '\n';

      trigger_hyprelhs_complete();
    }

    //! Set our portion of values of the distributed matrix
    void lhs() {
      Assert( m_vals.size() == m_cols.size(),
              "Matrix values incomplete on " + std::to_string(CkMyPe()) );
      // Set our portion of the matrix values
      m_A.set( static_cast< int >( m_upper - m_lower ),
               m_ncols.data(),
               m_rows.data(),
               m_cols.data(),
               m_vals.data() );
      // Activate SDAG trigger signaling that our matrix part has been filled
      trigger_fill_complete();
    }

    //! Assemble distributed matrix
    void assemble() {
      m_A.assemble();
      trigger_assembly_complete();
    }

    //! Signal back to host that the initialization of the matrix is complete
    //! \details This function contributes to a reduction on all branches (PEs)
    //!   of LinSysMerger to the host, inciter::CProxy_Conductor, given by a
    //!   template argument. This is an overload on the specialization,
    //!   inciter::CProxy_Conductor, of the LinSysMerger template. It creates a
    //!   Charm++ reduction target via creating a callback that invokes the
    //!   typed reduction client, where host is the proxy on which the
    //!   reduction target method, init(), is called upon completion of the
    //!   reduction. Note that we do not use Charm++'s CkReductionTarget macro,
    //!   but explicitly generate the code that the macro would generate. To
    //!   explain why here is Charm++'s CkReductionTarget macro's definition,
    //!   defined in ckreduction.h:
    //!   \code{.cpp}
    //!      #define CkReductionTarget(me, method) \
    //!        CkIndex_##me::redn_wrapper_##method(NULL)
    //!   \endcode
    //!   which takes arguments 'me' (a class name) and 'method' a member
    //!   function of class 'me' and generates the call
    //!   'CkIndex_<class>::redn_wrapper_<method>(NULL)'. With this overload to
    //!   contributeTo() we do the above macro's job for LinSysMerger
    //!   specialized on class inciter::CProxy_Conductor and its init()
    //!   reduction target. This is required since (1) Charm++'s
    //!   CkReductionTarget macro's preprocessing happens earlier than type
    //!   resolution and the string of the template argument would be
    //!   substituted instead of the type specialized (which not what we want
    //!   here), and (2) the template argument class, CProxy_Conductor, is in a
    //!   namespace different than that of LinSysMerger. When a new class is
    //!   used to specialize LinSysMerger, the compiler will alert that a new
    //!   overload needs to be defined.
    //! \note This simplifies client-code, e.g., Conductor, which now requires
    //!   no explicit book-keeping with counters, etc. Also a reduction (instead
    //!   of a direct call to the host) better utilizes the communication
    //!   network as computational nodes can send their aggregated contribution
    //!   to other nodes on a network instead of all chares sending their
    //!   (smaller) contributions to the same host.
    //! \see http://charm.cs.illinois.edu/manuals/html/charm++/manual.html,
    //!   Sections "Processor-Aware Chare Collections" and "Chare Arrays".
    void init_complete( const inciter::CProxy_Conductor& host ) {
      using inciter::CkIndex_Conductor;
      Group::contribute(
        CkCallback( CkIndex_Conductor::redn_wrapper_init(NULL), host )
      );
    }
};

} // tk::

#if defined(__clang__) || defined(__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wconversion"
#endif

#define CK_TEMPLATES_ONLY
#include <linsysmerger.def.h>
#undef CK_TEMPLATES_ONLY

#if defined(__clang__) || defined(__GNUC__)
  #pragma GCC diagnostic pop
#endif

#endif // LinSysMerger_h
