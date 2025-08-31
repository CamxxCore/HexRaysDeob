#pragma once
#include <hexrays.hpp>

struct ObfCompilerOptimizer : public optinsn_t
{
	int idaapi func( mblock_t* blk, minsn_t* ins, int optflags ) {
		return 0;
	}
};
