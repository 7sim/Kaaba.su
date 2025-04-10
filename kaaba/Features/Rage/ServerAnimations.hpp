#pragma once
#include "../../SDK/sdk.hpp"
#include "../Rage/LagCompensation.hpp"
#include <optional>

class ServerAnimations {
	struct AnimationInfo_t {
		std::array<C_AnimationLayer, 13> m_pAnimOverlays;
		std::array<float, 20> m_pPoseParameters;

		bool m_bRealignBreaker;

		float m_flEyeYaw;
		float m_flFootYaw;
		float m_flLowerBodyYawTarget;
		float m_flLowerBodyRealignTimer;
		float m_flLastValidDelta;
		float m_flSpawnTime;

		CCSGOPlayerAnimState m_pAnimState;
		bool m_bBreakingTeleportDst;

		Vector m_vecBonePos[ 256 ];
		Quaternion m_quatBoneRot[ 256 ];

		QAngle m_angUpdateAngles;

		alignas( 16 ) matrix3x4_t m_pMatrix[ 128 ];

		Vector m_vecLastOrigin;
	};

	bool IsModifiedLayer( int nLayer );

	float GetLayerIdealWeightFromSeqCycle( CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer );
	bool IsLayerSequenceCompleted( CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer );
	Activity GetLayerActivity( CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer );

	void SetLayerInactive( C_AnimationLayer* pLayer, int idx );

public:
	int SelectWeightedSequenceFromModifiers( C_CSPlayer* pEntity, int32_t activity, CUtlVector<uint16_t> modifiers );
private:
	void IncrementLayerCycle( CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer, bool bAllowLoop );
	void IncrementLayerWeight( CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer );
	void IncrementLayerCycleWeightRateGeneric( CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer );
	void SetLayerSequence( C_CSPlayer* pEntity, C_AnimationLayer* pLayer, int32_t activity, CUtlVector<uint16_t> modifiers, int nOverrideSequence = -1 );
	void HandleAnimationEvents( C_CSPlayer* pLocal, CCSGOPlayerAnimState* pState, C_AnimationLayer* layers, CUtlVector<uint16_t> uModifiers, CUserCmd* cmd );

public:
	AnimationInfo_t m_uCurrentAnimations;
	AnimationInfo_t m_uServerAnimations;
	QAngle m_angChokedShotAngle;

	void HandleServerAnimation( bool* bSendPacket, CUserCmd* pCmd );
	void HandleAnimations( bool* bSendPacket, CUserCmd* cmd );
};

extern ServerAnimations g_ServerAnimations;