//******************************************************************************
/*!
  \file      src/Main/TPLInfo/Silo.h
  \author    J. Bakosi
  \date      Wed Mar 19 11:40:09 2014
  \copyright Copyright 2005-2012, Jozsef Bakosi, All rights reserved.
  \brief     Silo info
  \details   Silo info
*/
//******************************************************************************
#ifndef SiloInfo_h
#define SiloInfo_h

#include <string>

#include <Print.h>

namespace tk {

//! Echo Silo library version information
void echoSilo(const tk::Print& print, const std::string& title);

} // tk::

#endif // SiloInfo_h