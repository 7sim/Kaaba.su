#include "Autowall.hpp"
#include "../../SDK/Displacement.hpp"
#include "../../SDK/Classes/player.hpp"
#include "../../SDK/Classes/weapon.hpp"

#include "../../Loader/Security/Security.hpp"

// IsBreakableEntity
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/sp/src/game/shared/obstacle_pushaway.cpp
bool Autowall::IsBreakable( C_BaseEntity *pEntity ) {
	if( !pEntity || pEntity->m_entIndex == 0 || !pEntity->GetCollideable( ) )
		return false;

	static uintptr_t uTakeDamage = *( uintptr_t * )( ( uintptr_t )Engine::Displacement.Function.m_uIsBreakable + 38 );
	uintptr_t uTakeDamageBackup = *( uint8_t * )( ( uintptr_t )pEntity + uTakeDamage );

	const ClientClass *pClientClass = pEntity->GetClientClass( );
	if( pClientClass ) {
		const char *name = pClientClass->m_pNetworkName;
		if( !strcmp( name, "CBreakableSurface" ) )
			*( uint8_t * )( ( uintptr_t )pEntity + uTakeDamage ) = 2;
		else if( !strcmp( name, "CBaseDoor" ) || !strcmp( name, "CDynamicProp" ) )
			*( uint8_t * )( ( uintptr_t )pEntity + uTakeDamage ) = 0;
	}

	using fnIsBreakable = bool( __thiscall * )( C_BaseEntity * );
	bool bResult = ( ( fnIsBreakable )Engine::Displacement.Function.m_uIsBreakable )( pEntity );
	*( uint8_t * )( ( uintptr_t )pEntity + uTakeDamage ) = uTakeDamageBackup;

	return bResult;
}

bool Autowall::IsArmored( C_CSPlayer *player, int nHitgroup ) {
	const bool bHasHelmet = player->m_bHasHelmet( );
	const bool bHasHeavyArmor = player->m_bHasHeavyArmor( );
	const float flArmorValue = player->m_ArmorValue( );

	if( flArmorValue > 0 ) {
		switch( nHitgroup ) {
			case Hitgroup_Generic:
			case Hitgroup_Chest:
			case Hitgroup_Stomach:
			case Hitgroup_LeftArm:
			case Hitgroup_RightArm:
			case Hitgroup_LeftLeg: // is leg armored?
			case Hitgroup_RightLeg:
			case Hitgroup_Gear:
				return true;
				break;
			case Hitgroup_Head:
				return bHasHelmet || bHasHeavyArmor;
				break;
			default:
				return bHasHeavyArmor;
				break;
		}
	}

	return false;
}

// references CCSPlayer::TraceAttack and CCSPlayer::OnTakeDamage
float Autowall::ScaleDamage( C_CSPlayer *player, float flDamage, float flArmorRatio, int nHitgroup ) {
	if( !player )
		return -1.f;

	C_CSPlayer *pLocal = C_CSPlayer::GetLocalPlayer( );

	if( !pLocal )
		return -1.f;

	C_WeaponCSBaseGun *pWeapon = ( C_WeaponCSBaseGun * )pLocal->m_hActiveWeapon( ).Get( );

	if( !pWeapon )
		return -1.f;

	const int nTeamNum = player->m_iTeamNum( );
	float flHeadDamageScale = nTeamNum == TEAM_CT ? g_Vars.mp_damage_scale_ct_head->GetFloat( ) : g_Vars.mp_damage_scale_t_head->GetFloat( );
	const float flBodyDamageScale = nTeamNum == TEAM_CT ? g_Vars.mp_damage_scale_ct_body->GetFloat( ) : g_Vars.mp_damage_scale_t_body->GetFloat( );

	const bool bIsArmored = IsArmored( player, nHitgroup );
	const bool bHasHeavyArmor = player->m_bHasHeavyArmor( );
	const bool bIsZeus = pWeapon->m_iItemDefinitionIndex( ) == WEAPON_ZEUS;

	const float flArmorValue = static_cast< float >( player->m_ArmorValue( ) );

	if( bHasHeavyArmor )
		flHeadDamageScale *= 0.5f;

	if( !bIsZeus ) {
		switch( nHitgroup ) {
			case Hitgroup_Head:
				flDamage = ( flDamage * 4.f ) * flHeadDamageScale;
				break;
			case Hitgroup_Stomach:
				flDamage = ( flDamage * 1.25f ) * flBodyDamageScale;
				break;
			case Hitgroup_Chest:
			case Hitgroup_LeftArm:
			case Hitgroup_RightArm:
			case Hitgroup_Gear:
				flDamage = flDamage * flBodyDamageScale;
				break;
			case Hitgroup_LeftLeg:
			case Hitgroup_RightLeg:
				flDamage = ( flDamage * 0.75f ) * flBodyDamageScale;
				break;
			default:
				break;
		}
	}

	// enemy have armor
	if( bIsArmored ) {
		float flArmorScale = 1.f;
		float flArmorBonusRatio = 0.5f;
		float flArmorRatioCalculated = flArmorRatio * 0.5f;
		float fDamageToHealth = 0.f;

		if( bHasHeavyArmor ) {
			flArmorRatioCalculated = flArmorRatio * 0.25f;
			flArmorBonusRatio = 0.33f;

			flArmorScale = 0.33f;

			fDamageToHealth = ( flDamage * flArmorRatioCalculated ) * 0.85f;
		}
		else {
			fDamageToHealth = flDamage * flArmorRatioCalculated;
		}

		float fDamageToArmor = ( flDamage - fDamageToHealth ) * ( flArmorScale * flArmorBonusRatio );

		// Does this use more armor than we have?
		if( fDamageToArmor > flArmorValue )
			fDamageToHealth = flDamage - ( flArmorValue / flArmorBonusRatio );

		flDamage = fDamageToHealth;
	}

	flDamage = std::floorf( flDamage );
	return flDamage;
}

void Autowall::TraceLine( const Vector &start, const Vector &end, uint32_t mask, ITraceFilter *ignore, CGameTrace *ptr ) {
	Ray_t ray;
	ray.Init( start, end );
	g_pEngineTrace->TraceRay( ray, mask, ignore, ptr );
}

__forceinline float DistanceToRay( const Vector &vecPosition, const Vector &vecRayStart, const Vector &vecRayEnd, float *flAlong = NULL, Vector *vecPointOnRay = NULL ) {
	Vector vecTo = vecPosition - vecRayStart;
	Vector vecDir = vecRayEnd - vecRayStart;
	float flLength = vecDir.Normalize( );

	float flRangeAlong = DotProduct( vecDir, vecTo );
	if( flAlong ) {
		*flAlong = flRangeAlong;
	}

	float flRange;

	if( flRangeAlong < 0.0f ) {
		// off start point
		flRange = -vecTo.Length( );

		if( vecPointOnRay ) {
			*vecPointOnRay = vecRayStart;
		}
	}
	else if( flRangeAlong > flLength ) {
		// off end point
		flRange = -( vecPosition - vecRayEnd ).Length( );

		if( vecPointOnRay ) {
			*vecPointOnRay = vecRayEnd;
		}
	}
	else { // within ray bounds
		Vector vecOnRay = vecRayStart + vecDir * flRangeAlong;
		flRange = ( vecPosition - vecOnRay ).Length( );

		if( vecPointOnRay ) {
			*vecPointOnRay = vecOnRay;
		}
	}

	return flRange;
}

void Autowall::ClipTraceToPlayer( const Vector vecAbsStart, const Vector &vecAbsEnd, uint32_t iMask, ITraceFilter *pFilter, CGameTrace *pGameTrace, C_FireBulletData *pData ) {
	if( !pData )
		return;

	constexpr float flMaxRange = 60.0f, flMinRange = 0.0f;

	ICollideable *pCollideble = pData->m_TargetPlayer->GetCollideable( );

	if( !pCollideble )
		return;

	// get bounding box
	const Vector vecObbMins = pCollideble->OBBMins( );
	const Vector vecObbMaxs = pCollideble->OBBMaxs( );
	const Vector vecObbCenter = ( vecObbMaxs + vecObbMins ) / 2.f;

	// calculate world space center
	const Vector vecPosition = vecObbCenter + pData->m_TargetPlayer->GetAbsOrigin( );

	Ray_t Ray;
	Ray.Init( vecAbsStart, vecAbsEnd );

	// calculate distance to ray
	const float flRange = DistanceToRay( vecPosition, vecAbsStart, vecAbsEnd );

	if( flRange < 0.0f || flRange > 60.0f )
		return;

#ifndef DEV
	VIRTUALIZER_TIGER_WHITE_START
		if( g_is_cracked ) {
			pData->m_flCurrentDamage = 0.f;
		}
	VIRTUALIZER_TIGER_WHITE_END
	#endif

		CGameTrace playerTrace;
	g_pEngineTrace->ClipRayToEntity( Ray, iMask, pData->m_TargetPlayer, &playerTrace );
	if( pData->m_EnterTrace.fraction > playerTrace.fraction )
		pData->m_EnterTrace = playerTrace;
}

void Autowall::ClipTraceToPlayers( const Vector &vecAbsStart, const Vector &vecAbsEnd, uint32_t iMask, ITraceFilter *pFilter, CGameTrace *pGameTrace, float flMaxRange, float flMinRange ) {
	float flSmallestFraction = pGameTrace->fraction;

	Vector vecDelta( vecAbsEnd - vecAbsStart );
	const float flDelta = vecDelta.Normalize( );

	Ray_t Ray;
	Ray.Init( vecAbsStart, vecAbsEnd );

	for( int i = 1; i <= g_pGlobalVars->maxClients; ++i ) {
		C_CSPlayer *pPlayer = C_CSPlayer::GetPlayerByIndex( i );
		if( !pPlayer || pPlayer->IsDormant( ) || pPlayer->IsDead( ) )
			continue;

		if( pFilter && !pFilter->ShouldHitEntity( pPlayer, iMask ) )
			continue;

		ICollideable *pCollideble = pPlayer->GetCollideable( );
		if( !pCollideble )
			continue;

		// get bounding box
		const Vector vecObbMins = pCollideble->OBBMins( );
		const Vector vecObbMaxs = pCollideble->OBBMaxs( );
		const Vector vecObbCenter = ( vecObbMaxs + vecObbMins ) / 2.f;

		// calculate world space center
		const Vector vecPosition = vecObbCenter + pPlayer->GetAbsOrigin( );

		// calculate distance to ray
		const float flRange = DistanceToRay( vecPosition, vecAbsStart, vecAbsEnd );

		if( flRange < flMinRange || flRange > flMaxRange )
			return;

		CGameTrace playerTrace;
		g_pEngineTrace->ClipRayToEntity( Ray, iMask, pPlayer, &playerTrace );
		if( playerTrace.fraction < flSmallestFraction ) {
			// we shortened the ray - save off the trace
			*pGameTrace = playerTrace;
			flSmallestFraction = playerTrace.fraction;
		}
	}
}

bool Autowall::TraceToExit( CGameTrace *pEnterTrace, Vector vecStartPos, Vector vecDirection, CGameTrace *pExitTrace ) {
	constexpr float flMaxDistance = 90.f, flStepSize = 4.f;
	float flCurrentDistance = 0.f;

	int iFirstContents = 0;

	bool bIsWindow = 0;
	auto v23 = 0;

	do {
		// Add extra distance to our ray
		flCurrentDistance += flStepSize;

		// Multiply the direction vector to the distance so we go outwards, add our position to it.
		Vector vecEnd = vecStartPos + ( vecDirection * flCurrentDistance );

		int iPointContents = g_pEngineTrace->GetPointContents( vecEnd, CS_MASK_SHOOT | CONTENTS_HITBOX );
		if( !iFirstContents )
			iFirstContents = iPointContents;

		if( !( iPointContents & CS_MASK_SHOOT ) || ( ( iPointContents & CONTENTS_HITBOX ) && iPointContents != iFirstContents ) ) {
			//Let's setup our end position by deducting the direction by the extra added distance
			Vector vecStart = vecEnd - ( vecDirection * flStepSize );

			// this gets a bit more complicated and expensive when we have to deal with displacements
			TraceLine( vecEnd, vecStart, CS_MASK_SHOOT | CONTENTS_HITBOX, nullptr, pExitTrace );

			// we exited the wall into a player's hitbox
			if( pExitTrace->startsolid && pExitTrace->surface.flags & SURF_HITBOX ) {
				uint32_t filter_[ 4 ] = { *reinterpret_cast< uint32_t * > ( Engine::Displacement.Function.m_TraceFilterSimple ), uint32_t( C_CSPlayer::GetLocalPlayer( ) ), 0, 0 };
				filter_[ 1 ] = reinterpret_cast< uint32_t >( pExitTrace->hit_entity );

				// do another trace, but skip the player to get the actual exit surface 
				TraceLine( vecEnd, vecStartPos, CS_MASK_SHOOT, reinterpret_cast< CTraceFilter * >( filter_ ), pExitTrace );

				if( pExitTrace->DidHit( ) && !pExitTrace->startsolid ) {
					vecEnd = pExitTrace->endpos;
					return true;
				}

				continue;
			}

			if( pExitTrace->DidHit( ) && !pExitTrace->startsolid ) {
				bool bStartIsNodraw = !!( pEnterTrace->surface.flags & ( SURF_NODRAW ) );
				bool bExitIsNodraw = !!( pExitTrace->surface.flags & ( SURF_NODRAW ) );

				if( bExitIsNodraw && IsBreakable( reinterpret_cast< C_BaseEntity * >( pExitTrace->hit_entity ) ) && IsBreakable( reinterpret_cast< C_BaseEntity * >( pEnterTrace->hit_entity ) ) ) {
					// we have a case where we have a breakable object, but the mapper put a nodraw on the backside
					vecEnd = pExitTrace->endpos;
					return true;
				}
				else if( bExitIsNodraw == false || ( bStartIsNodraw && bExitIsNodraw ) ) // exit nodraw is only valid if our entrace is also nodraw
				{
					Vector vecNormal = pExitTrace->plane.normal;
					float flDot = vecDirection.Dot( vecNormal );
					if( flDot <= 1.0f ) {
						// get the real end pos
						vecEnd = vecEnd - ( vecDirection * ( flStepSize * pExitTrace->fraction ) );
						return true;
					}
				}

				continue;
			}

			if( pEnterTrace->DidHitNonWorldEntity( ) && IsBreakable( reinterpret_cast< C_BaseEntity * >( pEnterTrace->hit_entity ) ) ) {
				// if we hit a breakable, make the assumption that we broke it if we can't find an exit (hopefully..)
				// fake the end pos
				pExitTrace = pEnterTrace;
				pExitTrace->endpos = vecStartPos + vecDirection;
				return true;
			}
		}
		// max pen distance is 90 units.
	} while( flCurrentDistance <= flMaxDistance );

	return false;
}

bool Autowall::HandleBulletPenetration( C_FireBulletData *data ) {
	if( !data )
		return true;

	if( !data->m_EnterSurfaceData )
		return true;

	int iEnterMaterial = data->m_EnterSurfaceData->game.material;
	const int nPenetrationSystem = g_Vars.sv_penetration_type->GetInt( );

	bool bSolidSurf = ( ( data->m_EnterTrace.contents >> 3 ) & CONTENTS_SOLID );
	bool bLightSurf = ( ( data->m_EnterTrace.surface.flags >> 7 ) & SURF_LIGHT );
	bool bContentsGrate = data->m_EnterTrace.contents & CONTENTS_GRATE;
	bool bNoDrawSurf = !!( data->m_EnterTrace.surface.flags & ( SURF_NODRAW ) ); // this is valve code :D!

	// check if bullet can penetrarte another entity
	if( data->m_iPenetrationCount == 0 &&
		!bContentsGrate &&
		!bNoDrawSurf &&
		iEnterMaterial != CHAR_TEX_GRATE &&
		iEnterMaterial != CHAR_TEX_GLASS )
		return true; // no, stop

	 // if we hit a grate with iPenetration == 0, stop on the next thing we hit
	if( data->m_WeaponData->m_flPenetration <= 0.f || data->m_iPenetrationCount == 0 )
		return true;

	// find exact penetration exit
	CGameTrace ExitTrace = { };
	if( !TraceToExit( &data->m_EnterTrace, data->m_EnterTrace.endpos, data->m_vecDirection, &ExitTrace ) ) {
		// ended in solid
		if( ( g_pEngineTrace->GetPointContents( data->m_EnterTrace.endpos, MASK_SHOT_HULL ) & MASK_SHOT_HULL ) == 0 )
			return true;
	}

	const surfacedata_t *pExitSurfaceData = g_pPhysSurface->GetSurfaceData( ExitTrace.surface.surfaceProps );

	if( !pExitSurfaceData )
		return true;

	const float flEnterPenetrationModifier = data->m_EnterSurfaceData->game.flPenetrationModifier;
	const float flExitPenetrationModifier = pExitSurfaceData->game.flPenetrationModifier;
	const float flExitDamageModifier = pExitSurfaceData->game.flDamageModifier;

	const int iExitMaterial = pExitSurfaceData->game.material;

	float flDamageModifier = 0.f;
	float flPenetrationModifier = 0.f;

	// percent of total damage lost automatically on impacting a surface
	flDamageModifier = 0.16f;
	flPenetrationModifier = ( flEnterPenetrationModifier + flExitPenetrationModifier ) * 0.5f;

	// new penetration method
	if( nPenetrationSystem == 1 ) {
		if( iEnterMaterial == CHAR_TEX_GRATE || iEnterMaterial == CHAR_TEX_GLASS ) {
			flDamageModifier = 0.05f;
			flPenetrationModifier = 3.0f;
		}
		else if( bSolidSurf || bLightSurf ) {
			flDamageModifier = 0.16f;
			flPenetrationModifier = 1.0f;
		}
		// for some weird reason some community servers have ff_damage_reduction_bullets > 0 but ff_damage_bullet_penetration == 0
		// so yeah, no shooting through teammates :)
		else if( iEnterMaterial == CHAR_TEX_FLESH && ( data->m_Player->IsTeammate( ( C_CSPlayer * )( data->m_EnterTrace.hit_entity ) ) ) &&
				 g_Vars.ff_damage_reduction_bullets->GetFloat( ) >= 0.f ) {
			//Look's like you aren't shooting through your teammate today
			if( g_Vars.ff_damage_bullet_penetration->GetFloat( ) == 0.f )
				return true;

			//Let's shoot through teammates and get kicked for teamdmg! Whatever, atleast we did damage to the enemy. I call that a win.
			flPenetrationModifier = g_Vars.ff_damage_bullet_penetration->GetFloat( );
			flDamageModifier = 0.16f;
		}
		else {
			// percent of total damage lost automatically on impacting a surface
			flDamageModifier = 0.16f;
			flPenetrationModifier = ( flEnterPenetrationModifier + flExitPenetrationModifier ) * 0.5f;
		}

		// if enter & exit point is wood we assume this is 
		// a hollow crate and give a penetration bonus
		if( iEnterMaterial == iExitMaterial ) {
			if( iExitMaterial == CHAR_TEX_WOOD || iExitMaterial == CHAR_TEX_CARDBOARD )
				flPenetrationModifier = 3.f;
			else if( iExitMaterial == CHAR_TEX_PLASTIC )
				flPenetrationModifier = 2.f;
		}

		// calculate damage  
		float flTraceDistance = ( ExitTrace.endpos - data->m_EnterTrace.endpos ).Length( );
		float flPenetrationMod = std::max( 0.0f, 1.0f / flPenetrationModifier );

		flTraceDistance = flTraceDistance * flTraceDistance * flPenetrationMod * 0.041666668f;

		auto flLostDamage = std::max( 0.0f, 3.0f / data->m_WeaponData->m_flPenetration * 1.25f ) * flPenetrationMod * 3.0f + data->m_flCurrentDamage * flDamageModifier + flTraceDistance;

		const float flClampedLostDamage = fmaxf( flLostDamage, 0.f );

		if( flClampedLostDamage > data->m_flCurrentDamage )
			return true;

		// reduce damage power each time we hit something other than a grate
		if( flClampedLostDamage > 0.0f )
			data->m_flCurrentDamage -= flClampedLostDamage;

		// do we still have enough damage to deal?
		if( data->m_flCurrentDamage < 1.0f )
			return true;

		// penetration was successful
		// setup new start end parameters for successive trace
		data->m_vecStart = ExitTrace.endpos;
		--data->m_iPenetrationCount;
		return false;
	}
	else {
		// since some railings in de_inferno are CONTENTS_GRATE but CHAR_TEX_CONCRETE, we'll trust the
		// CONTENTS_GRATE and use a high damage modifier.
		if( bContentsGrate || bNoDrawSurf ) {
			// If we're a concrete grate (TOOLS/TOOLSINVISIBLE texture) allow more penetrating power.
			flPenetrationModifier = 1.0f;
			flDamageModifier = 0.99f;
		}
		else {
			if( flExitPenetrationModifier < flPenetrationModifier ) {
				flPenetrationModifier = flExitPenetrationModifier;
			}
			if( flExitDamageModifier < flDamageModifier ) {
				flDamageModifier = flExitDamageModifier;
			}
		}

		// if enter & exit point is wood or metal we assume this is 
		// a hollow crate or barrel and give a penetration bonus
		if( iEnterMaterial == iExitMaterial ) {
			if( iExitMaterial == CHAR_TEX_WOOD ||
				iExitMaterial == CHAR_TEX_METAL ) {
				flPenetrationModifier *= 2;
			}
		}

		float flTraceDistance = ( ExitTrace.endpos - data->m_EnterTrace.endpos ).Length( );

		// check if bullet has enough power to penetrate this distance for this material
		if( flTraceDistance > ( data->m_WeaponData->m_flPenetration * flPenetrationModifier ) )
			return true; // bullet hasn't enough power to penetrate this distance

		 // reduce damage power each time we hit something other than a grate
		data->m_flCurrentDamage *= flDamageModifier;

		// penetration was successful
		// setup new start end parameters for successive trace
		data->m_vecStart = ExitTrace.endpos;
		--data->m_iPenetrationCount;
		return false;
	}
}

float Autowall::FireBullets( C_FireBulletData *data ) {
	if( !g_pEngine->IsInGame( ) || !g_pEngine->IsConnected( ) )
		return -1.f;

	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return -1.f;

	if( pLocal->IsDead( ) )
		return -1.f;

	constexpr float rayExtension = 40.f;

	if( !data )
		return -1.f;

	//This gets set in FX_Firebullets to 4 as a pass-through value.
	//CS:GO has a maximum of 4 surfaces a bullet can pass-through before it 100% stops.
	//Excerpt from Valve: https://steamcommunity.com/sharedfiles/filedetails/?id=275573090
	//"The total number of surfaces any bullet can penetrate in a single flight is capped at 4." -CS:GO Official

	if( !data->m_Weapon ) {
		data->m_Weapon = ( C_WeaponCSBaseGun * )( data->m_Player->m_hActiveWeapon( ).Get( ) );
		if( data->m_Weapon ) {
			data->m_WeaponData = data->m_Weapon->GetCSWeaponData( ).Xor( );
		}
	}

	data->m_flTraceLength = 0.f;
	data->m_flCurrentDamage = static_cast< float >( data->m_WeaponData->m_iWeaponDamage );

	CTraceFilter TraceFilter;
	TraceFilter.pSkip = data->m_Player;

	if( !data->m_Filter )
		data->m_Filter = &TraceFilter;

	data->m_flMaxLength = data->m_WeaponData->m_flWeaponRange;

	while( data->m_flCurrentDamage > 0.f ) {
		if( !data )
			break;

		if( pLocal->IsDead( ) )
			break;

		if( !data->m_Weapon || !data->m_WeaponData || !data->m_Player )
			break;

		// calculate max bullet range
		data->m_flMaxLength -= data->m_flTraceLength;

		//if( data->m_iPenetrationCount < 4 ) {
		//	data->m_flCurrentDamage -= 1.f;
		//}

		// create end point of bullet
		Vector vecEnd = data->m_vecStart + data->m_vecDirection * data->m_flMaxLength;

		TraceLine( data->m_vecStart, vecEnd, MASK_SHOT_PLAYER, &TraceFilter, &data->m_EnterTrace );

		// create extended end point
		Vector vecEndExtended = vecEnd + data->m_vecDirection * rayExtension;

		// NOTICE: can remove valve`s hack aka bounding box fix
		// Check for player hitboxes extending outside their collision bounds
		if( data->m_TargetPlayer ) {
			// clip trace to one player
			ClipTraceToPlayer( data->m_vecStart, vecEndExtended, MASK_SHOT_PLAYER, data->m_Filter, &data->m_EnterTrace, data );
		}
		else {
			ClipTraceToPlayers( data->m_vecStart, vecEndExtended, MASK_SHOT_PLAYER, data->m_Filter, &data->m_EnterTrace );
		}

		if( data->m_EnterTrace.fraction == 1.f )
			break;  // we didn't hit anything, stop tracing shoot

		 //calculate the damage based on the distance the bullet traveled.
		data->m_flTraceLength += data->m_EnterTrace.fraction * data->m_flMaxLength;

		//Let's make our damage drops off the further away the bullet is.
		if( !data->m_bShouldIgnoreDistance )
			data->m_flCurrentDamage *= powf( data->m_WeaponData->m_flRangeModifier, data->m_flTraceLength * 0.002f );

		C_CSPlayer *pHittedPlayer = ToCSPlayer( ( C_BasePlayer * )data->m_EnterTrace.hit_entity );

		const int nHitGroup = data->m_EnterTrace.hitgroup;
		const bool bHitgroupIsValid = data->m_Weapon->m_iItemDefinitionIndex( ) == WEAPON_ZEUS ? ( nHitGroup >= Hitgroup_Generic && nHitGroup <= Hitgroup_Neck ) : ( nHitGroup >= Hitgroup_Generic && nHitGroup <= Hitgroup_Neck );
		const bool bTargetIsValid = !data->m_TargetPlayer || ( pHittedPlayer != nullptr && pHittedPlayer->m_entIndex == data->m_TargetPlayer->m_entIndex );
		if( pHittedPlayer != nullptr ) {
			if( bTargetIsValid && bHitgroupIsValid && pHittedPlayer->IsPlayer( ) && pHittedPlayer->m_entIndex <= g_pGlobalVars->maxClients && pHittedPlayer->m_entIndex > 0 ) {
				data->m_flCurrentDamage = ScaleDamage( pHittedPlayer, data->m_flCurrentDamage, data->m_WeaponData->m_flArmorRatio, data->m_Weapon->m_iItemDefinitionIndex( ) == WEAPON_ZEUS ? Hitgroup_Generic : nHitGroup );
				data->m_iHitgroup = nHitGroup;
				return data->m_flCurrentDamage;
			}
		}

		bool bCanPenetrate = data->m_bPenetration;
		if( !data->m_bPenetration )
			bCanPenetrate = data->m_EnterTrace.contents & CONTENTS_WINDOW;

		if( !bCanPenetrate )
			break;

		data->m_EnterSurfaceData = g_pPhysSurface->GetSurfaceData( data->m_EnterTrace.surface.surfaceProps );

		if( !data->m_EnterSurfaceData )
			break;

		// check if we reach penetration distance, no more penetrations after that
		// or if our modifier is super low, just stop the bullet
		if( ( data->m_flTraceLength > 3000.f && data->m_WeaponData->m_flPenetration > 0.f ) ||
			data->m_EnterSurfaceData->game.flPenetrationModifier < 0.1f ) {
			data->m_iPenetrationCount = 0;
			break;
		}

		bool bIsBulletStopped = HandleBulletPenetration( data );
		if( bIsBulletStopped )
			break;
	}

	data->m_flCurrentDamage = -1.f;
	return -1.f;
}

Autowall::C_FireBulletData *Autowall::CalculateDamageOnPoint( Vector start, Vector end, C_CSPlayer *from_entity, C_CSPlayer *to_entity, int specific_hitgroup ) {
	// default values for return info, in case we need to return abruptly
	C_FireBulletData return_info;
	return_info.m_flCurrentDamage = -1;
	return_info.m_iHitgroup = -1;
	return_info.m_TargetPlayer = nullptr;
	return_info.m_iPenetrationCount = 4;
	return_info.m_flTraceLength = 0.f;
	return_info.m_bPenetration = false;

	C_FireBulletData autowall_info;
	autowall_info.m_iPenetrationCount = 4;
	autowall_info.m_vecStart = start;
	autowall_info.m_vecPos = end;
	autowall_info.m_flTraceLength = 0.f;

	// direction 
	Math::AngleVectors( Math::CalcAngle( start, end ), autowall_info.m_vecDirection );

	// attacking entity
	if( !from_entity )
		from_entity = C_CSPlayer::GetLocalPlayer( );;

	if( !from_entity )
		return &return_info;

	auto filter_player = CTraceFilterOneEntity( );
	filter_player.pEntity = to_entity;

	auto filter_local = CTraceFilter( );
	filter_local.pSkip = from_entity;

	// setup filters
	if( to_entity )
		autowall_info.m_Filter = &filter_player;
	else
		autowall_info.m_Filter = &filter_player;

	// weapon
	auto weapon = reinterpret_cast< C_WeaponCSBaseGun * >( from_entity->m_hActiveWeapon( ).Get( ) );
	if( !weapon )
		return &return_info;

	// weapon data
	auto weapon_info = weapon->GetCSWeaponData( ).Xor( );
	if( !weapon_info )
		return &return_info;

	// client class
	auto weapon_client_class = weapon->GetClientClass( );
	if( !weapon_client_class )
		return &return_info;

	// weapon range
	float range = std::min( weapon_info->m_flWeaponRange, ( start - end ).Length( ) );
	end = start + ( autowall_info.m_vecDirection * range );
	autowall_info.m_flCurrentDamage = weapon_info->m_iWeaponDamage;

	while( autowall_info.m_flCurrentDamage > 0 && autowall_info.m_iPenetrationCount > 0 ) {
		return_info.m_iPenetrationCount = autowall_info.m_iPenetrationCount;

		TraceLine( autowall_info.m_vecStart, end, MASK_SHOT | CONTENTS_GRATE, &filter_local, &autowall_info.m_EnterTrace );
		ClipTraceToPlayers( autowall_info.m_vecStart, autowall_info.m_vecStart + autowall_info.m_vecDirection * 40.f, MASK_SHOT | CONTENTS_GRATE, autowall_info.m_Filter, &autowall_info.m_EnterTrace );

		const float distance_traced = ( autowall_info.m_EnterTrace.endpos - start ).Length( );
		autowall_info.m_flCurrentDamage *= pow( weapon_info->m_flRangeModifier, ( distance_traced / 500.f ) );

		/// if reached the end
		if( autowall_info.m_EnterTrace.fraction == 1.f ) {
			if( to_entity && specific_hitgroup != -1 ) {
				ScaleDamage( to_entity, autowall_info.m_flCurrentDamage, weapon_info->m_flArmorRatio, specific_hitgroup );

				return_info.m_flCurrentDamage = autowall_info.m_flCurrentDamage;
				return_info.m_iHitgroup = specific_hitgroup;
				return_info.m_vecPos = autowall_info.m_EnterTrace.endpos;
				return_info.m_TargetPlayer = to_entity;
			}
			else {
				return_info.m_flCurrentDamage = autowall_info.m_flCurrentDamage;
				return_info.m_iHitgroup = -1;
				return_info.m_vecPos = autowall_info.m_EnterTrace.endpos;
				return_info.m_TargetPlayer = nullptr;
			}

			break;
		}
		// if hit an entity
		if( autowall_info.m_EnterTrace.hitgroup > 0 && autowall_info.m_EnterTrace.hitgroup <= 7 && autowall_info.m_EnterTrace.hit_entity ) {
			// checkles gg
			if( ( to_entity && autowall_info.m_EnterTrace.hit_entity != to_entity ) ||
				( ( ( C_CSPlayer * )autowall_info.m_EnterTrace.hit_entity )->m_iTeamNum( ) == from_entity->m_iTeamNum( ) ) ) {
				return_info.m_flCurrentDamage = -1;
				return &return_info;
			}

			if( specific_hitgroup != -1 )
				ScaleDamage( ( C_CSPlayer * )autowall_info.m_EnterTrace.hit_entity, autowall_info.m_flCurrentDamage, weapon_info->m_flArmorRatio, specific_hitgroup );
			else
				ScaleDamage( ( C_CSPlayer * )autowall_info.m_EnterTrace.hit_entity, autowall_info.m_flCurrentDamage, weapon_info->m_flArmorRatio, autowall_info.m_EnterTrace.hitgroup );

			// fill the return info
			return_info.m_flCurrentDamage = autowall_info.m_flCurrentDamage;
			return_info.m_iHitgroup = autowall_info.m_EnterTrace.hitgroup;
			return_info.m_vecPos = autowall_info.m_EnterTrace.endpos;
			return_info.m_TargetPlayer = ( C_CSPlayer * )autowall_info.m_EnterTrace.hit_entity;

			break;
		}

		// noob
		autowall_info.m_WeaponData = weapon_info;
		autowall_info.m_EnterSurfaceData = g_pPhysSurface->GetSurfaceData( autowall_info.m_EnterTrace.surface.surfaceProps );

		// break out of the loop retard
		if( HandleBulletPenetration( &autowall_info ) )
			break;

		return_info.m_bPenetration = true;
	}

	return_info.m_iPenetrationCount = autowall_info.m_iPenetrationCount;

	return &return_info;
}