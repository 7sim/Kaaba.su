#pragma once
#include "../../SDK/sdk.hpp"

class AdaptiveAngle {
public:
	float m_yaw;
	float m_dist;

public:
	// ctor.
	__forceinline AdaptiveAngle( float yaw, float penalty = 0.f ) {
		// set yaw.
		m_yaw = Math::AngleNormalize( yaw );

		// init distance.
		m_dist = 0.f;

		// remove penalty.
		m_dist -= penalty;
	}
};

enum ESide {
	SIDE_LEFT,
	SIDE_RIGHT,
	SIDE_BACK,
	SIDE_MAX
};

class AntiAim {
public:
	void Think( CUserCmd* pCmd, bool* bSendPacket, bool* bFinalPacket );
	ESide GetSide( ) { return m_eSide; }

	bool DoEdgeAntiAim( C_CSPlayer* player, QAngle& out );
	C_CSPlayer *GetBestPlayer( );
	float UpdateFreestandPriority( float flLength, int nIndex );
private:
	void DoFakeYaw( CUserCmd* pCmd, C_CSPlayer* pLocal );
	void DoRealYaw( CUserCmd* pCmd, C_CSPlayer* pLocal );
	void DoPitch( CUserCmd* pCmd, C_CSPlayer* pLocal );
	bool IsAbleToFlick( C_CSPlayer* pLocal );
	void AutoDirection( C_CSPlayer* player );
	void Distortion( CUserCmd* pCmd, C_CSPlayer* pLocal );
	void PerformBodyFlick( CUserCmd *pCmd, bool *bSendPacket, float flOffset = 0.f );
	void HandleManual( CUserCmd* pCmd );
	void BreakLastBody( CUserCmd* pCmd, C_CSPlayer* pLocal, bool* bSendPacket );
private:
	float m_flLastRealAngle;
	bool m_bEdging;
	ESide m_eAutoSide;
	float m_flAutoYaw;
	float m_flAutoDist;
	float m_flAutoTime;
	bool m_bHasValidAuto;
	bool m_bLastPacket;
	bool m_bDistorting;
	int  m_nBodyFlicks;
	bool m_bJitterUpdate;

	ESide m_eBreakerSide = SIDE_MAX;
	ESide m_eSide = SIDE_MAX;
public:
	QAngle angViewangle;
	QAngle angRadarAngle;
	float  flLastMovingLby;
	bool m_bAllowFakeWalkFlick;
};

extern AntiAim g_AntiAim;