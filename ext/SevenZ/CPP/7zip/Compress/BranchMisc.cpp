<<<<<<< HEAD
// BranchMisc.cpp

#include "StdAfx.h"

#include "BranchMisc.h"

namespace NCompress {
namespace NBranch {

STDMETHODIMP CCoder::Init()
{
  _bufferPos = 0;
  return S_OK;
}

STDMETHODIMP_(UInt32) CCoder::Filter(Byte *data, UInt32 size)
{
  UInt32 processed = (UInt32)BraFunc(data, size, _bufferPos, _encode);
  _bufferPos += processed;
  return processed;
}

}}
=======
// BranchMisc.cpp

#include "StdAfx.h"

#include "BranchMisc.h"

namespace NCompress {
namespace NBranch {

STDMETHODIMP CCoder::Init()
{
  _bufferPos = 0;
  return S_OK;
}

STDMETHODIMP_(UInt32) CCoder::Filter(Byte *data, UInt32 size)
{
  UInt32 processed = (UInt32)BraFunc(data, size, _bufferPos, _encode);
  _bufferPos += processed;
  return processed;
}

}}
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
