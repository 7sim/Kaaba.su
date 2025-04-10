#include "EnginePrediction.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../source.hpp"
#include "../../SDK/Valve/CBaseHandle.hpp"
#include "../../SDK/displacement.hpp"
#include "../Rage/TickbaseShift.hpp"
#include "../../Hooking/Hooked.hpp"
#include "../Rage/ServerAnimations.hpp"
#include "../Miscellaneous/Movement.hpp"
#include <deque>

Prediction g_Prediction;

void Prediction::PreThink( CUserCmd* pCmd ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	auto pWeapon = ( C_WeaponCSBaseGun* )pLocal->m_hActiveWeapon( ).Get( );
	if( !pWeapon )
		return;

	// s/o esoterik
	// https://i.imgur.com/Z988Rkn.png
	if( m_nLastFrameStage == FRAME_NET_UPDATE_END/*
		&& m_nLastTickCount
		&& ( pCmd->command_number - m_nLastTickCount - 2) <= 148*/ )
	{
		g_pPrediction->Update( g_pClientState->m_nDeltaTick( ),
			g_pClientState->m_nDeltaTick( ) > 0,
			g_pClientState->m_nLastCommandAck( ),
			g_pClientState->m_nLastOutgoingCommand( ) + g_pClientState->m_nChokedCommands( ) );
	}

	m_flCurtime = g_pGlobalVars->curtime;
	m_flFrametime = g_pGlobalVars->frametime;

	m_fFlags = pLocal->m_fFlags( );
	m_vecVelocity = pLocal->m_vecVelocity( );
}

void Prediction::Think( CUserCmd* pCmd ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	auto pWeapon = ( C_WeaponCSBaseGun* )pLocal->m_hActiveWeapon( ).Get( );
	if( !pWeapon )
		return;

	g_pPrediction->m_in_prediction = true;

	g_pMoveHelper->SetHost( pLocal );

	*( CUserCmd** )( uintptr_t( pLocal ) + 0x3314 ) = 0;
	pLocal->m_PlayerCommand( ) = *pCmd;

	C_BaseEntity::SetPredictionRandomSeed( pCmd );
	C_BaseEntity::SetPredictionPlayer( pLocal );

	// Set globals appropriately
	g_pGlobalVars->curtime = pLocal->m_nTickBase( ) * g_pGlobalVars->interval_per_tick;
	g_pGlobalVars->frametime = g_pPrediction->m_engine_paused ? 0 : g_pGlobalVars->interval_per_tick;

	// Add and subtract buttons we're forcing on the player
	pCmd->buttons |= *reinterpret_cast< uint32_t* >( uint32_t( pLocal ) + 0x3310 );

	g_pGameMovement->StartTrackPredictionErrors( pLocal );

	// Do weapon selection
	if( pCmd->weaponselect != 0 )
	{
		if( pWeapon ) {
			auto weapon_data = pWeapon->GetCSWeaponData( );
			if( weapon_data.Xor( ) ) {
				// todo, SelectItem( ... )
			}
		}
	}

	// Latch in impulse.
	//IClientVehicle* pVehicle = player->GetVehicle( );
	if( pCmd->impulse )
	{
		// Discard impulse commands unless the vehicle allows them.
		// FIXME: UsingStandardWeapons seems like a bad filter for this. 
		// The flashlight is an impulse command, for example.
		if( /*!pVehicle || player->UsingStandardWeaponsInVehicle( )*/true )
		{
			// https://i.imgur.com/41YsK3G.png
			*( int* )( uintptr_t( pLocal ) + 0x31EC ) = pCmd->impulse;
			//player->m_nImpulse = ucmd->impulse;
		}
	}

	// Get button states
	int* nPlayerButtons = reinterpret_cast< int* >( uint32_t( pLocal ) + 0x31E8 );
	const int buttonsChanged = pCmd->buttons ^ *nPlayerButtons;

	// https://i.imgur.com/sN1glG9.png
	// inlined UpdateButtonState
	*( int* )( uintptr_t( pLocal ) + 0x31DC ) = *nPlayerButtons;
	*( int* )( uintptr_t( pLocal ) + 0x31E8 ) = pCmd->buttons;
	*( int* )( uintptr_t( pLocal ) + 0x31E0 ) = buttonsChanged & pCmd->buttons;  // The changed ones still down are "pressed"
	*( int* )( uintptr_t( pLocal ) + 0x31E4 ) = buttonsChanged & ~pCmd->buttons; // The ones not down are "released"

	// pLocal->UpdateButtonState( ucmd->buttons );

	// TODO
	g_pPrediction->CheckMovingGround( pLocal, g_pGlobalVars->frametime );

	// TODO
	//	g_pMoveData->m_vecOldAngles = player->pl.v_angle;

	// Copy from command to player unless game .dll has set angle using fixangle
	// if ( !player->pl.fixangle )
	{
		g_pPrediction->SetLocalViewAngles( pCmd->viewangles );
	}

	// Call standard client pre-think
	if( pLocal->PhysicsRunThink( 0 ) )
		pLocal->PreThink( );

	// Call Think if one is set
	const auto v19 = *( int* )( uint32_t( pLocal ) + 0xF8 );
	if( v19 != -1 && v19 > 0 && v19 <= pLocal->m_nTickBase( ) ) {
		pLocal->SetNextThink( -1 );
		pLocal->Think( );
	}

	// Setup input.
	g_pPrediction->SetupMove( pLocal, pCmd, g_pMoveHelper.Xor( ), &m_uMoveData );

	//VPROF_BUDGET( "CPrediction::ProcessMovement", "CPrediction::ProcessMovement" );

	// RUN MOVEMENT
	if( /*!pVehicle*/true )
	{
		Assert( g_pGameMovement );
		g_pGameMovement->ProcessMovement( pLocal, &m_uMoveData );
	}
	else
	{
		//pVehicle->ProcessMovement( player, g_pMoveData );
	}

	g_pPrediction->FinishMove( pLocal, pCmd, &m_uMoveData );

	// Let server invoke any needed impact functions
	//VPROF_SCOPE_BEGIN( "moveHelper->ProcessImpacts(cl)" );
	g_pMoveHelper->ProcessImpacts( );
	//VPROF_SCOPE_END( );

	//pLocal->PostThink( );
	pLocal->PostEntityThink( );

	g_pGameMovement->FinishTrackPredictionErrors( pLocal );

	*( CUserCmd** )( uintptr_t( pLocal ) + 0x3314 ) = 0;

	C_BaseEntity::SetPredictionRandomSeed( nullptr );
	C_BaseEntity::SetPredictionPlayer( nullptr );
	g_pGameMovement->Reset( );  // fixes a crash: when loading highlights twice or after previously loaded map, there was a dirty player pointer in game movement

	//if( !g_pPrediction->m_engine_paused && g_pGlobalVars->frametime > 0 ) {
	//	pLocal->m_nTickBase( )++;
	//}

	pWeapon->UpdateAccuracyPenalty( );

	m_flSpread = pWeapon->GetSpread( );
	m_flInaccuracy = pWeapon->GetInaccuracy( );
}

void Prediction::PostThink( CUserCmd* pCmd, bool* bSendPacket ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	g_pPrediction->m_in_prediction = false;

	auto pNetChannel = g_pEngine->GetNetChannelInfo( );
	if( !pNetChannel )
		return;

	if( *bSendPacket ) {
		m_nQueuedCommands.push_back( pCmd->command_number );
	}
	else {
		if( pNetChannel->m_nChokedPackets > 0 && !( pNetChannel->m_nChokedPackets % 4 ) ) {
			int nChokedPackets = pNetChannel->m_nChokedPackets;

			pNetChannel->m_nChokedPackets = 0;

			pNetChannel->SendDatagram( );

			pNetChannel->m_nOutSequenceNr--;
			pNetChannel->m_nChokedPackets = nChokedPackets;
		}
	}
}

void Prediction::OnPacketStart( void* ecx, int incoming_sequence, int outgoing_acknowledged ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );

	if( !pLocal || pLocal->IsDead( ) ) {
		g_Prediction.m_nQueuedCommands.clear( );
		return Hooked::oPacketStart( ecx, incoming_sequence, outgoing_acknowledged );
	}

	for( auto it = g_Prediction.m_nQueuedCommands.begin( ); it != g_Prediction.m_nQueuedCommands.end( ); it++ ) {
		if( *it == outgoing_acknowledged ) {
			g_Prediction.m_nQueuedCommands.erase( it );
			return Hooked::oPacketStart( ecx, incoming_sequence, outgoing_acknowledged );
		}
	}
}

void Prediction::StoreCompressionNetvars( ) {
	int          tickbase;
	CompressionData_t *data;

	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal || pLocal->IsDead( ) ) {
		return;
	}

	tickbase = pLocal->m_nTickBase( );

	// get current record and store data.
	data = &m_CompressionData[ tickbase % MULTIPLAYER_BACKUP ];

	data->m_nTickbase = tickbase;
	data->m_aimPunchAngle = pLocal->m_aimPunchAngle( );
	data->m_aimPunchAngleVel = pLocal->m_aimPunchAngleVel( );
	data->m_vecViewOffset = pLocal->m_vecViewOffset( );
	data->m_flVelocityModifier = pLocal->m_flVelocityModifier( );
}

void Prediction::ApplyCompressionNetvars( ) {
	int          tickbase;
	CompressionData_t *data;
	QAngle        punch_delta, punch_vel_delta;
	Vector       view_delta;
	float modifier_delta;
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal || pLocal->IsDead( ) ) {
		return;
	}

	tickbase = pLocal->m_nTickBase( );

	// get current record and validate.
	data = &m_CompressionData[ tickbase % MULTIPLAYER_BACKUP ];

	if( pLocal->m_nTickBase( ) != data->m_nTickbase )
		return;

	punch_delta = pLocal->m_aimPunchAngle( ) - data->m_aimPunchAngle;
	punch_vel_delta = pLocal->m_aimPunchAngleVel( ) - data->m_aimPunchAngleVel;
	view_delta = pLocal->m_vecViewOffset( ) - data->m_vecViewOffset;
	modifier_delta = pLocal->m_flVelocityModifier( ) - data->m_flVelocityModifier;

	// set data.
	if( std::abs( punch_delta.x ) < 0.03125f &&
		std::abs( punch_delta.y ) < 0.03125f &&
		std::abs( punch_delta.z ) < 0.03125f )
		pLocal->m_aimPunchAngle( ) = data->m_aimPunchAngle;

	if( std::abs( punch_vel_delta.x ) < 0.03125f &&
		std::abs( punch_vel_delta.y ) < 0.03125f &&
		std::abs( punch_vel_delta.z ) < 0.03125f )
		pLocal->m_aimPunchAngleVel( ) = data->m_aimPunchAngleVel;

	if( std::abs( view_delta.x ) < 0.03125f &&
		std::abs( view_delta.y ) < 0.03125f &&
		std::abs( view_delta.z ) < 0.03125f )
		pLocal->m_vecViewOffset( ) = data->m_vecViewOffset;

	if( std::abs( modifier_delta ) < 0.00625f )
		pLocal->m_flVelocityModifier( ) = data->m_flVelocityModifier;
}

void Prediction::CorrectViewModel( ClientFrameStage_t stage ) {
	if( !g_pEngine->IsInGame( ) )
		return;

	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( pLocal->IsDead( ) )
		return;

	// this works fine
	// but it could be improved/smoothed out
	// in some situations
	if( pLocal->m_hViewModel( ).IsValid( ) ) {
		const auto pViewmodel = pLocal->m_hViewModel( ).Get( );
		if( pViewmodel ) {
			static DWORD32 m_flCycle = SDK::Memory::FindInDataMap( pViewmodel->GetPredDescMap( ), XorStr( "m_flCycle" ) );
			static DWORD32 m_flAnimTime = SDK::Memory::FindInDataMap( pViewmodel->GetPredDescMap( ), XorStr( "m_flAnimTime" ) );

			// apply stored data
			if( stage == FRAME_NET_UPDATE_POSTDATAUPDATE_START ) {
				*( float* )( uintptr_t( pViewmodel ) + m_flCycle ) = m_flViewmodelCycle;
				*( float* )( uintptr_t( pViewmodel ) + m_flAnimTime ) = m_flViewmodelAnimTime;
			}

			// store data
			m_flViewmodelCycle = *( float* )( uintptr_t( pViewmodel ) + m_flCycle );
			m_flViewmodelAnimTime = *( float* )( uintptr_t( pViewmodel ) + m_flAnimTime );
		}
	}
}