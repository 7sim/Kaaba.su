#include "Movement.hpp"
#include "../../SDK/variables.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Classes/weapon.hpp"

#include "../Rage/AntiAim.hpp"
#include "../Rage/FakeLag.hpp"
#include "../Rage/EnginePrediction.hpp"
#include "../Rage/ServerAnimations.hpp"
#include "../Rage/AlternativeAttack.hpp"
#include "../Rage/TickbaseShift.hpp"

Movement g_Movement;

void Movement::FixMovement( CUserCmd *cmd, QAngle wishangle ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal ) {
		return;
	}

	Vector view_fwd, view_right, view_up, cmd_fwd, cmd_right, cmd_up;
	Math::AngleVectors( wishangle, view_fwd, view_right, view_up );
	Math::AngleVectors( cmd->viewangles, cmd_fwd, cmd_right, cmd_up );

	const auto v8 = sqrtf( ( view_fwd.x * view_fwd.x ) + ( view_fwd.y * view_fwd.y ) );
	const auto v10 = sqrtf( ( view_right.x * view_right.x ) + ( view_right.y * view_right.y ) );
	const auto v12 = sqrtf( view_up.z * view_up.z );

	const Vector norm_view_fwd( ( 1.f / v8 ) * view_fwd.x, ( 1.f / v8 ) * view_fwd.y, 0.f );
	const Vector norm_view_right( ( 1.f / v10 ) * view_right.x, ( 1.f / v10 ) * view_right.y, 0.f );
	const Vector norm_view_up( 0.f, 0.f, ( 1.f / v12 ) * view_up.z );

	const auto v14 = sqrtf( ( cmd_fwd.x * cmd_fwd.x ) + ( cmd_fwd.y * cmd_fwd.y ) );
	const auto v16 = sqrtf( ( cmd_right.x * cmd_right.x ) + ( cmd_right.y * cmd_right.y ) );
	const auto v18 = sqrtf( cmd_up.z * cmd_up.z );

	const Vector norm_cmd_fwd( ( 1.f / v14 ) * cmd_fwd.x, ( 1.f / v14 ) * cmd_fwd.y, 0.f );
	const Vector norm_cmd_right( ( 1.f / v16 ) * cmd_right.x, ( 1.f / v16 ) * cmd_right.y, 0.f );
	const Vector norm_cmd_up( 0.f, 0.f, ( 1.f / v18 ) * cmd_up.z );

	const auto v22 = norm_view_fwd.x * cmd->forwardmove;
	const auto v26 = norm_view_fwd.y * cmd->forwardmove;
	const auto v28 = norm_view_fwd.z * cmd->forwardmove;
	const auto v24 = norm_view_right.x * cmd->sidemove;
	const auto v23 = norm_view_right.y * cmd->sidemove;
	const auto v25 = norm_view_right.z * cmd->sidemove;
	const auto v30 = norm_view_up.x * cmd->upmove;
	const auto v27 = norm_view_up.z * cmd->upmove;
	const auto v29 = norm_view_up.y * cmd->upmove;

	cmd->forwardmove = ( ( ( ( norm_cmd_fwd.x * v24 ) + ( norm_cmd_fwd.y * v23 ) ) + ( norm_cmd_fwd.z * v25 ) )
						 + ( ( ( norm_cmd_fwd.x * v22 ) + ( norm_cmd_fwd.y * v26 ) ) + ( norm_cmd_fwd.z * v28 ) ) )
		+ ( ( ( norm_cmd_fwd.y * v30 ) + ( norm_cmd_fwd.x * v29 ) ) + ( norm_cmd_fwd.z * v27 ) );
	cmd->sidemove = ( ( ( ( norm_cmd_right.x * v24 ) + ( norm_cmd_right.y * v23 ) ) + ( norm_cmd_right.z * v25 ) )
					  + ( ( ( norm_cmd_right.x * v22 ) + ( norm_cmd_right.y * v26 ) ) + ( norm_cmd_right.z * v28 ) ) )
		+ ( ( ( norm_cmd_right.x * v29 ) + ( norm_cmd_right.y * v30 ) ) + ( norm_cmd_right.z * v27 ) );
	cmd->upmove = ( ( ( ( norm_cmd_up.x * v23 ) + ( norm_cmd_up.y * v24 ) ) + ( norm_cmd_up.z * v25 ) )
					+ ( ( ( norm_cmd_up.x * v26 ) + ( norm_cmd_up.y * v22 ) ) + ( norm_cmd_up.z * v28 ) ) )
		+ ( ( ( norm_cmd_up.x * v30 ) + ( norm_cmd_up.y * v29 ) ) + ( norm_cmd_up.z * v27 ) );

	wishangle = cmd->viewangles;

	CorrectMoveButtons( cmd, pLocal->m_MoveType( ) == MOVETYPE_LADDER );
}

void Movement::CorrectMoveButtons( CUserCmd *cmd, bool ladder ) {
	if( !g_Vars.rage.fake_lag_fix_leg_movement )
		return;

	// strip old flags.
	cmd->buttons &= ~( IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT );

	// set new button flags.
	if( cmd->forwardmove > ( ladder ? 200.f : 0.f ) )
		cmd->buttons |= IN_FORWARD;

	else if( cmd->forwardmove < ( ladder ? -200.f : 0.f ) )
		cmd->buttons |= IN_BACK;

	if( cmd->sidemove > ( ladder ? 200.f : 0.f ) )
		cmd->buttons |= IN_MOVERIGHT;

	else if( cmd->sidemove < ( ladder ? -200.f : 0.f ) )
		cmd->buttons |= IN_MOVELEFT;
}

void Movement::InstantStop( CUserCmd *cmd ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	Vector vel = pLocal->m_vecVelocity( );
	float speed = vel.Length2D( );

	auto weapon = ( C_WeaponCSBaseGun * )pLocal->m_hActiveWeapon( ).Get( );
	if( !weapon )
		return;

	auto weapon_data = weapon->GetCSWeaponData( ).Xor( );
	if( !weapon_data )
		return;

	if( speed < 18.f ) {
		cmd->forwardmove = 0.f;
		cmd->sidemove = 0.f;
		return;
	}

	static auto sv_accelerate = g_pCVar->FindVar( XorStr( "sv_accelerate" ) );
	float accel = sv_accelerate->GetFloat( );
	float max_speed = weapon_data->m_flMaxSpeed;
	if( pLocal->m_bIsScoped( ) ) {
		max_speed = weapon_data->m_flMaxSpeed2;
	}

	if( pLocal->m_fFlags( ) & FL_DUCKING ) {
		max_speed /= 3.f;
		accel /= 3.f;
	}

	float surf_friction = 1.f;
	float max_accelspeed = accel * g_pGlobalVars->interval_per_tick * max_speed * surf_friction;

	float wishspeed{ };

	if( speed - max_accelspeed <= -1.f ) {
		wishspeed = max_accelspeed / ( speed / ( accel * g_pGlobalVars->interval_per_tick ) );
	}
	else {
		wishspeed = max_accelspeed;
	}

	QAngle ndir;
	Math::VectorAngles( vel * -1.f, ndir );
	ndir.y = cmd->viewangles.y - ndir.y;
	Vector adir;
	Math::AngleVectors( ndir, adir );

	cmd->forwardmove = adir.x * wishspeed;
	cmd->sidemove = adir.y * wishspeed;
}

bool Movement::GetClosestPlane( Vector *plane ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return false;

	CGameTrace trace;
	CTraceFilterWorldOnly filter;

	auto start = pLocal->m_vecOrigin( ), mins = pLocal->OBBMins( ), maxs = pLocal->OBBMaxs( );

	Vector planes;
	auto count = 0;

	for( auto step = 0.f; step <= M_PI * 2.f; step += M_PI / 10.f ) {
		auto end = start;

		end.x += cos( step ) * 64.f;
		end.y += sin( step ) * 64.f;

		g_pEngineTrace->TraceRay( Ray_t( start, end, mins, maxs ), MASK_PLAYERSOLID, &filter, &trace );

		if( trace.fraction < 1.f ) {
			planes += trace.plane.normal;
			count++;
		}
	}

	planes /= count;

	if( planes.z < 0.1f ) {
		*plane = planes;
		return true;
	}

	return false;
}

bool Movement::WillHitObstacleInFuture( float predict_time, float step ) const {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return false;
	
	static auto sv_gravity = g_pCVar->FindVar( "sv_gravity" );
	static auto sv_jump_impulse = g_pCVar->FindVar( "sv_jump_impulse" );

	bool ground = pLocal->m_fFlags( ) & FL_ONGROUND;
	auto gravity_per_tick = sv_gravity->GetFloat( ) * g_pGlobalVars->interval_per_tick;

	auto start = pLocal->m_vecOrigin( ), end = start;
	auto velocity = pLocal->m_vecVelocity( );

	CGameTrace trace;
	CTraceFilterWorldOnly filter;

	auto predicted_ticks_needed = TIME_TO_TICKS( predict_time );
	auto velocity_rotation_angle = RAD2DEG( atan2( velocity.y, velocity.x ) );
	auto ticks_done = 0;

	if( predicted_ticks_needed <= 0 )
		return false;

	while( true ) 	{
		auto rotation_angle = velocity_rotation_angle + step;

		velocity.x = cos( DEG2RAD( rotation_angle ) ) * pLocal->m_vecVelocity( ).Length2D( );
		velocity.y = sin( DEG2RAD( rotation_angle ) ) * pLocal->m_vecVelocity( ).Length2D( );
		velocity.z = ground ? sv_jump_impulse->GetFloat( ) : velocity.z - gravity_per_tick;

		end += velocity * g_pGlobalVars->interval_per_tick;

		g_pEngineTrace->TraceRay( Ray_t( start, end, pLocal->OBBMins( ), pLocal->OBBMaxs( ) ), MASK_PLAYERSOLID, &filter, &trace );

		if( trace.fraction != 1.f && trace.plane.normal.z <= 0.9f || trace.startsolid || trace.allsolid )
			break;

		end = trace.endpos;
		end.z -= 2.f;

		g_pEngineTrace->TraceRay( Ray_t( trace.endpos, end, pLocal->OBBMins( ), pLocal->OBBMaxs( ) ), MASK_PLAYERSOLID, &filter, &trace );
		ground = ( trace.fraction < 1.f || trace.allsolid || trace.startsolid ) && trace.plane.normal.z >= 0.7f;

		if( ++ticks_done >= predicted_ticks_needed )
			return false;

		velocity_rotation_angle = rotation_angle;
	}

	return true;
}

void Movement::DoCircleStrafer( CUserCmd *cmd, float *circle_yaw ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( pLocal->IsDead( ) )
		return;

	const auto min_step = 2.25f;
	const auto max_step = 5.f;
	auto velocity_2d = pLocal->m_vecVelocity( ).Length2D( );

	auto ideal_strafe = std::clamp( Math::Clamp( ToDegrees( atan( 15.f / velocity_2d ) ), 0.0f, 90.0f ), min_step, max_step );
	auto predict_time = std::clamp( 320.f / velocity_2d, 0.35f, 1.f );

	auto step = ideal_strafe;

	while( true ) 	{
		if( !WillHitObstacleInFuture( predict_time, step ) || step > max_step )
			break;

		step += 0.2f;
	}

	if( step > max_step ) 	{
		step = ideal_strafe;

		while( true ) 		{
			if( !WillHitObstacleInFuture( predict_time, step ) || step < -min_step )
				break;

			step -= 0.2f;
		}

		if( step < -min_step ) 		{
			Vector plane;
			if( GetClosestPlane( &plane ) )
				step = -Math::AngleNormalize( *circle_yaw - RAD2DEG( atan2( plane.y, plane.x ) ) ) * 0.1f;
		}
		else
			step -= 0.2f;
	}
	else
		step += 0.2f;

	m_vecMovementAngles.yaw = *circle_yaw = Math::AngleNormalize( *circle_yaw + step );
	cmd->sidemove = copysign( 450.f, -step );
}

void Movement::PrePrediction( CUserCmd *cmd, bool *bSendPacket ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( pLocal->IsDead( ) )
		return;

	// bunny hop
	if( g_Vars.misc.bunnyhop ) {
		// holding space
		if( cmd->buttons & IN_JUMP ) {
			// valid movetype
			if( pLocal->m_MoveType( ) != MOVETYPE_LADDER &&
				pLocal->m_MoveType( ) != MOVETYPE_NOCLIP ) {
				// not on ground
				if( !( pLocal->m_fFlags( ) & FL_ONGROUND ) ) {
					// remove jump only when in air
					// will get added back the tick we're on ground
					cmd->buttons &= ~IN_JUMP;
				}
			}
		}
	}

	// auto strafers
	if( g_Vars.misc.autostrafer && !( cmd->buttons & IN_SPEED ) ) {
		// make sure we're not on ground
		if( !( pLocal->m_fFlags( ) & FL_ONGROUND ) ) {
			// valid movetype
			if( pLocal->m_MoveType( ) == MOVETYPE_WALK ) {
				// do the "strafes"
				static float flSideSwap = 1.0f;
				flSideSwap = -flSideSwap;

				// save velocity
				Vector vecVelocity = pLocal->m_vecVelocity( );
				vecVelocity.z = 0.0f;

				// save speed, compute the ideal strafe
				const float flSpeed = vecVelocity.Length2D( );
				const float flIdealStrafe = Math::Clamp( ToDegrees( atan( 15.f / flSpeed ) ), 0.0f, 90.0f );

				static auto circle_yaw = 0.f;
				bool bCircleStrafing = false;
				if( g_Vars.misc.autostrafer_circle ) {
					if( g_Vars.misc.autostrafer_circle_key.enabled ) {
						DoCircleStrafer( cmd, &circle_yaw );
						bCircleStrafing = true;
					}
				}

				if( !bCircleStrafing ) {
					circle_yaw = cmd->viewangles.y;

					// wasd strafer
					if( g_Vars.misc.autostrafer_wasd && ( cmd->forwardmove != 0.0f || cmd->sidemove != 0.0f ) ) {
						enum EDirections {
							FORWARDS = 0,
							BACKWARDS = 180,
							LEFT = 90,
							RIGHT = -90,
							BACK_LEFT = 135,
							BACK_RIGHT = -135
						};

						float flWishDirection{ };

						const bool bHoldingW = cmd->buttons & IN_FORWARD;
						const bool bHoldingA = cmd->buttons & IN_MOVELEFT;
						const bool bHoldingS = cmd->buttons & IN_BACK;
						const bool bHoldingD = cmd->buttons & IN_MOVERIGHT;

						if( bHoldingW ) {
							if( bHoldingA ) {
								flWishDirection += ( EDirections::LEFT / 2 );
							}
							else if( bHoldingD ) {
								flWishDirection += ( EDirections::RIGHT / 2 );
							}
							else {
								flWishDirection += EDirections::FORWARDS;
							}
						}
						else if( bHoldingS ) {
							if( bHoldingA ) {
								flWishDirection += EDirections::BACK_LEFT;
							}
							else if( bHoldingD ) {
								flWishDirection += EDirections::BACK_RIGHT;
							}
							else {
								flWishDirection += EDirections::BACKWARDS;
							}

							cmd->forwardmove = 0.f;
						}
						else if( bHoldingA ) {
							flWishDirection += EDirections::LEFT;
						}
						else if( bHoldingD ) {
							flWishDirection += EDirections::RIGHT;
						}

						m_vecMovementAngles.yaw += std::remainderf( flWishDirection, 360.f );
						cmd->sidemove = 0.f;
					}

					cmd->forwardmove = 0.f;

					// cba commenting after here
					static float flOldYaw = 0.f;
					const float flYawDelta = std::remainderf( m_vecMovementAngles.yaw - flOldYaw, 360.0f );
					const float flAbsYawDelta = abs( flYawDelta );
					flOldYaw = m_vecMovementAngles.yaw;

					if( flAbsYawDelta <= flIdealStrafe || flAbsYawDelta >= 30.f ) {
						auto angVelocityDirection = vecVelocity.ToEulerAngles( );
						auto flVelocityDelta = std::remainderf( m_vecMovementAngles.yaw - angVelocityDirection.yaw, 360.0f );

						if( flVelocityDelta <= flIdealStrafe || flSpeed <= 15.f ) {
							if( -( flIdealStrafe ) <= flVelocityDelta || flSpeed <= 15.f ) {
								m_vecMovementAngles.yaw += flSideSwap * flIdealStrafe;
								cmd->sidemove = g_Vars.cl_sidespeed->GetFloat( ) * flSideSwap;
							}
							else {
								m_vecMovementAngles.yaw = angVelocityDirection.yaw - flIdealStrafe;
								cmd->sidemove = g_Vars.cl_sidespeed->GetFloat( );
							}
						}
						else {
							m_vecMovementAngles.yaw = angVelocityDirection.yaw + flIdealStrafe;
							cmd->sidemove = -g_Vars.cl_sidespeed->GetFloat( );
						}
					}
					else if( flYawDelta > 0.0f ) {
						cmd->sidemove = -g_Vars.cl_sidespeed->GetFloat( );
					}
					else if( flYawDelta < 0.0f ) {
						cmd->sidemove = g_Vars.cl_sidespeed->GetFloat( );
					}

					//m_vecMovementAngles.Normalize( );
					//FixMovement( cmd, m_vecMovementAngles );
				}
			}
		}
	}

	if( g_Vars.misc.air_duck ) {
		if( !( pLocal->m_fFlags( ) & FL_ONGROUND ) && pLocal->m_MoveType()== MOVETYPE_WALK ) {
			cmd->buttons |= IN_DUCK;
		}
	}

	if( !g_TickbaseController.m_bRunningExploit )
		if( ( g_Vars.misc.quickstop && !g_Vars.globals.m_bShotAutopeek && pLocal->m_fFlags( ) & FL_ONGROUND && !( cmd->buttons & IN_JUMP ) && pLocal->m_vecVelocity( ).Length( ) > 1.0f ) ) {
			if( cmd->forwardmove == cmd->sidemove && cmd->sidemove == 0.0f )
				InstantStop( cmd );
		}

	g_Movement.FakeWalk( bSendPacket, cmd );
	g_AlternativeAttack.RunAttacks( bSendPacket, cmd );
}

void Movement::InPrediction( CUserCmd *cmd ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( pLocal->IsDead( ) )
		return;

	AutoPeek( cmd );

	// edge jump
	if( g_Vars.misc.edge_jump ) {
		bool bCanEdgeJump = true;
		if( g_Vars.misc.edge_jump_key.key > 0 && !g_Vars.misc.edge_jump_key.enabled )
			bCanEdgeJump = false;

		if( bCanEdgeJump ) {
			if( ( g_Prediction.m_fFlags & FL_ONGROUND ) && !( pLocal->m_fFlags( ) & FL_ONGROUND ) ) {
				cmd->buttons |= IN_JUMP;
			}
		}
	}
}

void Movement::PostPrediction( CUserCmd *cmd ) {

	// fix movement after all movement code has ran
	g_Movement.FixMovement( cmd, m_vecMovementAngles );
}

void Movement::AutoPeek( CUserCmd *cmd ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( pLocal->IsDead( ) )
		return;

	if( g_Vars.misc.autopeek && g_Vars.misc.autopeek_bind.enabled ) {
		if( ( pLocal->m_fFlags( ) & FL_ONGROUND ) ) {
			if( m_vecAutoPeekPos.IsZero( ) ) {
				m_vecAutoPeekPos = pLocal->GetAbsOrigin( );
			}
		}
	}
	else {
		m_vecAutoPeekPos = Vector( );
	}

	bool bPeek = true;
	if( g_Vars.misc.autopeek && g_Vars.globals.m_bShotAutopeek && !m_vecAutoPeekPos.IsZero( ) ) {
		cmd->buttons &= ~IN_JUMP;

		bPeek = false;

		auto delta = m_vecAutoPeekPos - pLocal->GetAbsOrigin( );
		/*m_vecMovementAngles = delta.ToEulerAngles( );

		cmd->forwardmove = g_Vars.cl_forwardspeed->GetFloat( );
		cmd->sidemove = 0.0f;*/
		auto angle = Math::CalcAngle( pLocal->GetAbsOrigin( ), m_vecAutoPeekPos );

		// fix direction by factoring in where we are looking.
		angle.y = m_vecMovementAngles.y - angle.y;

		Vector direction;
		Math::AngleVectors( angle, direction );

		auto stop = direction * -450.f;

		cmd->forwardmove = stop.x;
		cmd->sidemove = stop.y;

		if( delta.Length2D( ) <= std::fmaxf( 11.f, pLocal->m_vecVelocity( ).Length2D( ) * g_pGlobalVars->interval_per_tick ) ) {
			bPeek = true;
			g_Vars.globals.m_bShotAutopeek = false;
		}
	}
	else {
		g_Vars.globals.m_bShotAutopeek = false;
	}
}

void Movement::FakeWalk( bool *bSendPacket, CUserCmd *cmd ) {
	g_Vars.globals.m_bFakeWalking = false;
	g_AntiAim.m_bAllowFakeWalkFlick = false;

	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( pLocal->IsDead( ) )
		return;

	if( !g_Vars.misc.fakewalk )
		return;

	if( g_TickbaseController.m_bRunningExploit )
		return;

	static bool bWasMovingBeforeFakewalk = false;
	if( !g_Vars.misc.fakewalk_bind.enabled ) {
		bWasMovingBeforeFakewalk = pLocal->m_vecVelocity( ).Length2D( ) >= 0.1f;
		return;
	}

	if( !( pLocal->m_fFlags( ) & FL_ONGROUND ) )
		return;

	if( cmd->buttons & IN_JUMP )
		return;

	if( bWasMovingBeforeFakewalk ) {
		if( pLocal->m_vecVelocity( ).Length2D( ) >= 0.1f ) {
			InstantStop( cmd );
		}
		else {
			bWasMovingBeforeFakewalk = false;
		}

		return;
	}

	if( !g_Movement.PressingMovementKeys( cmd ) )
		return;

	if( !( pLocal->m_fFlags( ) & FL_ONGROUND ) )
		return;

	auto pWeapon = ( C_WeaponCSBaseGun * )pLocal->m_hActiveWeapon( ).Get( );
	if( !pWeapon )
		return;

	cmd->buttons &= ~IN_SPEED;

	auto vecPredictedVelocity = pLocal->m_vecVelocity( );
	int nTicksToStop;
	for( nTicksToStop = 0; nTicksToStop < 15; ++nTicksToStop ) {
		if( vecPredictedVelocity.Length2D( ) < 0.1f )
			break;

		// predict velocity into the future
		if( vecPredictedVelocity.Length( ) >= 0.1f ) {
			const float flSpeedToStop = std::max< float >( vecPredictedVelocity.Length( ), g_Vars.sv_stopspeed->GetFloat( ) );
			const float flStopTime = std::max< float >( g_pGlobalVars->interval_per_tick, g_pGlobalVars->frametime );
			vecPredictedVelocity *= std::max< float >( 0.f, vecPredictedVelocity.Length( ) - g_Vars.sv_friction->GetFloat( ) * flSpeedToStop * flStopTime / vecPredictedVelocity.Length( ) );
		}
	}

	const bool bIsMrX = strstr( g_pEngine->GetNetChannelInfo( )->GetAddress( ), XorStr( "178.32.80.148" ) );

	// change from 14 to 16 once we bypass the 14 tick choke limit
	float m_flMultiplier = bIsMrX ? 0.12f : 0.14f;

	const int nTicksTillRevolverFire = INT_MAX;// TIME_TO_TICKS( pWeapon->m_flPostponeFireReadyTime( ) - TICKS_TO_TIME( pLocal->m_nTickBase( ) ) ) - 1;
	const int nTicksTillUpdate = INT_MAX;// TIME_TO_TICKS( g_ServerAnimations.m_uServerAnimations.m_flLowerBodyRealignTimer - TICKS_TO_TIME( pLocal->m_nTickBase( ) ) ) - 1;
	const int nFakewalkTicks = int( std::min( 100.f, ( g_Vars.misc.slow_motion_speed * 0.7f ) + 15.f ) * m_flMultiplier );
	const int nMaxChokeTicks = Math::LimitLowest( nFakewalkTicks, nTicksTillUpdate, nTicksTillRevolverFire < -1 ? INT_MAX : nTicksTillRevolverFire );
	const int choked = g_pClientState->m_nChokedCommands( );
	const int nTicksLeftToStop = nMaxChokeTicks - g_pClientState->m_nChokedCommands( );

	if( g_pClientState->m_nChokedCommands( ) < nMaxChokeTicks || nTicksToStop ) {
		*bSendPacket = false;
	}

	if( !pLocal->m_vecVelocity( ).Length2D( ) && !*bSendPacket && g_pClientState->m_nChokedCommands( ) > nMaxChokeTicks ) {
		*bSendPacket = true;
	}

	if( nTicksToStop > nTicksLeftToStop - 1 || !g_pClientState->m_nChokedCommands( ) ) {
		InstantStop( cmd );

		g_AntiAim.m_bAllowFakeWalkFlick = true;
	}

	g_Vars.globals.m_bFakeWalking = true;
}

void Movement::MovementControl( CUserCmd *cmd, float velocity ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( velocity <= 0.52f )
		cmd->buttons |= IN_SPEED;
	else
		cmd->buttons &= ~IN_SPEED;

	float movement_speed = std::sqrtf( cmd->forwardmove * cmd->forwardmove + cmd->sidemove * cmd->sidemove );
	if( movement_speed > 0.f ) {
		if( movement_speed > velocity ) {
			float mov_speed = pLocal->m_vecVelocity( ).Length2D( );
			if( ( velocity + 1.0f ) <= mov_speed ) {
				cmd->forwardmove = 0.0f;
				cmd->sidemove = 0.0f;
			}
			else {
				float forward_ratio = cmd->forwardmove / movement_speed;
				float side_ratio = cmd->sidemove / movement_speed;

				cmd->forwardmove = forward_ratio * std::min( movement_speed, velocity );
				cmd->sidemove = side_ratio * std::min( movement_speed, velocity );
			}
		}
	}
}

void Movement::LagWalk( CUserCmd *pCmd, bool *bSendPacket ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal ) {
		m_bLagWalking = false;
		return;
	}

	if( !g_Vars.misc.lag_walk ) {
		m_bLagWalking = false;
		return;
	}

	if( !g_Vars.misc.lag_walk_key.enabled ) {
		m_bLagWalking = false;
		return;
	}

	if( g_pClientState->m_nChokedCommands( ) < 100 )
		*bSendPacket = false;

	m_bLagWalking = true;
}