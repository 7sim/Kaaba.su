#pragma once
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Valve/CBaseHandle.hpp"

#include <map>
#include <deque>

// forward declaration
class CServerSetupBones;

struct SimulationRestore {
	int m_fFlags;
	float m_flDuckAmount;
	float m_flFeetCycle;
	float m_flMoveWeight;
	QAngle m_angEyeAngles;
	Vector m_vecOrigin;
};

class AnimationRecord {
public:
	C_CSPlayer* m_pEntity;

	bool m_bIsResolved;
	bool m_bLBYFlicked;
	bool m_bBrokeTeleportDst;
	bool m_bIsInvalid;
	bool m_bIsFakewalking;

	Vector m_vecOrigin;
	Vector m_vecVelocity;
	QAngle m_angEyeAngles;

	int m_fFlags;
	int m_iChokeTicks;

	float m_flLastNonShotPitch;
	float m_flChokeTime;
	float m_flSimulationTime;
	float m_flAnimationTime;
	float m_flLowerBodyYawTarget;
	float m_flDuckAmount;
	float m_flDelta;

	float m_flMoveWeight;
	float m_flFeetCycle;

	alignas( 16 ) matrix3x4_t m_pMatrix[128];

	std::array<C_AnimationLayer, 13> m_pServerAnimOverlays;
};

class AnimationData {
public:
	void Update( );
	void Collect( C_CSPlayer* pPlayer );

	void UpdatePlayer( C_CSPlayer* pEntity, AnimationRecord* pRecord, AnimationRecord *pPreviousRecord, AnimationRecord *pPenultimateRecord );
	void SimulateMovement( C_CSPlayer* pEntity, Vector& vecOrigin, Vector& vecVelocity, int& fFlags, bool bOnGround );
	void SimulateAnimations( AnimationRecord *pRecord, AnimationRecord *pPreviousRecord, AnimationRecord *pPenultimateRecord );

	C_CSPlayer* player;

	float m_flSimulationTime;
	float m_flOldSimulationTime;

	struct 
	{
		float m_flCycle;
		float m_flPlaybackRate;
	} m_uLayerAliveLoopData;

	bool m_bWasDormant = false;
	bool m_bUpdated = false;

	bool m_bIsAlive = false;

	std::deque<AnimationRecord> m_AnimationRecord;
};

class AnimationSystem {
public:
	void FrameStage( );
	AnimationData* GetAnimationData( int index );

	bool m_bInBoneSetup[ 65 ];
private:
	std::map<int, AnimationData> m_AnimatedEntities = { };
};

extern AnimationSystem g_AnimationSystem;