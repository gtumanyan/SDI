<<<<<<< HEAD
// BcjCoder.h

#ifndef __COMPRESS_BCJ_CODER_H
#define __COMPRESS_BCJ_CODER_H

#include "../../../C/Bra.h"

#include "../../Common/MyCom.h"

#include "../ICoder.h"

namespace NCompress {
namespace NBcj {

class CCoder:
  public ICompressFilter,
  public CMyUnknownImp
{
  UInt32 _bufferPos;
  UInt32 _prevMask;
  int _encode;
public:
  MY_UNKNOWN_IMP1(ICompressFilter);
  INTERFACE_ICompressFilter(;)

  CCoder(int encode): _bufferPos(0), _encode(encode) { x86_Convert_Init(_prevMask); }
};

}}

#endif
=======
// BcjCoder.h

#ifndef __COMPRESS_BCJ_CODER_H
#define __COMPRESS_BCJ_CODER_H

#include "../../../C/Bra.h"

#include "../../Common/MyCom.h"

#include "../ICoder.h"

namespace NCompress {
namespace NBcj {

class CCoder:
  public ICompressFilter,
  public CMyUnknownImp
{
  UInt32 _bufferPos;
  UInt32 _prevMask;
  int _encode;
public:
  MY_UNKNOWN_IMP1(ICompressFilter);
  INTERFACE_ICompressFilter(;)

  CCoder(int encode): _bufferPos(0), _encode(encode) { x86_Convert_Init(_prevMask); }
};

}}

#endif
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
