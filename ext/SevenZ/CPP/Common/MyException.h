<<<<<<< HEAD
// Common/Exception.h

#ifndef __COMMON_EXCEPTION_H
#define __COMMON_EXCEPTION_H

#include "MyWindows.h"

struct CSystemException
{
  HRESULT ErrorCode;
  CSystemException(HRESULT errorCode): ErrorCode(errorCode) {}
};

#endif
=======
// Common/Exception.h

#ifndef __COMMON_EXCEPTION_H
#define __COMMON_EXCEPTION_H

#include "MyWindows.h"

struct CSystemException
{
  HRESULT ErrorCode;
  CSystemException(HRESULT errorCode): ErrorCode(errorCode) {}
};

#endif
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
