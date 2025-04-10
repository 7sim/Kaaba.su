#include "AntiAim.hpp"

#include "../Miscellaneous/Movement.hpp"
#include "../Rage/FakeLag.hpp"
#include "../Visuals/Visuals.hpp"
#include "ServerAnimations.hpp"
#include "EnginePrediction.hpp"
#include "TickbaseShift.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "Autowall.hpp"
#include "../Visuals/EventLogger.hpp"

AntiAim g_AntiAim;

C_CSPlayer *AntiAim::GetBestPlayer( ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return false;

	float bestDist = FLT_MAX;
	float bestFov = FLT_MAX;
	C_CSPlayer *bestPlayer = nullptr;

	QAngle viewAngle;
	g_pEngine->GetViewAngles( viewAngle );

	for( int i = 1; i <= g_pGlobalVars->maxClients; i++ ) {
		auto player = C_CSPlayer::GetPlayerByIndex( i );
		if( !player || player->IsDead( ) )
			continue;

		if( player->IsDormant( ) )
			continue;

		if( player->IsTeammate( pLocal ) )
			continue;

		float fov = Math::GetFov( viewAngle, pLocal->GetEyePosition( ), player->m_vecOrigin( ) );

		if( fov < bestFov ) {
			bestFov = fov;
			bestPlayer = player;
		}
	}

	return bestPlayer;
}


float AntiAim::UpdateFreestandPriority( float flLength, int nIndex ) {
	float flReturn = 1.f;

	// over 50% of the total length, prioritize this shit.
	if( nIndex > ( flLength * XorFlt( 0.5f ) ) )
		flReturn = XorFlt( 1.25f );

	// over 90% of the total length, prioritize this shit.
	if( nIndex > ( flLength * XorFlt( 0.75f ) ) )
		flReturn = XorFlt( 1.5f );

	// over 90% of the total length, prioritize this shit.
	if( nIndex > ( flLength * XorFlt( 0.9f ) ) )
		flReturn = XorFlt( 2.f );

	return flReturn;
}

void AntiAim::AutoDirection( C_CSPlayer *pLocal ) {
	if( GetSide( ) != SIDE_MAX )
		return;

	// constants.
	const float flXored4 = XorFlt( 4.f );
	const float flXored32 = XorFlt( 32.f );
	const float flXored90 = XorFlt( 90.f );

	// get our view angles.
	QAngle view_angles;
	g_pEngine->GetViewAngles( view_angles );

	// get our shoot pos.
	Vector local_start = pLocal->GetEyePosition( );

	// best target.
	struct AutoTarget_t { float fov; C_CSPlayer *player; };
	AutoTarget_t target{ 180.f + 1.f, nullptr };

	// detect nearby walls.
	QAngle angEdgeAngle;
	auto bEdgeDetected = DoEdgeAntiAim( pLocal, angEdgeAngle );

	// iterate players.
	target.player = GetBestPlayer( );

	if( !target.player ) {
		// set angle to backwards.
		m_flAutoYaw = -1.f;
		m_flAutoDist = -1.f;
		m_bHasValidAuto = false;
		m_eAutoSide = ESide::SIDE_BACK;
		return;
	}

	// get target away angle.
	QAngle away = Math::CalcAngle( target.player->m_vecOrigin( ), pLocal->m_vecOrigin( ) );

	// construct vector of angles to test.
	std::vector< AdaptiveAngle > angles{ };
	angles.emplace_back( 180.f );
	angles.emplace_back( flXored90 );
	angles.emplace_back( -flXored90 );

	// start the trace at the enemy shoot pos.
	Vector start = target.player->GetEyePosition( );

	// see if we got any valid result.
	// if this is false the path was not obstructed with anything.
	bool valid{ false };

	// iterate vector of angles.
	for( auto it = angles.begin( ); it != angles.end( ); ++it ) {

		// compute the 'rough' estimation of where our head will be.
		Vector end{ local_start.x + std::cos( DEG2RAD( away.y + it->m_yaw ) ) * flXored32,
			local_start.y + std::sin( DEG2RAD( away.y + it->m_yaw ) ) * flXored32,
			local_start.z };

		// compute the direction.
		Vector dir = end - start;
		float len = dir.Normalize( );

		// should never happen.
		if( len <= 0.f )
			continue;

		// step thru the total distance, 4 units per step.
		for( float i{ 0.f }; i < len; i += flXored4 ) {
			// get the current step position.
			Vector point = start + ( dir * i );

			// get the contents at this point.
			int contents = g_pEngineTrace->GetPointContents( point, MASK_SHOT_HULL );

			// contains nothing that can stop a bullet.
			if( !( contents & MASK_SHOT_HULL ) )
				continue;

			// append 'penetrated distance'.
			it->m_dist += ( flXored4 * UpdateFreestandPriority( len, i ) );

			// mark that we found anything.
			valid = true;
		}
	}

	if( !valid /*|| !bEdgeDetected*/ ) {
		// set angle to backwards.
		m_flAutoYaw = Math::AngleNormalize( away.y + 180.f );
		m_flAutoTime = -1.f;
		m_bHasValidAuto = true;
		m_eAutoSide = ESide::SIDE_BACK;
		return;
	}

	// put the most distance at the front of the container.
	std::sort( angles.begin( ), angles.end( ),
			   [ ] ( const AdaptiveAngle &a, const AdaptiveAngle &b ) {
		return a.m_dist > b.m_dist;
	} );

	// the best angle should be at the front now.
	AdaptiveAngle *best = &angles.front( );

	// how long has passed sinec we've updated our angles?
	float last_update_time = g_pGlobalVars->curtime - m_flAutoTime;

	// check if we are not doing a useless change.
	if( best->m_dist != m_flAutoDist && last_update_time >= g_Vars.rage.anti_aim_edge_dtc_freestanding_delay ) {
		auto TranslateSide = [ & ] ( float a ) {
			if( a <= -flXored90 ) {
				return ESide::SIDE_RIGHT;
			}

			if( a >= flXored90 ) {
				return ESide::SIDE_LEFT;
			}

			return ESide::SIDE_BACK;
		};

		// set yaw to the best result.
		m_eAutoSide = TranslateSide( best->m_yaw );
		m_flAutoYaw = Math::AngleNormalize( away.y + best->m_yaw );
		m_flAutoDist = best->m_dist;
		m_flAutoTime = g_pGlobalVars->curtime;
		m_bHasValidAuto = true;
	}

	// shit fucking autowall it won't work
#if 0
	QAngle angAway = Math::CalcAngle( target.player->m_vecOrigin( ), pLocal->m_vecOrigin( ) );

	const float flRange = 36.f;
	float flBestYaw = Math::AngleNormalize( angAway.y );
	float flBestDamage = FLT_MAX;

	// -90.f, 180.f, +90.f
	for( int i = -1; i <= 1; ++i ) {
		const float flWishHeadPosition = angAway.y + Math::AngleNormalize( 180.f + ( 90.f * i ) );

		Vector vecHeadPosition(
			flRange * cos( DEG2RAD( flWishHeadPosition ) ) + pLocal->GetEyePosition( ).x,
			flRange * sin( DEG2RAD( flWishHeadPosition ) ) + pLocal->GetEyePosition( ).y,
			pLocal->GetEyePosition( ).z );

		g_pDebugOverlay->AddLineOverlay( pLocal->GetEyePosition( ), vecHeadPosition, 255, 255, 255, false, g_pGlobalVars->interval_per_tick * 4 );

		Vector vecDirection = vecHeadPosition - target.player->GetEyePosition( );
		vecDirection.Normalize( );

		Autowall::C_FireBulletData fireData;

		fireData.m_vecStart = target.player->GetEyePosition( );
		fireData.m_vecDirection = vecDirection;
		fireData.m_Player = target.player;
		fireData.m_TargetPlayer = nullptr; // it wont hit local

		bool bAutowalled = Autowall::FireBullets( &fireData );

		//printf( "%i - %f:%i\n", i, fireData.m_flCurrentDamage, int( bAutowalled ) );

		g_pDebugOverlay->AddLineOverlay( target.player->GetEyePosition( ), vecHeadPosition, 0, 0, 255, false, g_pGlobalVars->interval_per_tick * 4 );

		if( fireData.m_flCurrentDamage < flBestDamage /*&& bAutowalled*/ ) {
			flBestYaw = flWishHeadPosition;
			flBestDamage = fireData.m_flCurrentDamage;
		}
	}

	if( flBestDamage == FLT_MAX ) {
		m_flAutoYaw = Math::AngleNormalize( angAway.y + 180.f );
		m_flAutoTime = -1.f;
		m_bHasValidAuto = true;
		return;
	}

	// how long has passed sinec we've updated our angles?
	float last_update_time = g_pGlobalVars->curtime - m_flAutoTime;

	// check if we are not doing a useless change.
	if( last_update_time >= g_Vars.rage.anti_aim_edge_dtc_freestanding_delay ) {
		// set yaw to the best result.
		m_flAutoYaw = Math::AngleNormalize( flBestYaw );
		m_flAutoTime = g_pGlobalVars->curtime;
		m_bHasValidAuto = true;
	}
#endif
}

bool AntiAim::DoEdgeAntiAim( C_CSPlayer *player, QAngle &out ) {
	// if we use this for resolver ever, we should only prevent
	// this from running if we have manual aa enabled
	if( player && player->EntIndex( ) == g_pEngine->GetLocalPlayer( ) ) {
		if( GetSide( ) != SIDE_MAX )
			return false;
	}

	CGameTrace trace;
	static CTraceFilter filter{ };

	if( player->m_MoveType( ) == MOVETYPE_LADDER )
		return false;

	// skip this player in our traces.
	filter.pSkip = player;

	// get player bounds.
	Vector mins = player->OBBMins( );
	Vector maxs = player->OBBMaxs( );

	// Trololololololol
	const float flXored20 = XorFlt( 20.f );
	const float flXored32 = XorFlt( 32.f );
	const float flXored48 = XorFlt( 48.f );
	const float flXored1 = XorFlt( 1.f );
	const float flXored24 = XorFlt( 24.f );
	const float flXored90 = XorFlt( 90.f );
	const float flXored3 = XorFlt( 3.f );
	const float flXored8 = XorFlt( 8.f );

	// make player bounds bigger.
	mins.x -= flXored20;
	mins.y -= flXored20;
	maxs.x += flXored20;
	maxs.y += flXored20;

	// get player origin.
	Vector start = player->GetAbsOrigin( );

	// offset the view.
	start.z += ( flXored32 + flXored24 ); // 56.f

	g_pEngineTrace->TraceRay( Ray_t( start, start, mins, maxs ), CONTENTS_SOLID, ( ITraceFilter * )&filter, &trace );
	if( !trace.startsolid )
		return false;

	float  smallest = flXored1;
	Vector plane;

	// trace around us in a circle, in 20 steps (anti-degree conversion).
	// find the closest object.
	for( float step{ }; step <= ( M_PI * ( flXored1 + flXored1 /*LOL*/ ) ); step += ( M_PI / ( flXored8 + flXored1 + flXored1 /*HAHAHAHAAH*/ ) ) ) {
		// extend endpoint x units.
		Vector end = start;

		// set end point based on range and step.
		end.x += std::cos( step ) * flXored32;
		end.y += std::sin( step ) * flXored32;

		g_pEngineTrace->TraceRay( Ray_t( start, end, { -flXored1, -flXored1, -flXored8 }, { flXored1, flXored1, flXored8 } ), CONTENTS_SOLID, ( ITraceFilter * )&filter, &trace );

		// we found an object closer, then the previouly found object.
		if( trace.fraction < smallest ) {
			// save the normal of the object.
			plane = trace.plane.normal;
			smallest = trace.fraction;
		}
	}

	// no valid object was found.
	if( smallest == flXored1 || plane.z >= 0.1f )
		return false;

	// invert the normal of this object
	// this will give us the direction/angle to this object.
	Vector inv = -plane;
	Vector dir = inv;
	dir.Normalize( );

	// extend point into object by 24 units.
	Vector point = start;
	point.x += ( dir.x * flXored24 );
	point.y += ( dir.y * flXored24 );

	// check if we can stick our head into the wall.
	if( g_pEngineTrace->GetPointContents( point, CONTENTS_SOLID ) & CONTENTS_SOLID ) {
		// trace from 72 units till 56 units to see if we are standing behind something.
		g_pEngineTrace->TraceRay( Ray_t( point + Vector{ 0.f, 0.f, ( flXored8 * 2.f ) }, point ), CONTENTS_SOLID, ( ITraceFilter * )&filter, &trace );

		// we didnt start in a solid, so we started in air.
		// and we are not in the ground.
		if( trace.fraction < flXored1 && !trace.startsolid && trace.plane.normal.z > 0.7f ) {
			// mean we are standing behind a solid object.
			// set our angle to the inversed normal of this object.
			out.y = RAD2DEG( std::atan2( inv.y, inv.x ) );
			return true;
		}
	}

	// if we arrived here that mean we could not stick our head into the wall.
	// we can still see if we can stick our head behind/asides the wall.

	// adjust bounds for traces.
	mins = { ( dir.x * -flXored3 ) - flXored1, ( dir.y * -flXored3 ) - flXored1, -flXored1 };
	maxs = { ( dir.x * flXored3 ) + flXored1, ( dir.y * flXored3 ) + flXored1, flXored1 };

	// move this point 48 units to the left 
	// relative to our wall/base point.
	Vector left = start;
	left.x = point.x - ( inv.y * flXored48 );
	left.y = point.y - ( inv.x * -flXored48 );

	g_pEngineTrace->TraceRay( Ray_t( left, point, mins, maxs ), CONTENTS_SOLID, ( ITraceFilter * )&filter, &trace );
	float l = trace.startsolid ? 0.f : trace.fraction;

	// move this point 48 units to the right 
	// relative to our wall/base point.
	Vector right = start;
	right.x = point.x + ( inv.y * flXored48 );
	right.y = point.y + ( inv.x * -flXored48 );

	g_pEngineTrace->TraceRay( Ray_t( right, point, mins, maxs ), CONTENTS_SOLID, ( ITraceFilter * )&filter, &trace );
	float r = trace.startsolid ? 0.f : trace.fraction;

	// both are solid, no edge.
	if( l == 0.f && r == 0.f )
		return false;

	// set out to inversed normal.
	out.y = RAD2DEG( std::atan2( inv.y, inv.x ) );

	// left started solid.
	// set angle to the left.
	if( l == 0.f ) {
		out.y += flXored90;
		return true;
	}

	// right started solid.
	// set angle to the right.
	if( r == 0.f ) {
		out.y -= flXored90;
		return true;
	}

	return false;
}

void AntiAim::DoPitch( CUserCmd *pCmd, C_CSPlayer *pLocal ) {
	switch( g_Vars.rage.anti_aim_pitch ) {
		case 1:
			// down.
			pCmd->viewangles.x = 89.f;// g_Vars.misc.anti_untrusted ? 89.f : 720.f;
			break;
		case 2:
			// up.
			pCmd->viewangles.x = -89.f;// g_Vars.misc.anti_untrusted ? -89.f : -720.f;
		case 3:
			// zero.
			pCmd->viewangles.x = 0.f;
			break;
		case 4:
			// minimal.
			pCmd->viewangles.x = pLocal->m_PlayerAnimState( )->m_flAimPitchMin;
			break;
	}
}

void AntiAim::DoRealYaw( CUserCmd *pCmd, C_CSPlayer *pLocal ) {
	if( GetSide( ) != SIDE_MAX )
		return;

	if( g_Vars.rage.anti_aim_at_players ) {
		auto pBestPlayer = GetBestPlayer( );
		if( pBestPlayer ) {
			pCmd->viewangles.y = Math::CalcAngle( pBestPlayer->m_vecOrigin( ), pLocal->m_vecOrigin( ) ).y;
		}
	}

	const float flAdditive = m_bJitterUpdate ? -g_Vars.rage.anti_aim_yaw_jitter : g_Vars.rage.anti_aim_yaw_jitter;

	if( g_Vars.rage.anti_aim_yaw_running == 1 && pLocal->m_PlayerAnimState( )->m_flVelocityLengthXY > 0.1f && ( pLocal->m_fFlags( ) & FL_ONGROUND ) && ( g_Prediction.m_fFlags & FL_ONGROUND ) && !( pCmd->buttons & IN_JUMP ) ) {
		// this basically breaks last move lby resolvers/lby breaker detection (in theory it will prolly also break our lby detection lol)
		pCmd->viewangles.y += Math::AngleNormalize( 180.f + ( m_bJitterUpdate ? -55 : 55 ) );
		return;
	}

	switch( g_Vars.rage.anti_aim_yaw ) {
		case 0:
			break;
		case 1: // 180
			pCmd->viewangles.y += 179.f;
			break;
		case 2: // 180 jitter
			pCmd->viewangles.y += Math::AngleNormalize( 179.f + flAdditive );
			break;
		case 3: // jitter
			pCmd->viewangles.y += Math::AngleNormalize( m_bJitterUpdate ? g_Vars.rage.anti_aim_yaw_jitter : -g_Vars.rage.anti_aim_yaw_jitter );
			break;
		case 4: // Spin
			// set base angle.
			pCmd->viewangles.y -= ( ( float )g_Vars.rage.anti_aim_yaw_spin_direction / 2.f );

			// apply spin.
			pCmd->viewangles.y += std::fmod( g_pGlobalVars->curtime * ( g_Vars.rage.anti_aim_yaw_spin_speed * 25.f ), ( float )g_Vars.rage.anti_aim_yaw_spin_direction );
			break;
		case 5: // Sideways
			pCmd->viewangles.y += g_Vars.rage.anti_aim_base_yaw_additive;
			break;
		case 6: // Random
			pCmd->viewangles.y += RandomFloat( -360.f, 360.f );
			break;
		case 7: // Static
			pCmd->viewangles.y = g_Vars.rage.anti_aim_base_yaw_additive;
			break;
		case 8: // 180z
		{
			static bool bShould180z = false;

			// choose your starting angle
			float startPoint = 110.0f;

			// fix it to one float
			static float currentAng = startPoint;

			bool bOnGround = ( pLocal->m_fFlags( ) & FL_ONGROUND ) || ( g_Prediction.m_fFlags & FL_ONGROUND );
			if( !bOnGround ) {
				// increment it if we're in air
				currentAng += 5.0f;

				// clamp it incase it goes out of our maximum spin rage
				if( currentAng >= 250.f )
					currentAng = 250.f;

				// yurr
				bShould180z = true;

				// do 180 z ))
				pCmd->viewangles.y += currentAng;
			}
			else {
				// next tick, start at starting point
				currentAng = startPoint;

				// if we were in air tick before, go back to start point 
				// in order to properly rotate next (so fakelag doesnt fuck it up)
				pCmd->viewangles.y += bShould180z ? startPoint : 179.0f;

				// set for future idkf nigger
				bShould180z = false;
			}
			break;
		}

		if( g_Vars.rage.anti_aim_yaw != 5 && g_Vars.rage.anti_aim_yaw != 7 )
			pCmd->viewangles.y += g_Vars.rage.anti_aim_base_yaw_additive;
	}
}

void AntiAim::DoFakeYaw( CUserCmd *pCmd, C_CSPlayer *pLocal ) {
	m_bJitterUpdate = !m_bJitterUpdate;

	switch( g_Vars.rage.anti_aim_fake_yaw ) {
		case 1:
			// set base to opposite of direction.
			pCmd->viewangles.y = m_flLastRealAngle + 180.f;

			// apply 45 degree jitter.
			pCmd->viewangles.y += RandomFloat( -90.f, 90.f );
			break;
		case 2:
			// set base to opposite of direction.
			pCmd->viewangles.y = m_flLastRealAngle + 180.f;

			// apply offset correction.
			pCmd->viewangles.y += g_Vars.rage.anti_aim_fake_yaw_relative;
			break;
		case 3:
		{
			// get fake jitter range from menu.
			float range = g_Vars.rage.anti_aim_fake_yaw_jitter;

			// set base to opposite of direction.
			pCmd->viewangles.y = m_flLastRealAngle + 180.f;

			// apply jitter.
			pCmd->viewangles.y += RandomFloat( -range, range );
			break;
		}
		case 4:
			pCmd->viewangles.y = m_flLastRealAngle + 90.f + std::fmod( g_pGlobalVars->curtime * 360.f, 180.f );
			break;
		case 5:
			pCmd->viewangles.y = RandomFloat( -180.f, 180.f );
			break;
	}
}

bool AntiAim::IsAbleToFlick( C_CSPlayer *pLocal ) {
	// FUCK IT until we fix it XD
	if( g_Vars.globals.m_bFakeWalking && !g_AntiAim.m_bAllowFakeWalkFlick )
		return false;

	if( g_TickbaseController.m_bRunningExploit )
		return false;

	return !g_pClientState->m_nChokedCommands( )
		&& g_Vars.rage.anti_aim_fake_body
		&& ( pLocal->m_fFlags( ) & FL_ONGROUND )
		&& abs( g_pGlobalVars->curtime - g_ServerAnimations.m_uServerAnimations.m_flLowerBodyRealignTimer ) < 1.35f
		&& pLocal->m_PlayerAnimState( )->m_flVelocityLengthXY < 0.1f;
}

// onetap XD
void AntiAim::Distortion( CUserCmd *pCmd, C_CSPlayer *pLocal ) {
	m_bDistorting = false;

	if( g_Vars.rage.anti_aim_manual_ignore_distortion ) {
		if( GetSide( ) != SIDE_MAX ) {
			return;
		}
	}

	int v94 = 0;
	if( !g_Vars.globals.m_bFakeWalking ) {
		if( pLocal->m_vecVelocity( ).Length2D( ) > 5.f ) {
			v94 = 1;
		}
	}

	// these names are so fucked but yea lmfao
	static float flFinalDistorYaw = 0.f;
	static float flDistortionFactor = 0.f;
	static bool bChangeDistortionSide = false;
	static bool bLastDistortionSide = false;
	static int nLastShiftChangeTime = 0;

	if( g_Vars.rage.anti_aim_distortion && !v94 && ( pLocal->m_fFlags( ) & FL_ONGROUND ) ) {
		float flDistortionTurnSpeed = ( g_Vars.rage.anti_aim_distortion_force_turn_speed * 3.f ) / 100.f;
		float flAdjustedSpeed = ( ( g_pGlobalVars->curtime * flDistortionTurnSpeed ) * 3.5f ); // idk what this is, prolly some random number
		float flDistortion = std::sin( flAdjustedSpeed );
		float flMaxTurnYaw = ( g_Vars.rage.anti_aim_distortion_force_turn_factor / 100.f ) * 120.f;
		float flMaxYawDistorted = flMaxTurnYaw * flDistortion;
		bool bForceTurnSide = bChangeDistortionSide;// ( m_nBodyFlicks % 2 );

		if( g_Vars.rage.anti_aim_distortion_shift ) {

			// might not be realtime based, instead tick based (if so just do ++shift_await_smth without any checks)
			static int nLastRealtime = int( g_pGlobalVars->realtime );
			if( nLastRealtime != int( g_pGlobalVars->realtime ) ) {
				++nLastShiftChangeTime;
				nLastRealtime = int( g_pGlobalVars->realtime );
			}

			if( nLastShiftChangeTime < g_Vars.rage.anti_aim_distortion_shift_await ) {
				bLastDistortionSide = bChangeDistortionSide;
			}
			else {
				nLastShiftChangeTime = 0;
				bLastDistortionSide = !bChangeDistortionSide;
				bChangeDistortionSide = !bChangeDistortionSide;
			}

			flDistortionFactor = g_Vars.rage.anti_aim_distortion_shift_factor / 100.0;

			if( !bLastDistortionSide ) {
				flDistortionFactor *= -1.f;
			}

			flMaxYawDistorted = ( flDistortionFactor + flDistortion ) * flMaxTurnYaw;
		}

		if( g_Vars.rage.anti_aim_distortion_force_turn ) {
			static float flDistortionSideYaw = 0.f;

			float flMaxYawDistortedAlt = flMaxYawDistorted;
			if( bForceTurnSide ) {
				flDistortionSideYaw = 60.f;
			}
			else {
				flDistortionSideYaw = 120.f;
				flMaxYawDistortedAlt *= -1.f;
			}

			flFinalDistorYaw = ( flDistortionSideYaw /*+ 45.f*/ )+flMaxYawDistortedAlt;
		}
		else {
			flFinalDistorYaw = /*pCmd->viewangles.y +*/ flMaxYawDistorted;
		}

		flFinalDistorYaw = Math::AngleNormalize( flFinalDistorYaw );
		pCmd->viewangles.y += flFinalDistorYaw;

		m_bDistorting = true;
	}
}

void AntiAim::PerformBodyFlick( CUserCmd *pCmd, bool *bSendPacket, float flOffset ) {
	static bool bINVERT = false;

	// (c) andosa
	const float flDelta = g_Vars.globals.m_bFakeWalking ? ( bINVERT ? -g_Vars.rage.anti_aim_fake_body_amount : g_Vars.rage.anti_aim_fake_body_amount ) : g_Vars.rage.anti_aim_fake_body_amount;

	// note - maxwell; what where you smoking here nico? everyone has perfect lby flick prediction because they're
	// pasting supremacy. this basically gives them a free kill, lol.
	// -------------
	// HI MAXWELL =D
	// to keep the delta big enough
	// we have to flick in positive and then on the next flick
	// into the negative direction (or vice versa)
	// otherwise the LBU breaker will fail
	// before 180 was used as delta to always keep the delta big enough :)
	// whilst fixing fatality I came up with this (they did it similar or the same)


	// Hello NICO!! :DD!!! netivy#1082  add me LDDD!!!:DD!!
	// This Will WorK!! but is not good.... (imo!) we might flick out in open (tap not good..))
	// My Idea is to do Twist sorta so we keep delta large enough!
	// Bad At Explaining but:
	// 1st tick: yaw - ( 180.f - fake_body_slider_amt )
	// 2nd tick: yaw + fake_body_slider_amt 
	// Maybe This Better? IDK maybe i test...
	// THX!
	bINVERT = !bINVERT;

	// 'freestand' lby flick
	if( g_Vars.rage.anti_aim_fake_body_safe_break && !g_Vars.rage.anti_aim_fake_body_twist ) {
		static int nLastAutoSide = ESide::SIDE_MAX;

		// when using edge priority sometimes u will not freestand, causing
		// ur flick to flick out. we can see if this is the case by checking delta
		// between the current detected freestand angle and our actual angle, and 
		// if we're not at that angle then we should break towards the other side
		const float flFreestandAngleDelta = abs( m_flAutoYaw - m_flLastRealAngle );

		// if our freestand is facing left or right, break backwards
		if( m_eAutoSide == ESide::SIDE_LEFT ) {
			nLastAutoSide = ESide::SIDE_LEFT;

			if( flFreestandAngleDelta > 35.f ) {
				m_eBreakerSide = ESide::SIDE_RIGHT;
				pCmd->viewangles.y = m_flLastRealAngle - flDelta + flOffset;
			}
			else {
				m_eBreakerSide = ESide::SIDE_LEFT;
				pCmd->viewangles.y = m_flLastRealAngle + flDelta - flOffset;
			}
		}
		else if( m_eAutoSide == ESide::SIDE_RIGHT ) {
			nLastAutoSide = ESide::SIDE_RIGHT;

			if( flFreestandAngleDelta > 35.f ) {
				m_eBreakerSide = ESide::SIDE_LEFT;
				pCmd->viewangles.y = m_flLastRealAngle + flDelta - flOffset;
			}
			else {
				m_eBreakerSide = ESide::SIDE_RIGHT;
				pCmd->viewangles.y = m_flLastRealAngle - flDelta + flOffset;
			}
		}

		// don't break if we're backwards
		if( m_eAutoSide == ESide::SIDE_BACK && m_eAutoSide != nLastAutoSide ) {
			if( nLastAutoSide == ESide::SIDE_LEFT ) {
				m_eBreakerSide = ESide::SIDE_RIGHT;
				pCmd->viewangles.y = m_flLastRealAngle - flDelta + flOffset;
			}
			else {
				m_eBreakerSide = ESide::SIDE_LEFT;
				pCmd->viewangles.y = m_flLastRealAngle + flDelta - flOffset;
			}
		}
	}
	else {
		m_eBreakerSide = ESide::SIDE_RIGHT;
		pCmd->viewangles.y = m_flLastRealAngle - flDelta + flOffset;
	}

	if( g_pClientState->m_nChokedCommands( ) < 1 ) {
		if( !g_Vars.globals.m_bFakeWalking )
			*bSendPacket = false;
	}

	if( g_Vars.globals.m_bFakeWalking ) {
		//g_ServerAnimations.m_uServerAnimations.m_flLowerBodyRealignTimer = TICKS_TO_TIME( C_CSPlayer::GetLocalPlayer( )->m_nTickBase( ) ) + 1.1f;
		*bSendPacket = true;
	}
}

void AntiAim::BreakLastBody( CUserCmd *pCmd, C_CSPlayer *pLocal, bool *bSendPacket ) {
	if( g_Movement.PressingMovementKeys( pCmd ) || g_Vars.rage.fake_flick_key.enabled )
		return;

	// don't do this if our lby breaker is off
	if( !g_Vars.rage.anti_aim_fake_body )
		return;

	int nTicksToStop = 0;
	const int nTicksLeft = g_FakeLag.m_iAwaitingChoke - g_pClientState->m_nChokedCommands( );

	Vector vecVelocity = pLocal->m_vecVelocity( );
	Vector vecPredictedVelocity = vecVelocity;

	if( !( pLocal->m_fFlags( ) & FL_ONGROUND ) || !( g_Prediction.m_fFlags & FL_ONGROUND ) )
		return;

	static auto m_surfaceFriction = Memory::FindInDataMap( pLocal->GetPredDescMap( ), XorStr( "m_surfaceFriction" ) );
	float flFriction = g_Vars.sv_friction->GetFloat( ) * *( float * )( ( uintptr_t )pLocal + m_surfaceFriction );

	// (c) nitro
	for( ; nTicksToStop <= 15; ++nTicksToStop ) {
		float flSpeed = vecPredictedVelocity.Length( );
		if( flSpeed <= 0.1f )
			break;

		const float flVelocityToStopSpeed = std::max( flSpeed, g_Vars.sv_stopspeed->GetFloat( ) );
		const float flSpeedToStop = flVelocityToStopSpeed * flFriction * g_pGlobalVars->interval_per_tick;
		float flVelocityScale = std::max( 0.f, flSpeed - flSpeedToStop );

		if( flVelocityScale != flSpeed ) {
			flVelocityScale /= flSpeed;

			vecPredictedVelocity *= flVelocityScale;
		}
	}

	// set our last moving lby to where we will break lby to
	// this will completely fuck up last moving lby resolvers, since our last move lby delta
	// and our actual lby delta will be close to zero, so cheats will either think we aren't
	// breaking lby and dump, or just shoot it once and bruteforce later

	// i haven't tested the nTicksToStop shit, but the idea here is we should flick to this position
	// 1 tick before we fully stopped moving (so at the point of the flick we're still moving obv)

	// ALSO do not mention any of this in the GitHub changelogs, just do decoy push names or smth (nothing related to AA)
	if( vecVelocity.Length( ) > 0.1f && ( nTicksToStop > ( 14 - g_pClientState->m_nChokedCommands( ) ) || !g_pClientState->m_nChokedCommands( ) ) ) {
		// g_EventLog.PushEvent( "", XorStr( "breaking last moving" ), Color::White( ), Color::White( ) );

		PerformBodyFlick( pCmd, bSendPacket, 30.f );
	}
}

void AntiAim::HandleManual( CUserCmd *pCmd ) {
	if( !g_Vars.rage.anti_aim_manual ) {
		if( m_eSide != SIDE_MAX )
			m_eSide = SIDE_MAX;

		return;
	}

	static bool bLastPressState[ SIDE_MAX ] = { false, false, false };

	auto UpdateSide = [ & ] ( ESide eSide, int key ) {
		if( InputHelper::Down( key ) && !bLastPressState[ eSide ] ) {
			m_eSide = m_eSide == eSide ? SIDE_MAX : eSide;
			bLastPressState[ eSide ] = true;
		}
		else if( bLastPressState[ eSide ] && !InputHelper::Down( key ) ) {
			bLastPressState[ eSide ] = false;
		}
	};

	UpdateSide( SIDE_LEFT, g_Vars.rage.anti_aim_manual_left_key.key );
	UpdateSide( SIDE_RIGHT, g_Vars.rage.anti_aim_manual_right_key.key );
	UpdateSide( SIDE_BACK, g_Vars.rage.anti_aim_manual_back_key.key );

	switch( m_eSide ) {
		case SIDE_LEFT:
			pCmd->viewangles.y += g_Vars.rage.anti_aim_manual_left;
			break;
		case SIDE_RIGHT:
			pCmd->viewangles.y += g_Vars.rage.anti_aim_manual_right;
			break;
		case SIDE_BACK:
			pCmd->viewangles.y += g_Vars.rage.anti_aim_manual_back;
			break;
	}
}

void AntiAim::Think( CUserCmd *pCmd, bool *bSendPacket, bool *bFinalPacket ) {
	bool attack, attack2;

	if( !g_Vars.rage.anti_aim_active )
		return;

	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	// lalw.
	m_flAutoTime = -1.f;

	auto pWeapon = reinterpret_cast< C_WeaponCSBaseGun * >( pLocal->m_hActiveWeapon( ).Get( ) );
	if( !pWeapon )
		return;

	auto pWeaponData = pWeapon->GetCSWeaponData( );
	if( !pWeaponData.IsValid( ) )
		return;

	attack = pCmd->buttons & IN_ATTACK;
	attack2 = pCmd->buttons & IN_ATTACK2;

	// disable on round freeze period.
	if( g_pGameRules.Xor( ) ) {
		if( g_pGameRules->m_bFreezePeriod( ) )
			return;
	}

	// disable if moving around on a ladder.
	if( ( pCmd->forwardmove || pCmd->sidemove ) && ( pLocal->m_MoveType( ) == MOVETYPE_LADDER || pLocal->m_MoveType( ) == MOVETYPE_NOCLIP ) )
		return;

	// disable if shooting.
	if( attack && pLocal->CanShoot( ) )
		return;

	// CBaseCSGrenade::ItemPostFrame()
	// https://github.com/VSES/SourceEngine2007/blob/master/src_main/game/shared/cstrike/weapon_basecsgrenade.cpp#L209
	if( pWeaponData->m_iWeaponType == WEAPONTYPE_GRENADE
		&& ( !pWeapon->m_bPinPulled( ) || attack || attack2 )
		&& pWeapon->m_fThrowTime( ) > 0.f && pWeapon->m_fThrowTime( ) < g_pGlobalVars->curtime )
		return;

	// disable if knifing.
	if( attack2 && pWeaponData->m_iWeaponType == WEAPONTYPE_KNIFE && pLocal->CanShoot( ) )
		return;

	// disable if pressing +use and not defusing.
	if( ( pCmd->buttons & IN_USE ) && !pLocal->m_bIsDefusing( ) )
		return;

	// note - maxwell; is this even needed? this is from the old prepare method.
	if( g_FakeLag.m_iAwaitingChoke < 1 ) {
		g_FakeLag.m_iAwaitingChoke = 1;

		*bSendPacket = g_pClientState->m_nChokedCommands( ) >= 1;
	}

	DoPitch( pCmd, pLocal );

	// note - maxwell; don't allow two send ticks in a row....)))
	if( *bSendPacket && m_bLastPacket )
		*bSendPacket = false;

	if( !*bSendPacket || !*bFinalPacket ) {
		HandleManual( pCmd );

		// note - maxwell; this has to be ran always because huge iq.
		AutoDirection( pLocal );

		DoRealYaw( pCmd, pLocal );

		// holy.
		bool bCanFreestand = ( pLocal->m_fFlags( ) & FL_ONGROUND ) && ( ( g_Vars.rage.anti_aim_edge_dtc_freestanding_running && pLocal->m_vecVelocity( ).Length2D( ) >= 0.1f )
																		|| ( g_Vars.rage.anti_aim_edge_dtc_freestanding_standing && g_Vars.rage.anti_aim_edge_dtc_freestanding_standing && pLocal->m_vecVelocity( ).Length2D( ) < 0.1f ) );
		bool bCanEdge = ( pLocal->m_fFlags( ) & FL_ONGROUND ) && pLocal->m_vecVelocity( ).Length2D( ) < 0.1f;

		bool bEdgeDetected = false;
		bool bFreestanding = false;
		QAngle angEdgeAngle{ };

		if( GetSide( ) == SIDE_MAX ) {
			// mega lawl.
			if( !g_Vars.rage.anti_aim_edge_dtc_priority ) {
				bFreestanding = m_bHasValidAuto;

				if( !bFreestanding ) {
					bEdgeDetected = DoEdgeAntiAim( pLocal, angEdgeAngle );

					if( bEdgeDetected && bCanEdge && g_Vars.rage.anti_aim_edge_dtc )
						pCmd->viewangles.y = angEdgeAngle.y;
				}
				else if( bCanFreestand ) {
					pCmd->viewangles.y = m_flAutoYaw;
				}
			}
			else {
				bEdgeDetected = DoEdgeAntiAim( pLocal, angEdgeAngle );

				if( !bEdgeDetected ) {
					bFreestanding = m_bHasValidAuto;

					if( bFreestanding && bCanFreestand )
						pCmd->viewangles.y = m_flAutoYaw;
				}
				else if( bCanEdge && g_Vars.rage.anti_aim_edge_dtc )
					pCmd->viewangles.y = angEdgeAngle.y;
			}
		}

		Distortion( pCmd, pLocal );

		m_bEdging = ( bFreestanding && bCanFreestand ) || ( bEdgeDetected && bCanEdge );
		m_flLastRealAngle = pCmd->viewangles.y;
		angViewangle = pCmd->viewangles;
	}
	else {
		if( !g_TickbaseController.m_bRunningExploit ) {
			DoFakeYaw( pCmd, pLocal );
		}
	}

	// troller
	BreakLastBody( pCmd, pLocal, bSendPacket );

	// fake body.
	if( IsAbleToFlick( pLocal ) ) {
		int nTickbaseShift = 0;
		//if( g_Vars.rage.exploit )
		//	nTickbaseShift = 7;

		if( TICKS_TO_TIME( pLocal->m_nTickBase( ) - nTickbaseShift ) > g_ServerAnimations.m_uServerAnimations.m_flLowerBodyRealignTimer || g_ServerAnimations.m_uServerAnimations.m_bRealignBreaker ) {
			PerformBodyFlick( pCmd, bSendPacket );

			g_ServerAnimations.m_uServerAnimations.m_bRealignBreaker = false;
		}
		else {
			if( ( TICKS_TO_TIME( pLocal->m_nTickBase( ) - nTickbaseShift ) + g_pGlobalVars->interval_per_tick ) > g_ServerAnimations.m_uServerAnimations.m_flLowerBodyRealignTimer ) {
				// when breaking under 100 delta, we need the delta between the preflick and the actual flick
				// to be equal to 100 (as low as possibly) so that we can actually break to the angle we want to

				const float flDeltaFrom100 = fabs( g_Vars.rage.anti_aim_fake_body_amount - 100 );

				float flAngleToBreakTo = m_flLastRealAngle - flDeltaFrom100;

				if( m_eBreakerSide == ESide::SIDE_RIGHT ) {
					flAngleToBreakTo = m_flLastRealAngle + flDeltaFrom100;;
				}

				if( g_Vars.rage.anti_aim_fake_body_twist || g_Vars.rage.anti_aim_fake_body_amount <= 100.f )
					pCmd->viewangles.y = flAngleToBreakTo;
			}
		}
	}

	m_bLastPacket = *bSendPacket;
	pCmd->viewangles.y = Math::AngleNormalize( pCmd->viewangles.y );
}