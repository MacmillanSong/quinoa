// *****************************************************************************
/*!
  \file      src/Control/Inciter/Options/Physics.h
  \author    J. Bakosi
  \date      Mon 22 Aug 2016 08:41:17 AM MDT
  \copyright 2012-2015, Jozsef Bakosi, 2016, Los Alamos National Security, LLC.
  \brief     Physics options for inciter
  \details   Physics options for inciter
*/
// *****************************************************************************
#ifndef PhysicsOptions_h
#define PhysicsOptions_h

#include <boost/mpl/vector.hpp>
#include "NoWarning/for_each.h"

#include "Toggle.h"
#include "Keywords.h"
#include "PUPUtil.h"

namespace inciter {
namespace ctr {

//! Physics types
//! \author J. Bakosi
enum class PhysicsType : uint8_t { BASE=0,
                                   NAVIERSTOKES,
                                   EULER};

//! Pack/Unpack PhysicsType: forward overload to generic enum class packer
//! \author J. Bakosi
inline void operator|( PUP::er& p, PhysicsType& e ) { PUP::pup( p, e ); }

//! \brief Physics options: outsource to base templated on enum type
//! \author J. Bakosi
class Physics : public tk::Toggle< PhysicsType > {

  public:
    //! Valid expected choices to make them also available at compile-time
    //! \author J. Bakosi
    using keywords = boost::mpl::vector< kw::base
                                       , kw::compflow_navierstokes
                                       , kw::compflow_euler
                                       >;

    //! \brief Options constructor
    //! \details Simply initialize in-line and pass associations to base, which
    //!    will handle client interactions
    //! \author J. Bakosi
    explicit Physics() :
      Toggle< PhysicsType >(
        //! Group, i.e., options, name
        "Physics configuration",
        //! Enums -> names (if defined, policy codes, if not, name)
        { { PhysicsType::BASE, kw::base::name() },
          { PhysicsType::NAVIERSTOKES, kw::compflow_navierstokes::name() },
          { PhysicsType::EULER, kw::compflow_euler::name() } },
        //! keywords -> Enums
        { { kw::base::string(), PhysicsType::BASE },
          { kw::compflow_navierstokes::string(), PhysicsType::NAVIERSTOKES },
          { kw::compflow_euler::string(), PhysicsType::EULER } } )
    {
       boost::mpl::for_each< keywords >( assertPolicyCodes() );
    }

    //! \brief Return policy code based on Enum
    //! \param[in] p Enum value of the physics option requested
    //! \return Policy code of the option
    //! \author J. Bakosi
    const std::string& code( PhysicsType p ) const {
      using tk::operator<<;
      auto it = policy.find( p );
      Assert( it != end(policy),
              std::string("Cannot find policy code for physics \"") << p <<
                "\"" );
      return it->second;
    }

  private:
    //! Function object for ensuring the existence of policy codes
    //! \author J. Bakosi
    struct assertPolicyCodes {
      //! \brief Function call operator templated on the type to assert the
      //!   existence of a policy code
      template< typename U > void operator()( U ) {
        static_assert( tk::HasTypedefCode< typename U::info >::value,
                       "Policy code undefined for keyword" );
      }
    };

    //! Enums -> policy code
    std::map< PhysicsType, std::string > policy {
        { PhysicsType::BASE, *kw::base::code() }
      , { PhysicsType::NAVIERSTOKES, *kw::compflow_navierstokes::code() }
      , { PhysicsType::EULER, *kw::compflow_euler::code() }
    };
};

} // ctr::
} // inciter::

#endif // PhysicsOptions_h
