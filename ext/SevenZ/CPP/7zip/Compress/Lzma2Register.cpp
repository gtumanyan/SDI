// Lzma2Register.cpp

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "Lzma2Decoder.h"

#ifndef EXTRACT_ONLY
#include "Lzma2Encoder.h"
#endif

namespace NCompress {
namespace NLzma2 {

REGISTER_CODEC_E(LZMA2,
    CDecoder(),
    CEncoder(),
    0x21,
    "LZMA2")

}}
void registerLZMA2(){}