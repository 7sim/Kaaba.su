#pragma once
#include "../../source.hpp"

class Movement {
public:
	Vector m_vecAutoPeekPos;
	QAngle m_vecMovementAngles;
	bool m_bLagWalking;

	bool PressingMovementKeys( CUserCmd* cmd ) {
		return ( cmd->buttons & IN_MOVELEFT ) 
			|| ( cmd->buttons & IN_MOVERIGHT ) 
			|| ( cmd->buttons & IN_FORWARD ) 
			|| ( cmd->buttons & IN_BACK );
	}

	void FixMovement( CUserCmd* cmd, QAngle wish_angles );
	void CorrectMoveButtons(CUserCmd* cmd, bool ladder = false);
	void InstantStop( CUserCmd* cmd );
	bool GetClosestPlane( Vector *plane );
	bool WillHitObstacleInFuture( float predict_time, float step ) const;
	void DoCircleStrafer( CUserCmd *cmd, float *circle_yaw );
	void PrePrediction( CUserCmd* cmd, bool* bSendPacket );
	void InPrediction( CUserCmd* cmd );
	void PostPrediction( CUserCmd* cmd );

	void AutoPeek( CUserCmd* cmd );
	void FakeWalk( bool* bSendPacket, CUserCmd* cmd );
	void MovementControl( CUserCmd* cmd, float velocity );
	void LagWalk( CUserCmd* pCmd, bool* bSendPacket );
};

extern Movement g_Movement;