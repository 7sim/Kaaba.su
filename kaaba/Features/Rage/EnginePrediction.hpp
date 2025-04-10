#pragma once

#include "../../SDK/sdk.hpp"
#include <deque>

class C_CSPlayer;

struct CompressionData_t {
	int m_nTickbase = 0;
	QAngle m_aimPunchAngle = { };
	QAngle m_aimPunchAngleVel = { };

	Vector m_vecViewOffset = { };
	float m_flVelocityModifier = 0.f;
};

class Prediction {
public:
	void PreThink( CUserCmd* pCmd );
	void Think( CUserCmd* pCmd );
	void PostThink( CUserCmd* pCmd, bool *bSendPacket );

	void CorrectViewModel( ClientFrameStage_t stage );

	void OnPacketStart( void* ecx, int incoming_sequence, int outgoing_acknowledged );

	void StoreCompressionNetvars( );
	void ApplyCompressionNetvars( );

	int m_nLastFrameStage;
	int m_nLastTickCount;

	// backup variables
	float m_flCurtime;
	float m_flFrametime;
	float m_flSpread;
	float m_flInaccuracy;

	int m_fFlags;

	Vector m_vecVelocity;

private:
	CMoveData m_uMoveData;

	CUserCmd* m_pLastCmd;

	float m_flViewmodelCycle;
	float m_flViewmodelAnimTime;

	std::vector<int> m_nQueuedCommands;

	CompressionData_t m_CompressionData[ 150 ] = { };
};

extern Prediction g_Prediction;