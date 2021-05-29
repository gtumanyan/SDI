<<<<<<< HEAD
// CopyRegister.cpp

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "CopyCoder.h"

namespace NCompress {

REGISTER_CODEC_CREATE(CreateCodec, CCopyCoder())

REGISTER_CODEC_2(Copy, CreateCodec, CreateCodec, 0, "Copy")

}
=======
// CopyRegister.cpp

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "CopyCoder.h"

namespace NCompress {

REGISTER_CODEC_CREATE(CreateCodec, CCopyCoder())

REGISTER_CODEC_2(Copy, CreateCodec, CreateCodec, 0, "Copy")

}
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
void registerCopy(){}