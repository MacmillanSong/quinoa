// *****************************************************************************
/*!
  \file      src/NoWarning/performer.decl.h
  \author    J. Bakosi
  \date      Sun 07 Aug 2016 03:26:44 PM MDT
  \copyright 2012-2015, Jozsef Bakosi, 2016, Los Alamos National Security, LLC.
  \brief     Include performer.decl.h with turning off specific compiler
             warnings.
*/
// *****************************************************************************
#ifndef nowarning_performer_decl_h
#define nowarning_performer_decl_h

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wreserved-id-macro"
  #pragma clang diagnostic ignored "-Wunused-parameter"
  #pragma clang diagnostic ignored "-Wold-style-cast"
  #pragma clang diagnostic ignored "-Wextra-semi"
  #pragma clang diagnostic ignored "-Wshorten-64-to-32"
#elif defined(__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-parameter"
  #pragma GCC diagnostic ignored "-Wshadow"
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "../Inciter/performer.decl.h"

#if defined(__clang__)
  #pragma clang diagnostic pop
#elif defined(__GNUC__)
  #pragma GCC diagnostic pop
#endif

#endif // nowarning_performer_decl_h
