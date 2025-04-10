#include "LagCompensation.hpp"
#include "../../source.hpp"
#include "../../SDK/Classes/player.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../SDK/sdk.hpp"
#include "../../SDK/Valve/CBaseHandle.hpp"

#include "../../SDK/variables.hpp"
#include "../../SDK/Displacement.hpp"
#include "../../SDK/core.hpp"
#include "../../Utils/extern/FnvHash.hpp"

#include "../Visuals/EventLogger.hpp"

#include "Autowall.hpp"

#include "../../Utils/Threading/Threading.h"
#include "../../Utils/Threading/shared_mutex.h"

#include "../../Utils/InputSys.hpp"

#include "FakeLag.hpp"
#include "../../Hooking/Hooked.hpp"

#include "AnimationSystem.hpp"
#include "TickbaseShift.hpp"

#include "AnimationSystem.hpp"

#include <sstream>

LagCompensation g_LagCompensation;

bool LagCompensation::BrokeTeleportDistance( int nEntIndex ) {
	auto pLagData = GetLagData( nEntIndex );
	if( !pLagData )
		return true;

	if( pLagData->m_Records.empty( ) )
		return true;

	return pLagData->m_Records.front( ).m_bBrokeTeleportDst;
}

C_EntityLagData *LagCompensation::GetLagData( int entindex ) {
	C_EntityLagData *data = nullptr;
	if( lagData.m_PlayerHistory.count( entindex ) > 0 )
		data = &lagData.m_PlayerHistory.at( entindex );

	return data;
}

float LagCompensation::GetLerp( ) {
	return lagData.m_flLerpTime;
}

void LagCompensation::ClearLagData( ) {
	lagData.m_PlayerHistory.clear( );
}

void LagCompensation::BackupOperation( bool restore ) {
	for( int i = 1; i <= g_pGlobalVars->maxClients; i++ ) 	{
		if( i == g_pEngine->GetLocalPlayer( ) )
			continue;

		auto player = C_CSPlayer::GetPlayerByIndex( i );
		if( !player || !player->IsPlayer( ) )
			continue;

		auto lag_data = GetLagData( i );

		if( !restore ) 		{
			lag_data->backup_record.Setup( player, true );
		}
		else 		{
			lag_data->backup_record.Apply( player );
		}
	}
}

void LagCompensation::Update( ) {
	if( !g_pEngine->IsInGame( ) || !g_pEngine->GetNetChannelInfo( ) ) {
		lagData.m_PlayerHistory.clear( );
		return;
	}

	auto local = C_CSPlayer::GetLocalPlayer( );
	if( !local )
		return;

	for( int i = 1; i <= g_pGlobalVars->maxClients; ++i ) {
		auto player = C_CSPlayer::GetPlayerByIndex( i );
		if( !player || player == local || !player->IsPlayer( ) )
			continue;

		player_info_t player_info;
		if( !g_pEngine->GetPlayerInfo( player->m_entIndex, &player_info ) ) {
			continue;
		}

		if( !player->GetClientRenderable( ) )
			continue;

		if( Hooked::player_hooks.count( player->m_entIndex ) < 1 )
			continue;

		auto pLagData = &lagData.m_PlayerHistory[ player->m_entIndex ];
		pLagData->UpdateRecordData( pLagData, player, player_info );
	}
}

bool LagCompensation::IsRecordOutOfBounds( const C_LagRecord &record, float flTargetTime, int nTickbaseShiftTicks, bool bDeadTimeCheck ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return true;

	auto pNetChannel = Encrypted_t<INetChannel>( g_pEngine->GetNetChannelInfo( ) );
	if( !pNetChannel.IsValid( ) )
		return true;

	float flTime = TICKS_TO_TIME( pLocal->m_nTickBase( ) );
	float flCorrect = lagData.m_flLerpTime + lagData.m_flOutLatency;
	flCorrect += lagData.m_flServerLatency;

	Math::Clamp( flCorrect, 0.f, g_Vars.sv_maxunlag->GetFloat( ) );

	return std::abs( flCorrect - ( flTime - record.m_flSimulationTime ) ) >= 0.19f;
}

void LagCompensation::SetupLerpTime( ) {
	lagData.m_flLerpTime = std::max( g_Vars.cl_interp->GetFloat( ), g_Vars.cl_interp_ratio->GetFloat( ) / g_Vars.cl_updaterate->GetFloat( ) );

	auto netchannel = Encrypted_t<INetChannel>( g_pEngine->GetNetChannelInfo( ) );
	if( !netchannel.IsValid( ) )
		return;

	lagData.m_flOutLatency = netchannel->GetLatency( FLOW_OUTGOING );
	lagData.m_flServerLatency = netchannel->GetLatency( FLOW_INCOMING );
}

void C_EntityLagData::UpdateRecordData( C_EntityLagData *pThis, C_CSPlayer *player, const player_info_t &info ) {
	auto local = C_CSPlayer::GetLocalPlayer( );
	auto team_check = g_Vars.rage.enabled && !g_Vars.rage.team_check && player->IsTeammate( C_CSPlayer::GetLocalPlayer( ) );
	if( player->IsDead( ) || team_check ) {
		pThis->m_Records.clear( );
		pThis->m_flLastUpdateTime = 0.0f;
		return;
	}

	bool isDormant = player->IsDormant( );

	// no need to store insane amount of data
	while( pThis->m_Records.size( ) > int( 1.0f / g_pGlobalVars->interval_per_tick ) ) {
		pThis->m_Records.pop_back( );
	}

	if( isDormant ) {
		pThis->m_flLastUpdateTime = 0.0f;
		if( pThis->m_Records.size( ) > 0 && ( pThis->m_Records.front( ).m_bBrokeTeleportDst ) ) {
			pThis->m_Records.clear( );
		}

		return;
	}

	if( info.userId != pThis->m_iUserID ) {
		pThis->Clear( );
		pThis->m_iUserID = info.userId;
	}

	// did player update?
	float simTime = player->m_flSimulationTime( );
	if( pThis->m_flLastUpdateTime >= simTime ) {
		return;
	}

	if( player->m_flOldSimulationTime( ) > simTime ) {
		return;
	}

	auto pAnimData = g_AnimationSystem.GetAnimationData( player->m_entIndex );
	if( !pAnimData )
		return;

	if( pAnimData->m_AnimationRecord.empty( ) )
		return;

	auto pAnimRecord = &pAnimData->m_AnimationRecord.front( );

	if( !pAnimRecord )
		return;

	pThis->m_flLastUpdateTime = simTime;

	bool isTeam = local->IsTeammate( player );

	// add new pNewRecord and get reference to newly added pNewRecord.
	auto pNewRecord = &pThis->m_Records.emplace_front( );

	pNewRecord->Setup( player );
	pNewRecord->m_flRealTime = g_pEngine->GetLastTimeStamp( );
	pNewRecord->m_flServerLatency = g_LagCompensation.m_flServerLatency + g_LagCompensation.m_flOutLatency; // HHHHH
	pNewRecord->m_flDuckAmount = pAnimRecord->m_flDuckAmount;
	pNewRecord->m_angEyeAngles = pAnimRecord->m_angEyeAngles;
	pNewRecord->m_vecVelocity = pAnimRecord->m_vecVelocity;
	pNewRecord->m_bLBYFlicked = pAnimRecord->m_bLBYFlicked;
	pNewRecord->m_bBrokeTeleportDst = pAnimRecord->m_bBrokeTeleportDst;
	pNewRecord->m_bIsFakewalking = pAnimRecord->m_bIsFakewalking;
	pNewRecord->m_bIsInvalid = pAnimRecord->m_bIsInvalid;
	pNewRecord->m_bIsResolved = pAnimRecord->m_bIsResolved;
	pNewRecord->m_flAnimationVelocity = player->m_PlayerAnimState( )->m_flVelocityLengthXY;
	pNewRecord->m_iLaggedTicks = TIME_TO_TICKS( player->m_flSimulationTime( ) - player->m_flOldSimulationTime( ) );

	if( pAnimRecord->m_pMatrix )
		std::memcpy( pNewRecord->m_pMatrix, pAnimRecord->m_pMatrix, player->m_CachedBoneData( ).Count( ) * sizeof( matrix3x4_t ) );
}

void C_EntityLagData::Clear( ) {
	this->m_Records.clear( );
	m_iUserID = -1;
	m_flLastUpdateTime = 0.0f;
}

void C_LagRecord::Setup( C_CSPlayer *pPlayer, bool isBackup ) {
	if( !pPlayer )
		return;

	auto collidable = pPlayer->m_Collision( );
	if( collidable ) {
		m_vecMins = collidable->m_vecMins;
		m_vecMaxs = collidable->m_vecMaxs;
	}

	m_vecOrigin = pPlayer->m_vecOrigin( );
	m_angEyeAngles = pPlayer->m_angEyeAngles( );
	m_vecVelocity = pPlayer->m_vecVelocity( );
	m_flSimulationTime = pPlayer->m_flSimulationTime( );
	m_absAngles = pPlayer->GetAbsAngles( );
	m_flLowerBodyYawTarget = pPlayer->m_flLowerBodyYawTarget( );

	m_iFlags = pPlayer->m_fFlags( );

	std::memcpy( m_pMatrix, pPlayer->m_CachedBoneData( ).Base( ),
				 pPlayer->m_CachedBoneData( ).Count( ) * sizeof( matrix3x4_t ) );

	this->player = pPlayer;
}

void C_LagRecord::Apply( C_CSPlayer *pPlayer ) {
	if( !pPlayer || pPlayer == nullptr )
		return;

	if( !this->player || this->player == nullptr || !this->m_flSimulationTime )
		return;

	auto collidable = this->player->m_Collision( );

	if( collidable != nullptr )
		collidable->SetCollisionBounds( this->m_vecMins, this->m_vecMaxs );

	this->player->m_flSimulationTime( ) = m_flSimulationTime;
	this->player->m_fFlags( ) = m_iFlags;
	this->player->SetAbsOrigin( m_vecOrigin );
	this->player->SetAbsAngles( m_absAngles );
	this->player->m_angEyeAngles( ) = m_angEyeAngles;

	if( m_pMatrix ) {
		std::memcpy( this->player->m_CachedBoneData( ).Base( ), m_pMatrix,
					 this->player->m_CachedBoneData( ).Count( ) * sizeof( matrix3x4_t ) );

		// force bone cache
		this->player->m_iMostRecentModelBoneCounter( ) = *( int * )Engine::Displacement.Data.m_uModelBoneCounter;
		this->player->m_BoneAccessor( ).m_ReadableBones = this->player->m_BoneAccessor( ).m_WritableBones = 0xFFFFFFFF;
		this->player->m_flLastBoneSetupTime( ) = FLT_MAX;
	}
}