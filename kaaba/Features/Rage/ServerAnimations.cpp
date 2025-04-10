#include "ServerAnimations.hpp"
#include "../../SDK/Displacement.hpp"
#include "AntiAim.hpp"
#include "EnginePrediction.hpp"
#include "TickbaseShift.hpp"

ServerAnimations g_ServerAnimations;

#define USE_MDLCACHE

void ServerAnimations::IncrementLayerCycle( CCSGOPlayerAnimState *m_pAnimstate, C_AnimationLayer *pLayer, bool bAllowLoop ) {
	if( !pLayer || !m_pAnimstate )
		return;

	if( !m_pAnimstate->m_pPlayer )
		return;

	if( abs( pLayer->m_flPlaybackRate ) <= 0 )
		return;

	float flCurrentCycle = pLayer->m_flCycle;
	flCurrentCycle += m_pAnimstate->m_flLastUpdateIncrement * pLayer->m_flPlaybackRate;

	if( !bAllowLoop && flCurrentCycle >= 1 ) {
		flCurrentCycle = 0.999f;
	}

	if( pLayer->m_flCycle != Math::ClampCycle( flCurrentCycle ) ) {
		pLayer->m_flCycle = Math::ClampCycle( flCurrentCycle );
		m_pAnimstate->m_pPlayer->InvalidatePhysicsRecursive( ANIMATION_CHANGED );
	}
}

void ServerAnimations::IncrementLayerWeight( CCSGOPlayerAnimState *m_pAnimstate, C_AnimationLayer *pLayer ) {
	if( !pLayer )
		return;

	if( abs( pLayer->m_flWeightDeltaRate ) <= 0.f )
		return;

	float flCurrentWeight = pLayer->m_flWeight;
	flCurrentWeight += m_pAnimstate->m_flLastUpdateIncrement * pLayer->m_flWeightDeltaRate;
	flCurrentWeight = std::clamp( flCurrentWeight, 0.f, 1.f );

	if( pLayer->m_flWeight != flCurrentWeight ) {
		pLayer->m_flWeight = flCurrentWeight;
	}
}

float ServerAnimations::GetLayerIdealWeightFromSeqCycle( CCSGOPlayerAnimState *m_pAnimstate, C_AnimationLayer *pLayer ) {
	if( !pLayer )
		return 0.f;

#ifdef USE_MDLCACHE
	MDLCACHE_CRITICAL_SECTION( );
#endif

	float flCycle = pLayer->m_flCycle;
	if( flCycle >= 0.999f )
		flCycle = 1;

	float flEaseIn = pLayer->m_flLayerFadeOuttime; // seqdesc.fadeintime;
	float flEaseOut = pLayer->m_flLayerFadeOuttime; // seqdesc.fadeouttime;
	float flIdealWeight = 1;

	if( flEaseIn > 0 && flCycle < flEaseIn ) {
		flIdealWeight = Math::SmoothStepBounds( 0, flEaseIn, flCycle );
	}
	else if( flEaseOut < 1 && flCycle > flEaseOut ) {
		flIdealWeight = Math::SmoothStepBounds( 1.0f, flEaseOut, flCycle );
	}

	if( flIdealWeight < 0.0015f )
		return 0.f;

	return ( std::clamp( flIdealWeight, 0.f, 1.f ) );
}

bool ServerAnimations::IsLayerSequenceCompleted( CCSGOPlayerAnimState *m_pAnimstate, C_AnimationLayer *pLayer ) {
	if( pLayer ) {
		return ( ( pLayer->m_flCycle + ( m_pAnimstate->m_flLastUpdateIncrement * pLayer->m_flPlaybackRate ) ) >= 1 );
	}

	return false;
}

Activity ServerAnimations::GetLayerActivity( CCSGOPlayerAnimState *m_pAnimstate, C_AnimationLayer *pLayer ) {
	if( !m_pAnimstate || !m_pAnimstate->m_pPlayer )
		return ACT_INVALID;

#ifdef USE_MDLCACHE
	MDLCACHE_CRITICAL_SECTION( );
#endif

	if( pLayer ) {
		return ( Activity )m_pAnimstate->m_pPlayer->GetSequenceActivity( pLayer->m_nSequence );
	}

	return ACT_INVALID;
}

void ServerAnimations::IncrementLayerCycleWeightRateGeneric( CCSGOPlayerAnimState *m_pAnimstate, C_AnimationLayer *pLayer ) {
	float flWeightPrevious = pLayer->m_flWeight;
	IncrementLayerCycle( m_pAnimstate, pLayer, false );
	pLayer->m_flWeight = GetLayerIdealWeightFromSeqCycle( m_pAnimstate, pLayer );
	pLayer->m_flWeightDeltaRate = flWeightPrevious;
}

int ServerAnimations::SelectWeightedSequenceFromModifiers( C_CSPlayer *pEntity, int32_t activity, CUtlVector<uint16_t> modifiers ) {
	typedef CStudioHdr::CActivityToSequenceMapping *( __thiscall *fnFindMapping )( void * );
	typedef int32_t( __thiscall *fnSelectWeightedSequenceFromModifiers )( void *, void *, int32_t, const void *, int32_t );

	static const auto FindMappingAdr = Memory::Scan( XorStr( "client.dll" ), XorStr( "55 8B EC 83 E4 F8 81 EC ? ? ? ? 53 56 57 8B F9 8B 17" ) );
	static const auto SelectWeightedSequenceFromModifiersAdr = Memory::Scan( XorStr( "server.dll" ), XorStr( "55 8B EC 83 EC 2C 53 56 8B 75 08 8B D9" ) );

	auto pHdr = pEntity->m_pStudioHdr( );
	if( !pHdr ) {
		return -1;
	}

	const auto pMapping = ( ( fnFindMapping )FindMappingAdr )( pHdr );
	if( !pHdr->m_pActivityToSequence ) {
		pHdr->m_pActivityToSequence = pMapping;
	}

	return ( ( fnSelectWeightedSequenceFromModifiers )SelectWeightedSequenceFromModifiersAdr ) ( pMapping, pHdr, activity, modifiers.Base( ), modifiers.Count( ) );
}

void ServerAnimations::SetLayerSequence( C_CSPlayer *pEntity, C_AnimationLayer *pLayer, int32_t activity, CUtlVector<uint16_t> modifiers, int nOverrideSequence ) {

#ifdef USE_MDLCACHE
	g_pMDLCache->BeginLock( );
#endif

	int nSequence = SelectWeightedSequenceFromModifiers( pEntity, activity, modifiers );

#ifdef USE_MDLCACHE
	g_pMDLCache->EndLock( );
#endif

	if( nOverrideSequence != -1 )
		nSequence = nOverrideSequence;

	if( nSequence >= 2 ) {
	#ifdef USE_MDLCACHE
		g_pMDLCache->BeginLock( );
	#endif
		if( pLayer ) {
			pLayer->m_nSequence = nSequence;
			pLayer->m_flPlaybackRate = pEntity->GetLayerSequenceCycleRate( pLayer, nSequence );
			pLayer->m_flCycle = 0.0f;
			pLayer->m_flWeight = 0.0f;

			// todo: maybe some other day, i don't think it's needed
			// UpdateLayerOrderPreset( 0.0f, layer, sequence ); 
		}
	#ifdef USE_MDLCACHE
		g_pMDLCache->EndLock( );
	#endif
	}
}

bool ServerAnimations::IsModifiedLayer( int nLayer ) {
	// note - michal;
	// in the future, we should look into rebuilding each layer (or atleast the most curcial ones,
	// such as weapon_action, movement_move, whole_body, etc). i only rebuilt these for the time being
	// as they're the only layers we really need to rebuild, they're responsible for eyepos when landing
	// (also known as "landing comp" kek) etc. plus once they're fixed the animations are eye candy :-)
	return ( nLayer == ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB || nLayer == ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL );
}

void ServerAnimations::HandleAnimationEvents( C_CSPlayer *pLocal, CCSGOPlayerAnimState *pState, C_AnimationLayer *layers, CUtlVector< uint16_t > uModifiers, CUserCmd *cmd ) {
	if( !pLocal || !pState || !cmd )
		return;

	if( pLocal->IsDead( ) )
		return;

	if( !pLocal->m_PlayerAnimState( ) || !g_pEngine->IsInGame( ) || !g_pEngine->IsConnected( ) )
		return;

	auto pWeapon = ( C_WeaponCSBaseGun * )pLocal->m_hActiveWeapon( ).Get( );
	if( !pWeapon )
		return;

#ifdef USE_MDLCACHE
	MDLCACHE_CRITICAL_SECTION( );
#endif

	// setup layers that we want to use/fix
	C_AnimationLayer *ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB = &layers[ animstate_layer_t::ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB ];
	C_AnimationLayer *ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL = &layers[ animstate_layer_t::ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL ];

	// setup ground and flag stuff
	int fFlags = pLocal->m_fFlags( );
	static bool bOnGround = false;
	bool bWasOnGround = bOnGround;
	bOnGround = ( fFlags & 1 );

	static float m_flLadderWeight = 0.f, m_flLadderSpeed = 0.f;

	// not sure about these two, they don't seem wrong though
	bool bLeftTheGroundThisFrame = bWasOnGround && !bOnGround;
	bool bLandedOnGroundThisFrame = !bWasOnGround && bOnGround;
	static bool m_bAdjustStarted = false;

	// ladder stuff
	static bool bOnLadder = false;
	bool bPreviouslyOnLadder = bOnLadder;
	bOnLadder = !bOnGround && pLocal->m_MoveType( ) == MOVETYPE_LADDER;
	bool bStartedLadderingThisFrame = ( !bPreviouslyOnLadder && bOnLadder );
	bool bStoppedLadderingThisFrame = ( bPreviouslyOnLadder && !bOnLadder );

	// TODO: fix the rest of the layers, for now I'm only fixing the land and jump layer
	// because until I get this working without any fuckery, there is no point to continue
	// trying to fix every other layer.
	// see: CSGOPlayerAnimState::SetUpMovement
	int nSequence = 0;

	// fix ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB
	if( bOnLadder ) {
		if( bStartedLadderingThisFrame ) {
			SetLayerSequence( pLocal, ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, ACT_CSGO_CLIMB_LADDER, uModifiers );
		}

		if( abs( pState->m_flVelocityLengthZ ) > 100 ) {
			m_flLadderSpeed = Math::Approach( 1, m_flLadderSpeed, pState->m_flLastUpdateIncrement * 10.0f );
		}
		else {
			m_flLadderSpeed = Math::Approach( 0, m_flLadderSpeed, pState->m_flLastUpdateIncrement * 10.0f );
		}
		m_flLadderSpeed = std::clamp( m_flLadderSpeed, 0.f, 1.f );

		if( bOnLadder ) {
			m_flLadderWeight = Math::Approach( 1, m_flLadderWeight, pState->m_flLastUpdateIncrement * 5.0f );
		}
		else {
			m_flLadderWeight = Math::Approach( 0, m_flLadderWeight, pState->m_flLastUpdateIncrement * 10.0f );
		}
		m_flLadderWeight = std::clamp( m_flLadderWeight, 0.f, 1.f );

		Vector vecLadderNormal = pLocal->m_vecLadderNormal( );
		QAngle angLadder;

		Math::VectorAngles( vecLadderNormal, angLadder );
		float flLadderYaw = Math::AngleDiff( angLadder.y, pState->m_flFootYaw );
		pState->m_tPoseParamMappings[ LADDER_YAW ].SetValue( pLocal, flLadderYaw );

		float flLadderClimbCycle = ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB->m_flCycle;
		flLadderClimbCycle += ( pState->m_vecPositionCurrent.z - pState->m_vecPositionLast.z ) * Math::Lerp( m_flLadderSpeed, 0.010f, 0.004f );

		pState->m_tPoseParamMappings[ LADDER_SPEED ].SetValue( pLocal, m_flLadderSpeed );

		if( GetLayerActivity( pState, ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB ) == ACT_CSGO_CLIMB_LADDER ) {
			ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB->m_flWeight = m_flLadderWeight;
		}

		ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB->m_flCycle = flLadderClimbCycle;

		// fade out jump if we're climbing
		if( bOnLadder ) {
			float flIdealJumpWeight = 1.0f - m_flLadderWeight;
			if( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL->m_flWeight > flIdealJumpWeight ) {
				ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL->m_flWeight = flIdealJumpWeight;
			}
		}
	}
	// fix ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL
	else {
		//?????
		if( pState->m_bOnGround ) {
			pState->m_flDurationInAir = 0;
		}

		m_flLadderSpeed = m_flLadderWeight = 0.f;

		// TODO: bStoppedLadderingThisFrame
		if( bLandedOnGroundThisFrame ) {
			// setup the sequence responsible for landing
			nSequence = 20;
			if( pState->m_flSpeedAsPortionOfWalkTopSpeed > .25f )
				nSequence = 22;

			IncrementLayerCycle( pState, ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, false );
			IncrementLayerCycle( pState, ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, false );

			pState->m_tPoseParamMappings[ JUMP_FALL ].SetValue( pLocal, 0 );

			if( IsLayerSequenceCompleted( pState, ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB ) ) {
				pState->m_bLanding = false;
				ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB->m_flWeight = 0.f;
				ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL->m_flWeight = 0.f;
				pState->m_flLandAnimMultiplier = 1.0f;
			}
			else {
				float flLandWeight = GetLayerIdealWeightFromSeqCycle( pState, ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB ) * pState->m_flLandAnimMultiplier;

				// if we hit the ground crouched, reduce the land animation as a function of crouch, since the land animations move the head up a bit ( and this is undesirable )
				flLandWeight *= std::clamp( ( 1.0f - pState->m_flAnimDuckAmount ), 0.2f, 1.0f );

				ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB->m_flWeight = flLandWeight;

				// fade out jump because land is taking over
				float flCurrentJumpFallWeight = ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL->m_flWeight;
				if( flCurrentJumpFallWeight > 0 ) {
					flCurrentJumpFallWeight = Math::Approach( 0, flCurrentJumpFallWeight, pState->m_flLastUpdateIncrement * 10.0f );
					ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL->m_flWeight = flCurrentJumpFallWeight;
				}
			}

			SetLayerSequence( pLocal, ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, -1, uModifiers, nSequence );
		}
		else if( bLeftTheGroundThisFrame ) {
			// setup the sequence responsible for jumping
			if( pState->m_flSpeedAsPortionOfWalkTopSpeed > .25f ) {
				nSequence = pState->m_flAnimDuckAmount > .55f ? 18 : 16;
			}
			else {
				nSequence = pState->m_flAnimDuckAmount > .55f ? 17 : 15;
			}

			SetLayerSequence( pLocal, ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, -1, uModifiers, nSequence );
		}

		// blend jump into fall. This is a no-op if we're playing a fall anim.
		pState->m_tPoseParamMappings[ JUMP_FALL ].SetValue( pLocal, std::clamp( Math::SmoothStepBounds( 0.72f, 1.52f, pState->m_flDurationInAir ), 0.f, 1.f ) );
	}

	// apply our changes
	layers[ animstate_layer_t::ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB ] = *ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB;
	layers[ animstate_layer_t::ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL ] = *ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL;
}

void ServerAnimations::SetLayerInactive( C_AnimationLayer *layers, int idx ) {
	if( !layers )
		return;

	layers[ idx ].m_flCycle = 0.f;
	layers[ idx ].m_nSequence = 0.f;
	layers[ idx ].m_flWeight = 0.f;
	layers[ idx ].m_flPlaybackRate = 0.f;
}

void ServerAnimations::HandleServerAnimation( bool *bSendPacket, CUserCmd *pCmd ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	// perform basic sanity checks
	if( pLocal->IsDead( ) )
		return;

	auto state = pLocal->m_PlayerAnimState( );
	if( !state )
		return;

	auto _pState = pLocal->m_PlayerAnimState( );

	// baboom, stack is broken 
	// https://youtu.be/ovaLaF6Rmhg
	// https://youtu.be/oavMtUWDBTM
	Encrypted_t<CCSGOPlayerAnimState> kekw( _pState );
	Encrypted_t<Encrypted_t<CCSGOPlayerAnimState>> pState( &kekw );

	if( !pState.Xor( )->IsValid( ) )
		return;

	if( g_pClientState->m_nChokedCommands( ) > 0 )
		return;

	g_AntiAim.angViewangle = pCmd->viewangles;

	if( ( *pState.Xor( ) )->m_bLanding && ( g_Prediction.m_fFlags & FL_ONGROUND ) && ( pLocal->m_fFlags( ) & FL_ONGROUND ) ) {
		g_AntiAim.angViewangle.x = XorFlt( -10.f );
	}

	// allow re-animating in the same frame
	// https://github.com/perilouswithadollarsign/cstrike15_src/blob/f82112a2388b841d72cb62ca48ab1846dfcc11c8/game/shared/cstrike15/csgo_playeranimstate.cpp#L266

	if( ( *pState.Xor( ) )->m_nLastUpdateFrame == g_pGlobalVars->framecount )
		( *pState.Xor( ) )->m_nLastUpdateFrame = g_pGlobalVars->framecount - 1;

	if( ( *pState.Xor( ) )->m_flLastUpdateTime == g_pGlobalVars->curtime )
		( *pState.Xor( ) )->m_flLastUpdateTime = g_pGlobalVars->curtime - 1;

	// prevent C_BaseEntity::CalcAbsoluteVelocity being called
	pLocal->m_iEFlags( ) &= ~EFL_DIRTY_ABSVELOCITY;

	// snap to footyaw, instead of approaching
	( *pState.Xor( ) )->m_flMoveWeight = 0.f;

	// note - michal;
	// not needed, i hardcoded most sequences.. 
	// we should look into this when we decide to rebuild
	// more layers, for now it can stay like this, lmfao

	// rebuild server CCSGOPlayerAnimState::SetUpMovement
	// update m_bStrafing, happens on server but not on client, make sure to sync it up
	// https://github.com/perilouswithadollarsign/cstrike15_src/blob/f82112a2388b841d72cb62ca48ab1846dfcc11c8/game/shared/cstrike15/csgo_playeranimstate.cpp#L1452

	// get the user's left and right button pressed states
	bool moveRight = ( pCmd->buttons & ( IN_MOVERIGHT ) ) != 0;
	bool moveLeft = ( pCmd->buttons & ( IN_MOVELEFT ) ) != 0;
	bool moveForward = ( pCmd->buttons & ( IN_FORWARD ) ) != 0;
	bool moveBackward = ( pCmd->buttons & ( IN_BACK ) ) != 0;

	Vector vecForward;
	Vector vecRight;
	Vector vecUp; //unused
	Math::AngleVectors( QAngle( 0, ( *pState.Xor( ) )->m_flFootYaw, 0 ), vecForward, vecRight, vecUp );
	vecRight.Normalized( );
	float flVelToRightDot = DotProduct( ( *pState.Xor( ) )->m_vecVelocityNormalizedNonZero, vecRight );
	float flVelToForwardDot = DotProduct( ( *pState.Xor( ) )->m_vecVelocityNormalizedNonZero, vecForward );

	bool bStrafeRight = ( ( *pState.Xor( ) )->m_flSpeedAsPortionOfWalkTopSpeed >= 0.73f && moveRight && !moveLeft && flVelToRightDot < -0.63f );
	bool bStrafeLeft = ( ( *pState.Xor( ) )->m_flSpeedAsPortionOfWalkTopSpeed >= 0.73f && moveLeft && !moveRight && flVelToRightDot > 0.63f );
	bool bStrafeForward = ( ( *pState.Xor( ) )->m_flSpeedAsPortionOfWalkTopSpeed >= 0.65f && moveForward && !moveBackward && flVelToForwardDot < -0.55f );
	bool bStrafeBackward = ( ( *pState.Xor( ) )->m_flSpeedAsPortionOfWalkTopSpeed >= 0.65f && moveBackward && !moveForward && flVelToForwardDot > 0.55f );

	//v52 = *( v2 + 0x18 ); // pState->m_pPlayer
	//if( *( v52 + 0x39E0 ) ) // m_pPlayer->m_bStrafing
	static auto m_bStrafing = Engine::g_PropManager.GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_bStrafing" ) );
	*( bool * )( uintptr_t( pLocal ) + m_bStrafing ) = ( bStrafeRight || bStrafeLeft || bStrafeForward || bStrafeBackward ) && !g_Vars.rage.fake_lag_fix_leg_movement;

	// set thirdperson angles
	pLocal->pl( ).v_angle = g_AntiAim.angViewangle;

	// update animations
	pLocal->UpdateClientSideAnimationEx( );

	if( m_uServerAnimations.m_flSpawnTime != pLocal->m_flSpawnTime( ) ) {
		( *pState.Xor( ) )->Reset( );
		m_uServerAnimations.m_flSpawnTime = pLocal->m_flSpawnTime( );
	}

	// build activity modifiers
	// auto modifiers = BuildActivityModifiers( pLocal );
	auto modifiers = CUtlVector<uint16_t>( );

	// handle animation events on client
	// HandleAnimationEvents( pLocal, pState, pLocal->m_AnimOverlay( ).Base( ), modifiers, pCmd );

	// note - michal;
	// might want to make some storage for constant anim variables
	const float Xored1_1 = XorFlt( 1.1f );
	const float Xored0_22 = XorFlt( 0.22f );
	const float flServerTime = TICKS_TO_TIME( pLocal->m_nTickBase( ) );

	if( ( *pState.Xor( ) )->m_bOnGround ) {
		// rebuild server CCSGOPlayerAnimState::SetUpVelocity
		// predict m_flLowerBodyYawTarget
		if( ( *pState.Xor( ) )->m_flVelocityLengthXY > 0.1f ) {
			m_uServerAnimations.m_flLowerBodyRealignTimer = flServerTime + ( Xored0_22 );
			//	m_uServerAnimations.m_flLowerBodyYawTarget = ( *pState.Xor( ) )->m_flEyeYaw;
		}
		// note - michal;
		// hello ledinchik men so if you've noticed our fakewalk breaks/stops for a while if we don't use "random" 
		// fake yaw option, coz random swaps flick side making this footyaw and eyeyaw delta pretty much always > 35.f
		// and the other options only flick to one side and due to something something footyaw being weird when flicking 
		// the delta jumps below 35, which causes the fakewalk to freak out and stop
		// so if we remove the delta check the fakewalk will work perfectly on every lby breaker option (but sometimes fail cos obv it failed on server)
		// only way i can think of fixing this without removing the delta check (coz ur comment below is right) is to force flick further or smth or 
		// somehow make sure that the footyaw is always at a bigger than 35deg delta from eyeyaw, whether that'd be by recalculating it ourselves
		// or maybe doing some other fuckery shit IDK!!
		// TLDR: fakewalk stops for 2 hours coz of the delta check (vague asf)
		else if( flServerTime > m_uServerAnimations.m_flLowerBodyRealignTimer && abs( Math::AngleDiff( ( *pState.Xor( ) )->m_flFootYaw, ( *pState.Xor( ) )->m_flEyeYaw ) ) > XorFlt( 35.0f ) ) {
			m_uServerAnimations.m_flLowerBodyRealignTimer = flServerTime + Xored1_1;
			// m_uServerAnimations.m_flLowerBodyYawTarget = ( *pState.Xor( ) )->m_flEyeYaw;
		}

		const float flLBYDelta = std::abs( Math::AngleNormalize( g_ServerAnimations.m_uServerAnimations.m_flLowerBodyYawTarget - g_ServerAnimations.m_uServerAnimations.m_flEyeYaw ) );

		if( flLBYDelta >= 35.f ) {
			m_uServerAnimations.m_flLastValidDelta = flServerTime;
		}

		const float flLastValidDeltaTime = std::abs( m_uServerAnimations.m_flLastValidDelta - flServerTime );

		// lby isn't broken and hasn't been broken for 
		// a good while now, force it to break and force 
		// timer to start predicting once again.
		if( flLBYDelta < 35.f && flLastValidDeltaTime > Xored1_1 + g_pGlobalVars->interval_per_tick ) {
			// m_uServerAnimations.m_flLowerBodyRealignTimer = flServerTime + Xored1_1;
			m_uServerAnimations.m_bRealignBreaker = true;
		}

		m_uServerAnimations.m_flLowerBodyYawTarget = pLocal->m_flLowerBodyYawTarget( );
	}

	// fix legs failing idk
	// if( !( *pState.Xor( ) )->m_bOnGround ) {
	// 	pLocal->m_AnimOverlay( )[ animstate_layer_t::ANIMATION_LAYER_MOVEMENT_MOVE ].m_flWeight = 0.f;
	// 	pLocal->m_AnimOverlay( )[ animstate_layer_t::ANIMATION_LAYER_MOVEMENT_MOVE ].m_flCycle = 0.f;
	// }

	// remove ACT_CSGO_IDLE_TURN_BALANCEADJUST animation
	// pLocal->m_AnimOverlay( )[ animstate_layer_t::ANIMATION_LAYER_ADJUST ].m_flWeight = 0.f;
	// pLocal->m_AnimOverlay( )[ animstate_layer_t::ANIMATION_LAYER_ADJUST ].m_flCycle = 0.f;

	// remove model sway
	pLocal->m_AnimOverlay( )[ animstate_layer_t::ANIMATION_LAYER_LEAN ].m_flWeight = 0.f;

	/*if( *( ( m_MoveType ^ m_MoveType_xor ) + v188.player_ptr ) == 2
		&& ( v188.gapC->buttons & 2 ) != 0
		&& ( v188.gap10[ 92 ] & 1 ) != 0
		&& ( v188.gap10[ 32 ] & 1 ) == 0
		&& *( player_ptr + ( m_AnimOverlay ^ zero ) + 12 ) > 4 )
	{
		v94 = *( player_ptr + ( m_AnimOverlay ^ zero ) );
		v195[ 0 ] = g_pPlayerAnimstate;
		v196 = player_ptr;
		v95 = g_pPlayerAnimstate->GetWeightedSequenceFromActivity( v195, 985 );
		v94[ 4 ].m_flCycle = 0.0;
		v94[ 4 ].m_flWeight = 0.0;
		player_ptr = player_ptr_;
		v94[ 4 ].m_nSequence = v95;
	}*/

	C_AnimationLayer *v94 = &pLocal->m_AnimOverlay( )[ 4 ];
	if( ( pCmd->buttons & 2 ) != 0 && !( pLocal->m_fFlags( ) & FL_ONGROUND ) && ( g_Prediction.m_fFlags & FL_ONGROUND ) ) {
		int v95 = ( *pState.Xor( ) )->GetWeightedSequenceFromActivity( /*v195,*/ 985 );

		v94->m_nSequence = v95;
		v94->m_flCycle = 0.0f;
		v94->m_flWeight = 0.0f;
	}

	//if( GetAsyncKeyState( VK_MBUTTON ) && v94 ) {
	//	g_pCVar->ConsoleColorPrintf( Color::White( ), "m_flCycle: %.6f | m_flWeight: %.6f\n", v94->m_flCycle, v94->m_flWeight );
	//}

	// build bones, save bone rotation etc. on network update
	pLocal->SetupBones( nullptr, 128, BONE_USED_BY_ANYTHING, pLocal->m_flSimulationTime( ) );

	// save bone rotations
	std::memcpy( m_uServerAnimations.m_vecBonePos, pLocal->m_vecBonePos( ), pLocal->m_CachedBoneData( ).Count( ) * sizeof( Vector ) );
	std::memcpy( m_uServerAnimations.m_quatBoneRot, pLocal->m_quatBoneRot( ), pLocal->m_CachedBoneData( ).Count( ) * sizeof( Quaternion ) );

	// save real rotation
	m_uServerAnimations.m_flFootYaw = ( *pState.Xor( ) )->m_flFootYaw;
	m_uServerAnimations.m_flEyeYaw = ( *pState.Xor( ) )->m_flEyeYaw;

	// set dis to true when compiling dll for personal use
	// and not updating cheat, so ppl dont receive info abt you
#if 1
	bool bIsDeveloper = false;
	if( bIsDeveloper )
		return;

	// send off our angles LOOOOL
#endif
}

void ServerAnimations::HandleAnimations( bool *bSendPacket, CUserCmd *cmd ) {
	C_CSPlayer *pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( pLocal->IsDead( ) )
		return;

	// handle server animations
	HandleServerAnimation( bSendPacket, cmd );
}