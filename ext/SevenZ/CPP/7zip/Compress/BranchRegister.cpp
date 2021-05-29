<<<<<<< HEAD:ext/SevenZ/CPP/7zip/Compress/BranchRegister.cpp
// BranchRegister.cpp

#include "StdAfx.h"

#include "../../../C/Bra.h"

#include "../Common/RegisterCodec.h"

#include "BranchMisc.h"

namespace NCompress {
namespace NBranch {

#define CREATE_BRA(n) \
    REGISTER_FILTER_CREATE(CreateBra_Decoder_ ## n, CCoder(n ## _Convert, false)) \
    REGISTER_FILTER_CREATE(CreateBra_Encoder_ ## n, CCoder(n ## _Convert, true)) \

CREATE_BRA(PPC)
CREATE_BRA(ARM)
CREATE_BRA(ARMT)
CREATE_BRA(SPARC)

#define METHOD_ITEM(n, id, name) \
    REGISTER_FILTER_ITEM( \
      CreateBra_Decoder_ ## n, \
      CreateBra_Encoder_ ## n, \
      0x3030000 + id, name)

REGISTER_CODECS_VAR
{
  METHOD_ITEM(PPC,   0x205, "PPC"),
  METHOD_ITEM(ARM,   0x501, "ARM"),
  METHOD_ITEM(ARMT,  0x701, "ARMT"),
  METHOD_ITEM(SPARC, 0x805, "SPARC")
};

REGISTER_CODECS(Branch)

}}
=======
// BranchRegister.cpp

#include "StdAfx.h"

#include "../../../C/Bra.h"

#include "../Common/RegisterCodec.h"

#include "BranchMisc.h"

namespace NCompress {
namespace NBranch {

#define CREATE_BRA(n) \
    REGISTER_FILTER_CREATE(CreateBra_Decoder_ ## n, CCoder(n ## _Convert, false)) \
    REGISTER_FILTER_CREATE(CreateBra_Encoder_ ## n, CCoder(n ## _Convert, true)) \

CREATE_BRA(PPC)
CREATE_BRA(ARM)
CREATE_BRA(ARMT)
CREATE_BRA(SPARC)

#define METHOD_ITEM(n, id, name) \
    REGISTER_FILTER_ITEM( \
      CreateBra_Decoder_ ## n, \
      CreateBra_Encoder_ ## n, \
      0x3030000 + id, name)

REGISTER_CODECS_VAR
{
  METHOD_ITEM(PPC,   0x205, "PPC"),
  METHOD_ITEM(ARM,   0x501, "ARM"),
  METHOD_ITEM(ARMT,  0x701, "ARMT"),
  METHOD_ITEM(SPARC, 0x805, "SPARC")
};

REGISTER_CODECS(Branch)

}}
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8:external/SevenZ/build/CPP/7zip/Compress/BranchRegister.cpp
void registerBranch(){}