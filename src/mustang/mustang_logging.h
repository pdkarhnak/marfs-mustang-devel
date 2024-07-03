#ifndef __MUSTANG_LOGGING__
#define __MUSTANG_LOGGING__

#ifndef DEBUG_MUSTANG
#define DEBUG_MUSTANG 1
#endif

#define ID_MASK 0xFFFFFFFF
#define SHORT_ID(id) (id & ID_MASK)

#endif
