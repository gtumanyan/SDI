<<<<<<< HEAD
// UniqBlocks.h

#ifndef __UNIQ_BLOCKS_H
#define __UNIQ_BLOCKS_H

#include "../../Common/MyTypes.h"
#include "../../Common/MyBuffer.h"
#include "../../Common/MyVector.h"

struct CUniqBlocks
{
  CObjectVector<CByteBuffer> Bufs;
  CUIntVector Sorted;
  CUIntVector BufIndexToSortedIndex;

  unsigned AddUniq(const Byte *data, size_t size);
  UInt64 GetTotalSizeInBytes() const;
  void GetReverseMap();

  bool IsOnlyEmpty() const
  {
    return (Bufs.Size() == 0 || Bufs.Size() == 1 && Bufs[0].Size() == 0);
  }
};

#endif
=======
// UniqBlocks.h

#ifndef __UNIQ_BLOCKS_H
#define __UNIQ_BLOCKS_H

#include "../../Common/MyTypes.h"
#include "../../Common/MyBuffer.h"
#include "../../Common/MyVector.h"

struct CUniqBlocks
{
  CObjectVector<CByteBuffer> Bufs;
  CUIntVector Sorted;
  CUIntVector BufIndexToSortedIndex;

  unsigned AddUniq(const Byte *data, size_t size);
  UInt64 GetTotalSizeInBytes() const;
  void GetReverseMap();

  bool IsOnlyEmpty() const
  {
    return (Bufs.Size() == 0 || Bufs.Size() == 1 && Bufs[0].Size() == 0);
  }
};

#endif
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
