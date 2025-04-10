#include "AnimationSystem.hpp"
#include "LagCompensation.hpp"
#include "Resolver.hpp"
#include "../../Utils/Threading/Threading.h"

AnimationSystem g_AnimationSystem;

AnimationData *AnimationSystem::GetAnimationData( int index ) {
	if( m_AnimatedEntities.count( index ) < 1 )
		return nullptr;

	return &m_AnimatedEntities[ index ];
}

void AnimationSystem::FrameStage( ) {
	if( !g_pEngine->IsInGame( ) || !g_pEngine->GetNetChannelInfo( ) ) {
		m_AnimatedEntities.clear( );
		return;
	}

	if( !g_pEngine->IsPlayingDemo( ) ) {
		constexpr auto te_fire_bullets = hash_32_fnv1a_const( "CTEFireBullets" );

		for( auto event = *( CEventInfo ** )( uintptr_t( g_pClientState.Xor( ) ) + 0x4DEC ); event; event = event->pNextEvent ) {
			auto v25 = event->pClientClass;
			if( v25 ) {
				if( hash_32_fnv1a_const( v25->m_pNetworkName ) == te_fire_bullets )
					event->fire_delay = 0.0f;
			}
		}

		g_pEngine->FireEvents( );
	}

	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	for( int i = 1; i <= g_pGlobalVars->maxClients; ++i ) {
		auto player = C_CSPlayer::GetPlayerByIndex( i );
		if( !player || player == pLocal )
			continue;

		player_info_t player_info;
		if( !g_pEngine->GetPlayerInfo( player->m_entIndex, &player_info ) ) {
			continue;
		}

		m_AnimatedEntities[ i ].Collect( player );
	}

	if( m_AnimatedEntities.empty( ) )
		return;

	for( auto &[key, value] : m_AnimatedEntities ) {
		auto entity = C_CSPlayer::GetPlayerByIndex( key );
		if( !entity )
			continue;

		if( value.m_bUpdated )
			value.Update( );

		value.m_bUpdated = false;
	}
}

void AnimationData::Collect( C_CSPlayer *pPlayer ) {
	if( pPlayer->IsDead( ) )
		player = nullptr;

	// reset data
	if( player != pPlayer ) {
		m_flSimulationTime = 0.0f;
		m_flOldSimulationTime = 0.0f;
		m_AnimationRecord.clear( );
		m_bWasDormant = false;
		player = pPlayer;
		m_bIsAlive = false;
	}

	if( !pPlayer )
		return;

	m_bIsAlive = true;
	m_flOldSimulationTime = player->m_flOldSimulationTime( );
	m_flSimulationTime = player->m_flSimulationTime( );

	if( m_flSimulationTime == 0.0f || player->IsDormant( ) ) {
		m_bWasDormant = true;
		m_uLayerAliveLoopData.m_flCycle = 0;
		m_uLayerAliveLoopData.m_flPlaybackRate = 0;
		return;
	}

	if( m_uLayerAliveLoopData.m_flCycle == player->m_AnimOverlay( )[ animstate_layer_t::ANIMATION_LAYER_ALIVELOOP ].m_flCycle
		/*&& m_uLayerAliveLoopData.m_flPlaybackRate == player->m_AnimOverlay( )[ animstate_layer_t::ANIMATION_LAYER_ALIVELOOP ].m_flPlaybackRate*/ ) {
		player->m_flSimulationTime( ) = m_flOldSimulationTime;
		return;
	}

	m_uLayerAliveLoopData.m_flCycle = player->m_AnimOverlay( )[ animstate_layer_t::ANIMATION_LAYER_ALIVELOOP ].m_flCycle;
	m_uLayerAliveLoopData.m_flPlaybackRate = player->m_AnimOverlay( )[ animstate_layer_t::ANIMATION_LAYER_ALIVELOOP ].m_flPlaybackRate;

	//
	// no need for this check anymore boi
	// - L3D451R7
	/*if( m_flOldSimulationTime == m_flSimulationTime ) {
		return;
	}*/

	if( m_bWasDormant ) {
		m_AnimationRecord.clear( );
	}

	m_bUpdated = true;
	m_bWasDormant = false;

	int nTickRate = int( 1.0f / g_pGlobalVars->interval_per_tick );

	while( m_AnimationRecord.size( ) > nTickRate ) {
		m_AnimationRecord.pop_back( );
	}

	auto pWeapon = ( C_WeaponCSBaseGun * )( player->m_hActiveWeapon( ).Get( ) );

	Encrypted_t<AnimationRecord> _previousRecord = nullptr;
	Encrypted_t<AnimationRecord> _penultimateRecord = nullptr;

	if( m_AnimationRecord.size( ) > 0 ) {
		_previousRecord = &m_AnimationRecord.front( );

		if( m_AnimationRecord.size( ) > 1 ) {
			_penultimateRecord = &m_AnimationRecord.at( 1 );
		}
	}

	// cruel
	Encrypted_t<Encrypted_t<AnimationRecord>> pPreviousRecord( &_previousRecord );
	Encrypted_t<Encrypted_t<AnimationRecord>> pPenultimateRecord( &_penultimateRecord );

	// emplace new record
	auto pNewAnimRecord = &m_AnimationRecord.emplace_front( );

	// fill up this record with all basic information
	std::memcpy( pNewAnimRecord->m_pServerAnimOverlays.data( ), player->m_AnimOverlay( ).Base( ), 13 * sizeof( C_AnimationLayer ) );
	pNewAnimRecord->m_flFeetCycle = pNewAnimRecord->m_pServerAnimOverlays[ 6 ].m_flCycle;
	pNewAnimRecord->m_flMoveWeight = pNewAnimRecord->m_pServerAnimOverlays[ 6 ].m_flWeight;
	pNewAnimRecord->m_flLowerBodyYawTarget = player->m_flLowerBodyYawTarget( );
	pNewAnimRecord->m_angEyeAngles = player->m_angEyeAngles( );
	pNewAnimRecord->m_flDuckAmount = player->m_flDuckAmount( );
	pNewAnimRecord->m_flSimulationTime = m_flSimulationTime;
	pNewAnimRecord->m_flAnimationTime = m_flOldSimulationTime + g_pGlobalVars->interval_per_tick;
	pNewAnimRecord->m_vecOrigin = player->m_vecOrigin( );
	pNewAnimRecord->m_fFlags = player->m_fFlags( );
	pNewAnimRecord->m_bIsInvalid = false;

	pNewAnimRecord->m_pEntity = player;

	const auto simulation_ticks = TIME_TO_TICKS( m_flSimulationTime );
	const auto old_simulation_ticks = TIME_TO_TICKS( m_flOldSimulationTime );

	// calculate choke time and choke ticks, compensating
	// for players coming out of dormancy / newly generated records
	if( ( *pPreviousRecord.Xor( ) ).IsValid( ) && abs( simulation_ticks - old_simulation_ticks ) <= 31 ) {
		pNewAnimRecord->m_flChokeTime = m_flSimulationTime - m_flOldSimulationTime;
		pNewAnimRecord->m_iChokeTicks = simulation_ticks - old_simulation_ticks;
	} 
	else {
		pNewAnimRecord->m_flChokeTime = g_pGlobalVars->interval_per_tick;
		pNewAnimRecord->m_iChokeTicks = 1;
		( *pPreviousRecord.Xor( ) ) = nullptr;
	}

	if( pWeapon && pWeapon->IsGun( ) ) {
		if( pWeapon->m_fLastShotTime( ) > m_flOldSimulationTime && pWeapon->m_fLastShotTime( ) <= m_flSimulationTime ) {
			// set it (in case this guy continues to shoot)
			if( ( *pPreviousRecord.Xor( ) ).IsValid( ) ) {
				pNewAnimRecord->m_flLastNonShotPitch = ( *pPreviousRecord.Xor( ) )->m_flLastNonShotPitch;
				pNewAnimRecord->m_angEyeAngles.x = pNewAnimRecord->m_flLastNonShotPitch; 
			}
		}
		else {
			pNewAnimRecord->m_flLastNonShotPitch = pNewAnimRecord->m_angEyeAngles.x;
		}
	}

	// we'll need information from the previous record in order to further
	// fix animations, skip everything and invalidate crucial data
	if( !( *pPreviousRecord.Xor( ) ).IsValid( ) ) {
		pNewAnimRecord->m_bIsInvalid = true;
		pNewAnimRecord->m_vecVelocity.Init( );

		// we're done here
		return;
	}

	// fix velocity
	pNewAnimRecord->m_vecVelocity = ( pNewAnimRecord->m_vecOrigin - ( *pPreviousRecord.Xor( ) )->m_vecOrigin ) * ( 1.0f / TICKS_TO_TIME( pNewAnimRecord->m_iChokeTicks ) );

	// detect fakewalking players
	pNewAnimRecord->m_bIsFakewalking =
		pNewAnimRecord->m_vecVelocity.Length2D( ) > 0.1f
		&& pNewAnimRecord->m_pServerAnimOverlays[ 4 ].m_flWeight == 0.0f
		&& pNewAnimRecord->m_pServerAnimOverlays[ 5 ].m_flWeight == 0.0f
		&& pNewAnimRecord->m_pServerAnimOverlays[ 6 ].m_flPlaybackRate == 0.0f
		&& pNewAnimRecord->m_fFlags & FL_ONGROUND;

	// their server velocity will be zero
	if( pNewAnimRecord->m_bIsFakewalking )
		pNewAnimRecord->m_vecVelocity.Init( );

	// validate this shit
	pNewAnimRecord->m_vecVelocity.ValidateVector( );

	// detect players breaking teleport distance
	// https://github.com/perilouswithadollarsign/cstrike15_src/blob/master/game/server/player_lagcompensation.cpp#L384-L388
	pNewAnimRecord->m_bBrokeTeleportDst = pNewAnimRecord->m_vecOrigin.DistanceSquared( ( *pPreviousRecord.Xor( ) )->m_vecOrigin ) > XorFlt( 4096.0f );

	// shoutout esoterik & reis
	if( pNewAnimRecord->m_iChokeTicks > 1 && m_AnimationRecord.size( ) >= 2 ) {
		auto v490 = player->GetSequenceActivity( pNewAnimRecord->m_pServerAnimOverlays[ 5 ].m_nSequence );

		if( ( pNewAnimRecord->m_fFlags & FL_ONGROUND ) && ( ( *pPreviousRecord.Xor( ) )->m_fFlags & FL_ONGROUND ) ) {
			player->m_fFlags( ) |= FL_ONGROUND;
		}
		else {
			if( player->m_AnimOverlay( ).Count( ) <= 5
				|| pNewAnimRecord->m_pServerAnimOverlays[ 5 ].m_nSequence == ( *pPreviousRecord.Xor( ) )->m_pServerAnimOverlays[ 5 ].m_nSequence
				&& ( ( *pPreviousRecord.Xor( ) )->m_pServerAnimOverlays[ 5 ].m_flWeight != 0.0f
					 || pNewAnimRecord->m_pServerAnimOverlays[ 5 ].m_flWeight == 0.0f )
				|| !( v490 == ACT_CSGO_LAND_LIGHT || v490 == ACT_CSGO_LAND_HEAVY ) ) {
				if( ( pNewAnimRecord->m_fFlags & 1 ) != 0 && ( ( *pPreviousRecord.Xor( ) )->m_fFlags & FL_ONGROUND ) == 0 )
					player->m_fFlags( ) &= ~FL_ONGROUND;
			}
			else
				player->m_fFlags( ) |= FL_ONGROUND;
		}
	}
}

void AnimationData::Update( ) {
	if( !player || m_AnimationRecord.size( ) < 1 )
		return;

	C_CSPlayer *pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	auto pAnimationRecord = &m_AnimationRecord.front( );
	if( !pAnimationRecord )
		return;

	AnimationRecord *pPreviousAnimationRecord = nullptr;
	if( m_AnimationRecord.size( ) > 1 ) {
		pPreviousAnimationRecord = &m_AnimationRecord.at( 1 );
	}

	AnimationRecord *pPenultimateAnimationRecord = nullptr;
	if( m_AnimationRecord.size( ) > 2 ) {
		pPenultimateAnimationRecord = &m_AnimationRecord.at( 2 );
	}

	// simulate player animations
	SimulateAnimations( pAnimationRecord, pPreviousAnimationRecord, pPenultimateAnimationRecord );

	// restore server animlayers
	std::memcpy( player->m_AnimOverlay( ).Base( ), pAnimationRecord->m_pServerAnimOverlays.data( ), 13 * sizeof( C_AnimationLayer ) );

	player->m_AnimOverlay( )[ animstate_layer_t::ANIMATION_LAYER_LEAN ].m_flWeight = 0.f;

	// generate aimbot matrix
	player->SetupBones( pAnimationRecord->m_pMatrix, 128, 0x7FF00, player->m_flSimulationTime( ) );

	player->m_AnimOverlay( )[ animstate_layer_t::ANIMATION_LAYER_LEAN ].m_flWeight = pAnimationRecord->m_pServerAnimOverlays[ animstate_layer_t::ANIMATION_LAYER_LEAN ].m_flWeight;
}

void AnimationData::UpdatePlayer( C_CSPlayer *pEntity, AnimationRecord *pRecord, AnimationRecord *pPreviousRecord, AnimationRecord *pPenultimateRecord ) {
	const float flCurtime = g_pGlobalVars->curtime;
	const float flFrametime = g_pGlobalVars->frametime;

	CCSGOPlayerAnimState *pState = pEntity->m_PlayerAnimState( );
	if( !pState )
		return;

	// backup data before changes
	float flCurtimeBackup = g_pGlobalVars->curtime;
	float flFrametimeBackup = g_pGlobalVars->frametime;

	// calculate animations based on ticks aka server frames instead of render frames
	if( pRecord->m_iChokeTicks > 19 )
		g_pGlobalVars->curtime = pEntity->m_flSimulationTime( );
	else
		g_pGlobalVars->curtime = pRecord->m_flAnimationTime;

	g_pGlobalVars->frametime = g_pGlobalVars->interval_per_tick;

	pEntity->m_iEFlags( ) &= ~EFlags_t::EFL_DIRTY_ABSVELOCITY;

	if( pState->m_nLastUpdateFrame >= g_pGlobalVars->framecount )
		pState->m_nLastUpdateFrame = g_pGlobalVars->framecount - 1;

	// resolve player
	// todo - maxwell; don't resolve if pPreviousRecord is null?
	if( !pEntity->IsTeammate( C_CSPlayer::GetLocalPlayer( ) ) ) {
		g_Resolver.ResolvePlayers( pRecord, pPreviousRecord, pPenultimateRecord );
	}

	pEntity->m_bClientSideAnimation( ) = true;
	pEntity->UpdateClientSideAnimation( );
	pEntity->m_bClientSideAnimation( ) = false;

	pEntity->InvalidatePhysicsRecursive( ANGLES_CHANGED | ANIMATION_CHANGED | SEQUENCE_CHANGED );

	// restore globals
	g_pGlobalVars->curtime = flCurtimeBackup;
	g_pGlobalVars->frametime = flFrametimeBackup;
}

void AnimationData::SimulateMovement( C_CSPlayer *pEntity, Vector &vecOrigin, Vector &vecVelocity, int &fFlags, bool bOnGround ) {
	if( !( fFlags & FL_ONGROUND ) )
		vecVelocity.z -= TICKS_TO_TIME( g_Vars.sv_gravity->GetFloat( ) );
	else if( ( pEntity->m_fFlags( ) & FL_ONGROUND ) && !bOnGround )
		vecVelocity.z = g_Vars.sv_jump_impulse->GetFloat( );

	const auto src = vecOrigin;
	auto vecEnd = src + vecVelocity * g_pGlobalVars->interval_per_tick;

	Ray_t r;
	r.Init( src, vecEnd, pEntity->OBBMins( ), pEntity->OBBMaxs( ) );

	CGameTrace t{ };
	CTraceFilter filter;
	filter.pSkip = pEntity;

	g_pEngineTrace->TraceRay( r, CONTENTS_SOLID, &filter, &t );

	if( t.fraction != 1.f ) {
		for( auto i = 0; i < 2; i++ ) {
			vecVelocity -= t.plane.normal * vecVelocity.Dot( t.plane.normal );

			const auto flDot = vecVelocity.Dot( t.plane.normal );
			if( flDot < 0.f )
				vecVelocity -= Vector( flDot * t.plane.normal.x,
									   flDot * t.plane.normal.y, flDot * t.plane.normal.z );

			vecEnd = t.endpos + vecVelocity * TICKS_TO_TIME( 1.f - t.fraction );

			r.Init( t.endpos, vecEnd, pEntity->OBBMins( ), pEntity->OBBMaxs( ) );
			g_pEngineTrace->TraceRay( r, CONTENTS_SOLID, &filter, &t );

			if( t.fraction == 1.f )
				break;
		}
	}

	vecOrigin = vecEnd = t.endpos;
	vecEnd.z -= 2.f;

	r.Init( vecOrigin, vecEnd, pEntity->OBBMins( ), pEntity->OBBMaxs( ) );
	g_pEngineTrace->TraceRay( r, CONTENTS_SOLID, &filter, &t );

	fFlags &= ~FL_ONGROUND;

	if( t.DidHit( ) && t.plane.normal.z > .7f )
		fFlags |= FL_ONGROUND;
}

void AnimationData::SimulateAnimations( AnimationRecord *pRecord, AnimationRecord *pPreviousRecord, AnimationRecord *pPenultimateRecord ) {
	auto pState = player->m_PlayerAnimState( );
	if( !pState ) {
		return;
	}

	int m_fFlags;
	float m_flDuckAmount;
	float m_flPrimaryCycle;
	float m_flMoveWeight;
	Vector m_vecOrigin;

	m_fFlags = player->m_fFlags( );
	m_flDuckAmount = player->m_flDuckAmount( );
	m_vecOrigin = player->m_vecOrigin( );

	m_flPrimaryCycle = pState->m_flPrimaryCycle;
	m_flMoveWeight = pState->m_flMoveWeight;

	// nothing to work with..
	if( !pPreviousRecord ) {
		player->m_fFlags( ) = m_fFlags;
		player->m_flDuckAmount( ) = m_flDuckAmount;
		player->m_vecOrigin( ) = m_vecOrigin;

		pState->m_flPrimaryCycle = m_flPrimaryCycle;
		pState->m_flMoveWeight = m_flMoveWeight;

		UpdatePlayer( player, pRecord, nullptr, nullptr );
		return;
	}

	int fPreviousFlags = pPreviousRecord->m_fFlags;

	// backuP SHIT
	const float flBackupSimulationTime = player->m_flSimulationTime( );

	player->GetAbsVelocity( ) = pRecord->m_vecVelocity;
	player->m_vecVelocity( ) = pRecord->m_vecVelocity;
	player->SetAbsVelocity( pRecord->m_vecVelocity );
	player->SetAbsOrigin( pRecord->m_vecOrigin );
	player->m_fFlags( ) = pRecord->m_fFlags;
	player->m_angEyeAngles( ) = pRecord->m_angEyeAngles;
	player->m_flDuckAmount( ) = pRecord->m_flDuckAmount;

	// simulate player movement
	if( g_Vars.rage.fakelag_correction == 1 )
		SimulateMovement( player, pPreviousRecord->m_vecOrigin, player->m_vecVelocity( ), player->m_fFlags( ), fPreviousFlags & FL_ONGROUND );

	// store previous player flags, in order to simulate player movement the next tick
	fPreviousFlags = player->m_fFlags( );

	// simulate player
	UpdatePlayer( player, pRecord, pPreviousRecord, pPenultimateRecord );

	player->m_fFlags( ) = m_fFlags; 
	player->m_flDuckAmount( ) = m_flDuckAmount;
	player->m_vecOrigin( ) = m_vecOrigin;

	pState->m_flPrimaryCycle = m_flPrimaryCycle;
	pState->m_flMoveWeight = m_flMoveWeight;

	player->m_flSimulationTime( ) = flBackupSimulationTime;
}