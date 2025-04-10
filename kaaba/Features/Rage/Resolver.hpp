#pragma once
#include "LagCompensation.hpp"
#include <vector>
#include <deque>

enum EResolverStages {
	RESOLVE_MODE_NONE, // not currently resolving
	RESOLVE_MODE_STAND,
	RESOLVE_MODE_MOVE,
	RESOLVE_MODE_AIR,
	RESOLVE_MODE_PRED,
	RESOLVE_MODE_LOGIC,
	RESOLVE_MODE_OVERRIDE
};

enum EBodyBreakTypes {
	BREAK_NONE,
	BREAK_HIGH,
	BREAK_LOW
};

//enum EResolverFlags {
//	FLAG_NONE = ( 1 << 0 ),
//	FLAG_OVERRIDING = ( 1 << 1 ),
//	FLAG_IN_PRED_STAGE = ( 1 << 2 ),
//	FLAG_LBY_NO_UPD = ( 1 << 3 ),
//};

struct ResolverData_t {
	void ResetMoveData( ) {
		if( !m_vecLastMoveOrigin.IsZero( ) )
			m_vecLastMoveOrigin.Init( );

		if( m_flLastMoveBody != FLT_MAX )
			m_flLastMoveBody = FLT_MAX;

		if( m_flLastMoveAnimTime != FLT_MAX )
			m_flLastMoveAnimTime = FLT_MAX;
	}

	void ResetPredData( ) {
		if( m_flLowerBodyRealignTimer != FLT_MAX )
			m_flLowerBodyRealignTimer = FLT_MAX;

		if( m_flAnimationTime != FLT_MAX )
			m_flAnimationTime = FLT_MAX;

		if( m_flPreviousLowerBodyYaw != FLT_MAX )
			m_flPreviousLowerBodyYaw = FLT_MAX;

		if( m_bInPredictionStage != false )
			m_bInPredictionStage = false;

		if( m_flLastUpdateTime != FLT_MAX )
			m_flLastUpdateTime = FLT_MAX;
	}

	void ResetDetectData( ) {
		if( m_flLastUpdateTime != FLT_MAX )
			m_flLastUpdateTime = FLT_MAX;

		if( m_flLastWeightTriggerTime != FLT_MAX )
			m_flLastWeightTriggerTime = FLT_MAX;

		if( m_flLastCycleTriggerTime != FLT_MAX )
			m_flLastCycleTriggerTime = FLT_MAX;

		if( m_eBodyBreakType != BREAK_NONE )
			m_eBodyBreakType = BREAK_NONE;	
		
		if( m_bBreakingLowerBody != BREAK_NONE )
			m_bBreakingLowerBody = BREAK_NONE;

		if( m_bHasntUpdated != false )
			m_bHasntUpdated = false;
	}

	void ResetLogicData( ) {
		if( m_bSuppressDetectionResolvers != false )
			m_bSuppressDetectionResolvers = false;

		if( m_bUsedFreestandPreviously != false )
			m_bUsedFreestandPreviously = false;

		if( m_bUsedAwayPreviously != false )
			m_bUsedAwayPreviously = false;

		//if( m_bInMoveBodyStage != false )
		//	m_bInMoveBodyStage = false;

		//if( m_bResetBodyLogic != false )
		//	m_bResetBodyLogic = false;

		//if( m_nPrevMissedMoveBodyShots != 0 )
		//	m_nPrevMissedMoveBodyShots = 0;

		//if( m_nPrevHitMoveBodyShots != 0 )
		//	m_nPrevHitMoveBodyShots = 0;
	}

	void ResetDesyncData( ) {
		if( m_bDesyncFlicking != false )
			m_bDesyncFlicking = false;	
		
		if( m_bConstantFrequency != false )
			m_bConstantFrequency = false;
		
		if( m_nFlickDetectionCount != 0 )
			m_nFlickDetectionCount = 0;
		
		if( m_nLastFlickTick != 0 )
			m_nLastFlickTick = 0;		
		
		if( m_nFlickDetectionFrequency != 0 )
			m_nFlickDetectionFrequency = 0;	
		
		if( m_nLastFlickDetectionFrequency != 0 )
			m_nLastFlickDetectionFrequency = 0;	
				
		if( m_nIdealFootYawSide != 0 )
			m_nIdealFootYawSide = 0;

		if( m_tLoggedPlaybackRates != std::make_tuple( FLT_MAX, FLT_MAX, FLT_MAX ) )
			m_tLoggedPlaybackRates = std::make_tuple( FLT_MAX, FLT_MAX, FLT_MAX );
	}

	void Reset( ) {
		if( m_flFreestandYaw != FLT_MAX )
			m_flFreestandYaw = FLT_MAX;

		if( m_iCurrentResolverType != 0 )
			m_iCurrentResolverType = 0;	
		
		if( m_bSuppressHeightResolver != false )
			m_bSuppressHeightResolver = false;

		if( m_bSuppressDetectionResolvers != false )
			m_bSuppressDetectionResolvers = false;

		if( m_bUsedFreestandPreviously != false )
			m_bUsedFreestandPreviously = false;

		if( m_bUsedAwayPreviously != false )
			m_bUsedAwayPreviously = false;

		if( m_bHasHeightAdvantage != false )
			m_bHasHeightAdvantage = false;	
		
		if( m_flLastReceivedCheatDataTime != 0.f )
			m_flLastReceivedCheatDataTime = 0.f;
		
		if( m_bMissedNetworkedAngle != false )
			m_bMissedNetworkedAngle = false;	
		
		if( m_bCameFromDeveloper != false )
			m_bCameFromDeveloper = false;	
		
		if( m_bOverriding != false )
			m_bOverriding = false;
		
		if( m_flServerYaw != FLT_MAX )
			m_flServerYaw = FLT_MAX;

		m_iMissedShotsDesync = m_iMissedShots = m_iMissedShotsLBY = m_iMissedShotsSpread = 0;
		m_eCurrentStage = RESOLVE_MODE_NONE;
		ResetMoveData( );
		ResetPredData( );
		ResetDetectData( );
		ResetDesyncData( );
	}

	EResolverStages m_eCurrentStage;

	int m_iCurrentResolverType = 0;

	int m_iMissedShots;
	int m_iMissedShotsDesync;
	int m_iMissedShotsSpread;
	int m_iMissedShotsLBY;
	int m_iMissedShotsHeight;

	//bool m_bIsOverriding = false;

	// last move data
	Vector m_vecLastMoveOrigin = { 0, 0, 0 };
	float m_flLastMoveBody = FLT_MAX;
	float m_flLastMoveAnimTime = FLT_MAX;

	// lby pred data
	float m_flPreviousLowerBodyYaw = FLT_MAX;
	//bool m_bInPredictionStage = false;
	float m_flLowerBodyRealignTimer = FLT_MAX;
	float m_flAnimationTime = FLT_MAX;

	// lby detect data
	bool m_bHasntUpdated = false;
	float m_flLastUpdateTime = FLT_MAX;
	float m_flLastWeightTriggerTime = FLT_MAX;
	float m_flLastCycleTriggerTime = FLT_MAX;
	bool m_bBreakingLowerBody = false;
	int m_eBodyBreakType = BREAK_NONE; // maybe we can use this for smth
	//bool m_bHasntUpdated = false;

	// "log" data
	bool m_bSuppressDetectionResolvers = false;
	bool m_bUsedFreestandPreviously = false;
	bool m_bUsedAwayPreviously = false;

	bool m_bHasHeightAdvantage = false;
	bool m_bSuppressHeightResolver = false;

	QAngle m_angEdgeAngle;

	bool m_bOverriding = false;
	bool m_bInPredictionStage = false;

	float m_flFreestandYaw = 0.f;

	bool m_bMissedNetworkedAngle = false;
	float m_flLastReceivedCheatDataTime = 0.f;
	float m_flLastReceivedkaabaDataTime = 0.f;
	bool m_bCameFromDeveloper = false;
	float m_flServerYaw = FLT_MAX;

	// dopium desync data
	int m_nIdealFootYawSide;
	bool m_bDesyncFlicking = false;
	bool m_bConstantFrequency = false;
	std::tuple<float, float, float> m_tLoggedPlaybackRates; // latest, prev, penult
	int m_nFlickDetectionCount = 0;
	int m_nFlickDetectionFrequency = 0;
	int m_nLastFlickDetectionFrequency = 0;
	int m_nLastFlickTick = 0;
};

class Resolver {
	void UpdateDesyncDetection( AnimationRecord *pRecord, AnimationRecord *pPreviousRecord, AnimationRecord *pPenultimateRecord );
	bool IsAnglePlausible( AnimationRecord* pRecord, float flAngle );

	void UpdateLBYPrediction( AnimationRecord* pRecord );
	void UpdateResolverStage( AnimationRecord* pRecord );
	void UpdateBodyDetection( AnimationRecord* pRecord, AnimationRecord* pPreviousRecord );

	void ResolveHeight( AnimationRecord* pRecord );
	void ResolveStand( AnimationRecord* pRecord, AnimationRecord* pPreviousRecord );
	void ResolveMove( AnimationRecord* pRecord );
	void ResolveAir( AnimationRecord* pRecord );
	void ResolveAirUntrusted( AnimationRecord* pRecord );
	void ResolvePred( AnimationRecord* pRecord );
	void ResolveLogic( AnimationRecord *pRecord );
	void ResolveBrute( AnimationRecord* pRecord );
	void ResolveOverride( AnimationRecord* pRecord );
	float ResolveAntiFreestand( AnimationRecord* pRecord );
	void OverrideResolver( AnimationRecord* pRecord );

	// creds to esoterik
	float SnapToClosestYaw( float a1 ) {
		if( !g_Vars.rage.antiaim_resolver_snap_to_closest_yaw )
			return a1;

		double v97 = 1.0;
		if( a1 < 0.0 ) {
			v97 = -1.0;
			a1 = -a1;
		}

		if( a1 < 23.0 ) {
			a1 = 0.0;
		}
		else if( a1 < 67.0 ) {
			a1 = 45.0;
		}
		else if( a1 < 113.0 ) {
			a1 = 90.0f;
		}
		else if( a1 < 157.0 ) {
			a1 = 135.0;
		}
		else {
			a1 = 180.0;
		}

		return ( a1 * v97 );
	};
public:
	void ResolvePlayers( AnimationRecord* pRecord, AnimationRecord *pPreviousRecord, AnimationRecord *pPenultimateRecord );
	std::array< ResolverData_t, 65 > m_arrResolverData;
};

extern Resolver g_Resolver;