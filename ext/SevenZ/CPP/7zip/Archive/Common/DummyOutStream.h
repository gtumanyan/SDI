<<<<<<< HEAD
// DummyOutStream.h

#ifndef __DUMMY_OUT_STREAM_H
#define __DUMMY_OUT_STREAM_H

#include "../../../Common/MyCom.h"

#include "../../IStream.h"

class CDummyOutStream:
  public ISequentialOutStream,
  public CMyUnknownImp
{
  CMyComPtr<ISequentialOutStream> _stream;
  UInt64 _size;
public:
  void SetStream(ISequentialOutStream *outStream) { _stream = outStream; }
  void ReleaseStream() { _stream.Release(); }
  void Init() { _size = 0; }
  MY_UNKNOWN_IMP
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
  UInt64 GetSize() const { return _size; }
};

#endif
=======
// DummyOutStream.h

#ifndef __DUMMY_OUT_STREAM_H
#define __DUMMY_OUT_STREAM_H

#include "../../../Common/MyCom.h"

#include "../../IStream.h"

class CDummyOutStream:
  public ISequentialOutStream,
  public CMyUnknownImp
{
  CMyComPtr<ISequentialOutStream> _stream;
  UInt64 _size;
public:
  void SetStream(ISequentialOutStream *outStream) { _stream = outStream; }
  void ReleaseStream() { _stream.Release(); }
  void Init() { _size = 0; }
  MY_UNKNOWN_IMP
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
  UInt64 GetSize() const { return _size; }
};

#endif
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
