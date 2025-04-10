#include "ShotHandling.hpp"
#include "Resolver.hpp"
#include "../Visuals/EventLogger.hpp"
#include <iomanip>

ShotHandling g_ShotHandling;

void ShotHandling::ProcessShots( ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	const auto pWeapon = reinterpret_cast< C_WeaponCSBaseGun * >( pLocal->m_hActiveWeapon( ).Get( ) );
	if( !pWeapon )
		return;

	auto pWeaponData = pWeapon->GetCSWeaponData( );
	if( !pWeaponData.IsValid( ) )
		return;

	if( !m_bGotEvents ) {
		return;
	}

	m_bGotEvents = false;

	if( m_vecFilteredShots.empty( ) ) {
		return;
	}

	if( m_vecFilteredShots.back( ).m_vecImpacts.empty( ) && pLocal->IsDead( ) ) {
		//if( g_Vars.esp.event_resolver )
		//	g_EventLog.PushEvent( XorStr( "" ), XorStr( "missed due to death (latency)" ), Color( 255, 110, 110, 255 ), Color( 255, 110, 110, 255 ), 5.f, false );

		return;
	}

	auto it = m_vecFilteredShots.begin( );

	for( auto it = m_vecFilteredShots.begin( ); it != m_vecFilteredShots.end( ); ) {
		if( it->m_vecImpacts.empty( ) ) {
			it = m_vecFilteredShots.erase( it );
			continue;
		}

		const bool weHit = it->m_vecDamage.size( );

		// for some reason the m_Shotdata filtering is wrong but everything else seems to be fine
		// gotta look into it lmao
		if( weHit ) {

		}
		// note - michal;
		// we'll have to handle other misses than spread, occlusion and resolver too for 
		// max accuracy log wise (so user knows why he 'missed'), those include misses due to
		// the enemy dying (for example you fired two DT shots, both aimed at chest but due to 
		// spread you hit his head on the first shot, killed him, and second DT shot already
		// fired before we got the info about the player dying, causing in a "miss"), or misses
		// due to death (local, aka clientsides), other misses due to death of enemy (teammate
		// killed him like a tick before), etc.. you get the idea
		else {
			// intersection with resolved matrix
			// when true we missed due to resolver
			const bool bHit = TraceShot( &*it );

			std::stringstream msg{ };

			// handle miss logs here
			const float flPointDistance = it->m_ShotData.m_vecStart.Distance( it->m_ShotData.m_vecEnd );
			const float flImpactDistance = it->m_ShotData.m_vecStart.Distance( it->m_vecImpacts.back( ) );

			const Vector vecAimPoint = it->m_ShotData.m_vecEnd;
			const QAngle angAimAngle = Math::CalcAngle( it->m_ShotData.m_vecStart, vecAimPoint );
			const Vector vecImpactPoint = it->m_vecImpacts.back( );
			const QAngle angImpactAngle = Math::CalcAngle( it->m_ShotData.m_vecStart, vecImpactPoint );
			const QAngle angAngleDelta = angAimAngle - angImpactAngle;

			const float flDeltaAmount = fabsf( angAngleDelta.x + angAngleDelta.y + angAngleDelta.z );

			static ConVar *weapon_accuracy_nospread = g_pCVar->FindVar( XorStr( "weapon_accuracy_nospread" ) );
			static ConVar *weapon_recoil_scale = g_pCVar->FindVar( XorStr( "weapon_recoil_scale" ) );

			// TODO: Improve this/make it more consistent!!!!!!
			if( bHit || weapon_accuracy_nospread->GetInt( ) >= 1 ) {
				if( g_Vars.esp.event_resolver )
					g_EventLog.PushEvent( XorStr( "" ), g_Vars.menu.polish_mode ? XorStr( "Nie trafiono strzalu z powodu zgadywacza" ) : XorStr( "Missed shot due to resolver" ), Color( 220, 220, 220, 255 ), Color( 220, 220, 220, 255 ), 5.f, false );

				if( it->m_ShotData.m_bWasLBYFlick ) {
					g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iMissedShotsLBY++;

					int previous = g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iMissedShotsLBY;
					g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iMissedShotsLBY++;

					/*g_pCVar->ConsoleColorPrintf( Color( 220, 220, 220, 255 ), XorStr( "incrementing missed lby shots: idx: %i - pre %i - post %i\n" ),
						it->m_ShotData.m_iTargetIndex,
						previous,
						g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iMissedShotsLBY );*/
				}
				else {
					bool bOverriding = g_Vars.rage.antiaim_resolver_override;
					if( !g_Vars.rage.antiaim_correction_override.enabled )
						bOverriding = false;

					// don't count misses when overriding
					if( !bOverriding ) {
						const bool bIsLogicStage =
							g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iCurrentResolverType == 7 ||
							g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iCurrentResolverType == 8 ||
							g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iCurrentResolverType == 91 ||
							g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iCurrentResolverType == 92;

						int nPreviousRegular = g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iMissedShots;
						int nPreviousHeight = g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iMissedShotsHeight;

						// missed when firing at their last move lby, take note of this for the future
						//if( g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_bInMoveBodyStage ) {
						//	g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_nPrevMissedMoveBodyShots++;
						//
						//	g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_bInMoveBodyStage = false;
						//}

						// if we've already reset our logic stuff then don't do this already,
						// clearly we failed once before and failed once again by resetting it.
						// don't let it happen again.
						/*if( g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_bResetBodyLogic ) {
							// we missed a shot on one of these 'logic' stages, but have hit the last move lby
							// previously twice (or more), safe to assume that the initial miss was
							// caused by smth else (maybe prediction, who knows), but either way let's
							// let the cheat fire at last move lby once more.
							if( g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iCurrentResolverType == 104 ||
								g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iCurrentResolverType == 105 )
							{
								if( g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_nPrevHitMoveBodyShots >= 2 ) {
									g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_nPrevMissedMoveBodyShots = 0;

									// take note that we allowed the cheat to
									// shoot at last move lby once more
									g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_bResetBodyLogic = true;
								}
							}
						}*/

						// fuck this shit bruh, none of the height
						// resolver angles hit, don't even bother again..
						if( g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iMissedShotsHeight > 4 ) {
							//g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iMissedShotsHeight++;

							g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_bSuppressHeightResolver = true;
						}

						// we missed on logic stage, don't count missed shot so we can properly bruteforce next shots (last move lby, etc)
						if( bIsLogicStage && !g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_bSuppressDetectionResolvers ) {
							g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_bSuppressDetectionResolvers = true;

							//g_pCVar->ConsoleColorPrintf( Color( 220, 220, 220, 255 ), XorStr( "ignored missed shot increment on logic stage, next shot will start count\n" ) );
						}
						else {
							if( g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_bHasHeightAdvantage ) {
								g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iMissedShotsHeight++;

								/*g_pCVar->ConsoleColorPrintf( Color( 220, 220, 220, 255 ), XorStr( "incrementing height missed shots: idx: %i - pre %i - post %i\n" ),
									it->m_ShotData.m_iTargetIndex,
									nPreviousHeight,
									g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iMissedShots );*/
							}
							else {
								g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iMissedShots++;

								/*g_pCVar->ConsoleColorPrintf( Color( 220, 220, 220, 255 ), XorStr( "incrementing regular missed shots: idx: %i - pre %i - post %i\n" ),
									it->m_ShotData.m_iTargetIndex,
									nPreviousRegular,
									g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iMissedShots );*/
							}
						}

						// desync logic resolver
						if( g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iCurrentResolverType == 420 )
							g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iMissedShotsDesync++;
					}

					// if( g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_eCurrentStage == EResolverStages::RESOLVE_MODE_SERVER ) {
					// 	g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_bMissedNetworkedAngle = true;
					// }
				}
			}
			// spread/occlusion miss
			else {
				if( g_Vars.esp.event_resolver ) {
					char buf[ 128 ] = { };

					// [0] spread (occlusion)
					/*if( flPointDistance > flImpactDistance ) {
						sprintf( buf, XorStr( "missed due to %s [ %.2f deg ]" ), XorStr( "occlusion" ), flDeltaAmount );
					}
					// [1] spread (inaccuracy)
					else {
						sprintf( buf, XorStr( "missed due to %s [ %.2f deg ]" ), XorStr( "spread" ), flDeltaAmount );
					}*/

					g_EventLog.PushEvent( XorStr( "" ), g_Vars.menu.polish_mode ? XorStr( "Nie trafiono strzalu z powodu rozszerzenia strzalu" ) : XorStr( "Missed shot due to spread" ), Color( 220, 220, 220, 255 ), Color( 220, 220, 220, 255 ) );

				}

				g_Resolver.m_arrResolverData.at( it->m_ShotData.m_iTargetIndex ).m_iMissedShotsSpread++;
			}
		}

		it = m_vecFilteredShots.erase( it );
	}
}

void ShotHandling::RegisterShot( ShotInformation_t &shot ) {
	auto &newShot = m_vecShots.emplace_back( );

	newShot.m_ShotData = shot;
}

void ShotHandling::GameEvent( IGameEvent *pEvent ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	const auto pWeapon = reinterpret_cast< C_WeaponCSBaseGun * >( pLocal->m_hActiveWeapon( ).Get( ) );
	if( !pWeapon )
		return;

	auto pWeaponData = pWeapon->GetCSWeaponData( );
	if( !pWeaponData.IsValid( ) )
		return;

	if( m_vecShots.empty( ) && m_vecFilteredShots.empty( ) )
		return;

	if( !m_vecShots.empty( ) ) {
		for( auto it = m_vecShots.begin( ); it != m_vecShots.end( ); ) {
			if( std::fabsf( it->m_ShotData.m_flTime - g_pGlobalVars->realtime ) >= 2.5f || it->m_ShotData.m_bMatched ) {
				it = m_vecShots.erase( it );
			}
			else {
				it = next( it );
			}
		}
	}

	switch( hash_32_fnv1a( pEvent->GetName( ) ) ) {
		case hash_32_fnv1a_const( "weapon_fire" ):
		{
			if( g_pEngine->GetPlayerForUserID( pEvent->GetInt( XorStr( "userid" ) ) ) != pLocal->EntIndex( ) || m_vecShots.empty( ) )
				return;

			const auto it = std::find_if( m_vecShots.rbegin( ), m_vecShots.rend( ), [ & ] ( ShotEvents_t it ) -> bool {
				return !it.m_ShotData.m_bMatched;
			} );

			if( !&it || it->m_ShotData.m_bMatched )
				return;

			it->m_ShotData.m_bMatched = true;

			auto &filteredShot = m_vecFilteredShots.emplace_back( *it );

			break;
		}
		case hash_32_fnv1a_const( "player_hurt" ):
		{
			if( m_vecFilteredShots.empty( ) )
				return;

			auto target = C_CSPlayer::GetPlayerByIndex( g_pEngine->GetPlayerForUserID( pEvent->GetInt( XorStr( "userid" ) ) ) );
			if( !target || target == pLocal || pLocal->IsTeammate( target ) || target->IsDormant( ) )
				return;

			ShotEvents_t::player_hurt_t hurt{ };
			hurt.m_iDamage = pEvent->GetInt( XorStr( "dmg_health" ) );
			hurt.m_iTargetIndex = target->EntIndex( );
			hurt.m_iHitgroup = pEvent->GetInt( XorStr( "hitgroup" ) );

			m_vecFilteredShots.back( ).m_vecDamage.push_back( hurt );

			break;
		}
		case hash_32_fnv1a_const( "bullet_impact" ):
		{
			if( m_vecFilteredShots.empty( ) || g_pEngine->GetPlayerForUserID( pEvent->GetInt( XorStr( "userid" ) ) ) != pLocal->EntIndex( ) )
				return;

			m_vecFilteredShots.back( ).m_vecImpacts.emplace_back( pEvent->GetFloat( XorStr( "x" ) ), pEvent->GetFloat( XorStr( "y" ) ), pEvent->GetFloat( XorStr( "z" ) ) );

			break;
		}
	}

	m_bGotEvents = true;
}

bool ShotHandling::TraceShot( ShotEvents_t *shot ) {
	if( !shot )
		return false;

	if( !shot->m_ShotData.m_pHitbox || shot->m_ShotData.m_pHitbox == nullptr )
		return false;

	if( !shot->m_ShotData.m_pRecord.player || shot->m_ShotData.m_pRecord.player == nullptr )
		return false;

	const auto pMatrix = shot->m_ShotData.m_pRecord.m_pMatrix;
	if( !pMatrix )
		return false;

	const Vector vecServerImpact = shot->m_vecImpacts.back( );

	const float flRadius = shot->m_ShotData.m_pHitbox->m_flRadius;
	const bool bCapsule = flRadius != -1.f;

	const Vector vecMin = shot->m_ShotData.m_pHitbox->bbmin.Transform( pMatrix[ shot->m_ShotData.m_pHitbox->bone ] );
	const Vector vecMax = shot->m_ShotData.m_pHitbox->bbmax.Transform( pMatrix[ shot->m_ShotData.m_pHitbox->bone ] );

	auto dir = vecServerImpact - shot->m_ShotData.m_vecStart;
	dir.Normalize( );

	if( bCapsule )
		return Math::IntersectSegmentToSegment( shot->m_ShotData.m_vecStart, vecServerImpact, vecMin, vecMax, flRadius );

	CGameTrace tr;
	g_pEngineTrace->ClipRayToEntity( Ray_t( shot->m_ShotData.m_vecStart, vecServerImpact ), MASK_SHOT, shot->m_ShotData.m_pRecord.player, &tr );
	if( !tr.hit_entity )
		return false;

	return tr.hit_entity == shot->m_ShotData.m_pRecord.player;
}