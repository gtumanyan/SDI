<<<<<<< HEAD
// BcjRegister.cpp

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "BcjCoder.h"

namespace NCompress {
namespace NBcj {

REGISTER_FILTER_E(BCJ,
    CCoder(false),
    CCoder(true),
    0x3030103, "BCJ")

}}
=======
// BcjRegister.cpp

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "BcjCoder.h"

namespace NCompress {
namespace NBcj {

REGISTER_FILTER_E(BCJ,
    CCoder(false),
    CCoder(true),
    0x3030103, "BCJ")

}}
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
void registerBCJ(){}