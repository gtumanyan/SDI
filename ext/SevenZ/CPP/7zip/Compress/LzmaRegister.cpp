<<<<<<< HEAD
// LzmaRegister.cpp

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "LzmaDecoder.h"

#ifndef EXTRACT_ONLY
#include "LzmaEncoder.h"
#endif

namespace NCompress {
namespace NLzma {

REGISTER_CODEC_E(LZMA,
    CDecoder(),
    CEncoder(),
    0x30101,
    "LZMA")

}}
=======
// LzmaRegister.cpp

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "LzmaDecoder.h"

#ifndef EXTRACT_ONLY
#include "LzmaEncoder.h"
#endif

namespace NCompress {
namespace NLzma {

REGISTER_CODEC_E(LZMA,
    CDecoder(),
    CEncoder(),
    0x30101,
    "LZMA")

}}
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
void registerLZMA(){}