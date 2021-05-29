<<<<<<< HEAD
// UpdatePair.h

#ifndef __UPDATE_PAIR_H
#define __UPDATE_PAIR_H

#include "DirItem.h"
#include "UpdateAction.h"

#include "../../Archive/IArchive.h"

struct CUpdatePair
{
  NUpdateArchive::NPairState::EEnum State;
  int ArcIndex;
  int DirIndex;
  int HostIndex; // >= 0 for alt streams only, contains index of host pair

  CUpdatePair(): ArcIndex(-1), DirIndex(-1), HostIndex(-1) {}
};

void GetUpdatePairInfoList(
    const CDirItems &dirItems,
    const CObjectVector<CArcItem> &arcItems,
    NFileTimeType::EEnum fileTimeType,
    CRecordVector<CUpdatePair> &updatePairs);

#endif
=======
// UpdatePair.h

#ifndef __UPDATE_PAIR_H
#define __UPDATE_PAIR_H

#include "DirItem.h"
#include "UpdateAction.h"

#include "../../Archive/IArchive.h"

struct CUpdatePair
{
  NUpdateArchive::NPairState::EEnum State;
  int ArcIndex;
  int DirIndex;
  int HostIndex; // >= 0 for alt streams only, contains index of host pair

  CUpdatePair(): ArcIndex(-1), DirIndex(-1), HostIndex(-1) {}
};

void GetUpdatePairInfoList(
    const CDirItems &dirItems,
    const CObjectVector<CArcItem> &arcItems,
    NFileTimeType::EEnum fileTimeType,
    CRecordVector<CUpdatePair> &updatePairs);

#endif
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
