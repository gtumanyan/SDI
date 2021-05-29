<<<<<<< HEAD
// Windows/Defs.h

#ifndef __WINDOWS_DEFS_H
#define __WINDOWS_DEFS_H

#include "../Common/MyWindows.h"

#ifdef _WIN32
inline bool LRESULTToBool(LRESULT v) { return (v != FALSE); }
inline bool BOOLToBool(BOOL v) { return (v != FALSE); }
inline BOOL BoolToBOOL(bool v) { return (v ? TRUE: FALSE); }
#endif

inline VARIANT_BOOL BoolToVARIANT_BOOL(bool v) { return (v ? VARIANT_TRUE: VARIANT_FALSE); }
inline bool VARIANT_BOOLToBool(VARIANT_BOOL v) { return (v != VARIANT_FALSE); }

#endif
=======
// Windows/Defs.h

#ifndef __WINDOWS_DEFS_H
#define __WINDOWS_DEFS_H

#include "../Common/MyWindows.h"

#ifdef _WIN32
inline bool LRESULTToBool(LRESULT v) { return (v != FALSE); }
inline bool BOOLToBool(BOOL v) { return (v != FALSE); }
inline BOOL BoolToBOOL(bool v) { return (v ? TRUE: FALSE); }
#endif

inline VARIANT_BOOL BoolToVARIANT_BOOL(bool v) { return (v ? VARIANT_TRUE: VARIANT_FALSE); }
inline bool VARIANT_BOOLToBool(VARIANT_BOOL v) { return (v != VARIANT_FALSE); }

#endif
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
