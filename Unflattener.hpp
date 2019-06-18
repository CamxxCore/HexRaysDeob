#pragma once
#include <map>
#include <hexrays.hpp>
#include "CFFlattenInfo.hpp"
#include "DefUtil.hpp"
#include "TargetUtil.hpp"

struct CFUnflattener : public optblock_t
{
	CFFlattenInfo cfi;
	MovChain m_DeferredErasuresLocal;
	MovChain m_PerformedErasuresGlobal;
	bool bLastChance;
	bool bStop;

	void Clear(bool bFree)
	{
		cfi.Clear(bFree);
		m_DeferredErasuresLocal.clear();
		m_PerformedErasuresGlobal.clear();
		bLastChance = false;
		bStop = false;
	}

	CFUnflattener() { Clear(false); };
	virtual ~CFUnflattener() { Clear(true); }
	int idaapi func(mblock_t *blk);
	mblock_t *GetDominatedClusterHead(mbl_array_t *mba, int iDispPred, int &iClusterHead);
	int FindBlockTargetOrLastCopy(mblock_t *mb, mblock_t *mbClusterHead, mop_t *what, bool bAllowMultiSuccs, bool bRecursive);
	bool HandleTwoPreds(mblock_t *mb, mblock_t *mbClusterHead, mop_t *opCopy, mblock_t *&endsWithJcc, mblock_t *&nonJcc, int &actualGotoTarget, int &actualJccTarget);
	void CopyOrAppendMinsns(mblock_t * src, mblock_t *& dst);
	void CopyMblock(DeferredGraphModifier &dgm, mblock_t * src, mblock_t *& dst);
	void UpdateDestBlockNumber(DeferredGraphModifier & dgm, mblock_t * mb, int oldDest, int newDest);
	int CopyAndConnectBlocksToPred(DeferredGraphModifier &dgm, mblock_t *mb, mblock_t *&pred, int iDest);
	void CorrectStopBlockPreds(DeferredGraphModifier & dgm, mbl_array_t * mba, intvec_t stopPreds);
	void DisconnectBlockFromPred(DeferredGraphModifier &dgm, mblock_t *mb, mblock_t *&pred, int iDest);
	int PostHandleTwoPreds(DeferredGraphModifier &dgm, mblock_t *&mb, int actualGotoTargetOld, int actualGotoTargetNew, mblock_t *&nonJcc, int actualJccTarget);
	bool FindJccInFirstBlocks(mbl_array_t *mba, mop_t *&opCopy, mblock_t *&endsWithJcc, mblock_t *&nonJcc, int &actualGotoTarget, int &actualJccTarget);
	void ProcessErasures(mbl_array_t *mba);
	void CheckInterr50860(mblock_t * mb);
};