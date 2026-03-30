// Platform-specific forwarding
// mf.dll does not have a stable export table between versions of Windows

#if MFSTUB_TARGET_NT60
	#include "Forwards_60.h"
#elif MFSTUB_TARGET_NT61
	#include "Forwards_61.h"
#elif MFSTUB_TARGET_NT63
	#include "Forwards_63.h"
#elif MFSTUB_TARGET_NTBC
	#include "Forwards_BC.h"
#endif
