#include "Resolver.hpp"
#include "../Visuals/EventLogger.hpp"
#include "../Scripting/Scripting.hpp"
#include <sstream>
#include "Autowall.hpp"
#include "AntiAim.hpp"
#include <iomanip>
#include "../Miscellaneous/PlayerList.hpp"

Resolver g_Resolver;

void Resolver::UpdateDesyncDetection( AnimationRecord *pRecord, AnimationRecord *pPreviousRecord, AnimationRecord *pPenultimateRecord ) {
	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );
	if( !pRecord )
		return resolverData.ResetDesyncData( );

	if( !pPreviousRecord )
		return resolverData.ResetDesyncData( );

	if( !pPenultimateRecord )
		return resolverData.ResetDesyncData( );

	// player can only be on ground when doing this
	if( !( pRecord->m_fFlags bitand FL_ONGROUND ) )
		return resolverData.ResetDesyncData( );

	// log the playback rates
	resolverData.m_tLoggedPlaybackRates = std::make_tuple(
		pRecord->m_pServerAnimOverlays[ 6 ].m_flPlaybackRate * 1000.f,
		pPreviousRecord->m_pServerAnimOverlays[ 6 ].m_flPlaybackRate * 1000.f,
		pPenultimateRecord->m_pServerAnimOverlays[ 6 ].m_flPlaybackRate * 1000.f );

	const float flLatestMovePlayback = std::get<0>( resolverData.m_tLoggedPlaybackRates );
	const float flPreviousMovePlayback = std::get<1>( resolverData.m_tLoggedPlaybackRates );
	const float flPenultimateMovePlayback = std::get<2>( resolverData.m_tLoggedPlaybackRates );

#pragma region SPECIFIC_DETECT
	const bool bDetectedFlickOne = flLatestMovePlayback >= 1.f && flPreviousMovePlayback < 1.f && flPenultimateMovePlayback >= 1.f;
	const bool bDetectedFlickTwo = flLatestMovePlayback >= 1.f && flPreviousMovePlayback < 1.f && flPenultimateMovePlayback < 1.f;
	if( bDetectedFlickOne or bDetectedFlickTwo ) {
		// take note that we detected this
		++resolverData.m_nFlickDetectionCount;

		// take note of the last detected passed ticks we had
		resolverData.m_nLastFlickDetectionFrequency = resolverData.m_nFlickDetectionFrequency;

		// take note how many ticks have passed since the last 'detection'
		resolverData.m_nFlickDetectionFrequency = abs( g_pGlobalVars->tickcount - resolverData.m_nFlickDetectionFrequency );

		// take note of the tick we detected this thing in
		resolverData.m_nLastFlickTick = g_pGlobalVars->tickcount;
	}

	// caught 2 or more flicks
	if( resolverData.m_nFlickDetectionCount > 1 ) {
		// we can't be sure if they are desyncing here for sure
		// so let's check how many ticks pass between each
		// flick, if it's a constant number then there is something
		// fishy going on and we can mark them as desyncing.
		if( resolverData.m_nFlickDetectionFrequency == resolverData.m_nLastFlickDetectionFrequency ) {
			resolverData.m_bDesyncFlicking = true;

			// yep, its constant!
			resolverData.m_bConstantFrequency = true;
		}

		// ok we caught yet another flick
		if( resolverData.m_nFlickDetectionCount >= 3 ) {
			// Future - Mask Off (2:20)
			if( resolverData.m_nFlickDetectionCount >= 4 ) {
				resolverData.m_bDesyncFlicking = true;
			}

			// was the frequency between the ticks constant?
			// if so, we can be sure they are desync flicking
			if( resolverData.m_bConstantFrequency ) {
				resolverData.m_bDesyncFlicking = true;
			}
		}
	}

	// 14 ticks have passed since the last detection, they prob arent desyncing anymore
	if( resolverData.m_nFlickDetectionCount > 0 and resolverData.m_nLastFlickTick > 0 ) {
		if( abs( resolverData.m_nLastFlickTick - g_pGlobalVars->tickcount ) > 14 ) {
			resolverData.ResetDesyncData( );
		}
	}

#pragma endregion

}

bool Resolver::IsAnglePlausible( AnimationRecord *pRecord, float flAngle ) {
	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );
	float flAngleDelta = abs( flAngle - pRecord->m_flLowerBodyYawTarget );

	if( ( resolverData.m_eBodyBreakType == EBodyBreakTypes::BREAK_LOW and ( flAngleDelta >= XorFlt( 100.f ) or flAngleDelta < XorFlt( 120.f ) ) ) ||
		( resolverData.m_eBodyBreakType == EBodyBreakTypes::BREAK_HIGH and flAngleDelta >= XorFlt( 120.f ) ) ) {
		return true;
	}

	return false;
}

void Resolver::UpdateLBYPrediction( AnimationRecord *pRecord ) {
	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );
	if( !pRecord )
		return;

	// don't do
	if( resolverData.m_iMissedShotsLBY >= 2 ) {
		pRecord->m_bLBYFlicked = false;
		resolverData.m_bInPredictionStage = false;
		return;
	}

	if( g_PlayerList.GetSettings( pRecord->m_pEntity->GetSteamID( ) ).m_bDisableBodyPred ) {
		resolverData.ResetPredData( );
		return;
	}

	if( pRecord->m_vecVelocity.Length( ) > XorFlt( 0.1f ) || ( pRecord->m_fFlags & FL_ONGROUND ) == 0 ) {
		resolverData.m_flLowerBodyRealignTimer = FLT_MAX;
		pRecord->m_bLBYFlicked = false;
		resolverData.m_bInPredictionStage = false;
		return;
	}

	const float flXored1_1 = XorFlt( 1.1f );
	if( resolverData.m_flPreviousLowerBodyYaw != FLT_MAX && fabs( Math::AngleDiff( pRecord->m_flLowerBodyYawTarget, resolverData.m_flPreviousLowerBodyYaw ) ) > XorFlt( 1.f ) ) {
		resolverData.m_flLowerBodyRealignTimer = pRecord->m_flAnimationTime + flXored1_1;

		pRecord->m_bLBYFlicked = true;
	}

	resolverData.m_flPreviousLowerBodyYaw = pRecord->m_flLowerBodyYawTarget;

	if( pRecord->m_flAnimationTime >= resolverData.m_flLowerBodyRealignTimer && resolverData.m_flLowerBodyRealignTimer < FLT_MAX ) {
		resolverData.m_bInPredictionStage = true;
		resolverData.m_flLowerBodyRealignTimer = pRecord->m_flAnimationTime + flXored1_1;
		pRecord->m_bLBYFlicked = true;
	}
}

void Resolver::UpdateResolverStage( AnimationRecord *pRecord ) {
	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );

	const float flXored01 = XorFlt( 0.1f );
	bool bInPredictionStage = pRecord->m_bLBYFlicked;

	// if desync flick is detected,
	// do NOT FORCE LBY!!! USE LOGIC INSTEAD!
	if( resolverData.m_bDesyncFlicking ) {
		resolverData.m_eCurrentStage = RESOLVE_MODE_LOGIC;
	}
	// WE GOOD
	else {
		if( ( pRecord->m_fFlags & FL_ONGROUND ) && ( pRecord->m_vecVelocity.Length( ) <= flXored01 || ( pRecord->m_bIsFakewalking ) ) )
			resolverData.m_eCurrentStage = EResolverStages::RESOLVE_MODE_STAND;

		if( ( pRecord->m_fFlags & FL_ONGROUND ) && ( pRecord->m_vecVelocity.Length( ) > flXored01 && !( pRecord->m_bIsFakewalking ) ) ) {
			resolverData.m_eCurrentStage = EResolverStages::RESOLVE_MODE_MOVE;
		}
		else if( !( pRecord->m_fFlags & FL_ONGROUND ) ) {
			resolverData.m_eCurrentStage = EResolverStages::RESOLVE_MODE_AIR;
		}
		else if( bInPredictionStage ) {
			resolverData.m_eCurrentStage = EResolverStages::RESOLVE_MODE_PRED;
		}
	}

	// allow lby flicks on override
	// also important allow override to override micro moving players because of the
	// falco kid who uses his shitty exploit =D
	if( g_Vars.rage.antiaim_resolver_override && g_Vars.rage.antiaim_correction_override.enabled && ( !bInPredictionStage || resolverData.m_bDesyncFlicking ) && pRecord->m_vecVelocity.Length( ) < XorFlt( 20.f ) ) {
		resolverData.m_eCurrentStage = EResolverStages::RESOLVE_MODE_OVERRIDE;
	}
}

void Resolver::ResolveBrute( AnimationRecord *pRecord ) {
	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	// reset move data here, we won't be working with it anyway
	resolverData.ResetMoveData( );

	QAngle angAway;
	pLocal->IsDead( ) ? Vector( 0, 180, 0 ) : Math::VectorAngles( pLocal->m_vecOrigin( ) - pRecord->m_vecOrigin, angAway );

	const float flXored180 = XorFlt( 180.f );
	const float flXored110 = XorFlt( 110.f );

	// we have height advantage over this player
	// and we are basically above him, handle him separately
	/*if( resolverData.m_bHasHeightAdvantage && !resolverData.m_bSuppressHeightResolver ) {
		return ResolveHeight( pRecord );
	}*/

	switch( resolverData.m_iMissedShots % 7 ) {
		case 0:
			resolverData.m_iCurrentResolverType = 0;
			pRecord->m_angEyeAngles.y = resolverData.m_flFreestandYaw != FLT_MAX ? resolverData.m_flFreestandYaw : angAway.y + flXored180;
			break;
		case 1:
			resolverData.m_iCurrentResolverType = 1;
			pRecord->m_angEyeAngles.y = SnapToClosestYaw( resolverData.m_flFreestandYaw == FLT_MAX ? angAway.y : angAway.y + flXored180 );
			break;
		case 2:
			resolverData.m_iCurrentResolverType = 2;
			pRecord->m_angEyeAngles.y = SnapToClosestYaw( pRecord->m_flLowerBodyYawTarget );
			break;
		case 3:
			resolverData.m_iCurrentResolverType = 3;
			pRecord->m_angEyeAngles.y = SnapToClosestYaw( pRecord->m_flLowerBodyYawTarget + flXored180 );
			break;
		case 4:
			resolverData.m_iCurrentResolverType = 4;
			pRecord->m_angEyeAngles.y = SnapToClosestYaw( pRecord->m_flLowerBodyYawTarget + flXored110 );
			break;
		case 5:
			resolverData.m_iCurrentResolverType = 5;
			pRecord->m_angEyeAngles.y = SnapToClosestYaw( pRecord->m_flLowerBodyYawTarget - flXored110 );
			break;
		case 6:
			resolverData.m_iCurrentResolverType = 6;
			pRecord->m_angEyeAngles.y = SnapToClosestYaw( angAway.y );
			break;
	}
}

void Resolver::UpdateBodyDetection( AnimationRecord *pRecord, AnimationRecord *pPreviousRecord ) {
	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );

	if( !pRecord || !pPreviousRecord )
		return;

	C_AnimationLayer *pCurrentAdjustLayer = &pRecord->m_pServerAnimOverlays[ ANIMATION_LAYER_ADJUST ];
	C_AnimationLayer *pPreviousAdjustLayer = &pPreviousRecord->m_pServerAnimOverlays[ ANIMATION_LAYER_ADJUST ];

	if( !pCurrentAdjustLayer || !pPreviousAdjustLayer )
		return;

	// assume they're always breaking lby, incase this check fails
	if( !( resolverData.m_bBreakingLowerBody ) ) {
		resolverData.m_bBreakingLowerBody = true;

		// assume low breaker for now, it's the most common.
		resolverData.m_eBodyBreakType = EBodyBreakTypes::BREAK_LOW;
	}

	// reset this, it will get overwritten under the right
	// conditions anyway.
	resolverData.m_bHasntUpdated = false;

	if( resolverData.m_iMissedShotsLBY >= 2 || resolverData.m_iMissedShots >= 2 ) {
		resolverData.m_bBreakingLowerBody = true;
		return;
	}

	// see if the 979 animation was triggered
	if( pRecord->m_pEntity->GetSequenceActivity( pCurrentAdjustLayer->m_nSequence ) == ACT_CSGO_IDLE_TURN_BALANCEADJUST/* &&
		pRecord->m_pEntity->GetSequenceActivity( pPreviousAdjustLayer->m_nSequence ) == ACT_CSGO_IDLE_TURN_BALANCEADJUST*/ ) {
		// -- detect lby breakers above 120 delta
		if( !( pCurrentAdjustLayer->m_flWeight == 0.f && pCurrentAdjustLayer->m_flWeight == pPreviousAdjustLayer->m_flWeight ) ) {
			// balance adjust animation has started playing or is currently about to play again
			if( pCurrentAdjustLayer->m_flWeight == 1.f || pCurrentAdjustLayer->m_flWeight != pPreviousAdjustLayer->m_flWeight ) {
				resolverData.m_flLastWeightTriggerTime = pRecord->m_flAnimationTime;
			}

			// animation keeps playing or restarting, means that they are 
			// either moving their yaw left very fast or are breaking lby above 120 delta

			// since animation layers update every time the enemies choke cycle restarts, see if 
			// the last weight update time exceeds their choke time plus some tolerance (2, random number)
			if( TIME_TO_TICKS( fabs( resolverData.m_flLastWeightTriggerTime - pRecord->m_flAnimationTime ) ) < pRecord->m_iChokeTicks + 2 ) {
				// while weight is 1.f or incrementing from 0, the cycle increments until some number (~0.6)
				// once cycle reaches that number, both weight and cycle restart and go to zero once again.
				if( pCurrentAdjustLayer->m_flWeight == 1.f || pCurrentAdjustLayer->m_flWeight != pPreviousAdjustLayer->m_flWeight ) {
					if( pCurrentAdjustLayer->m_flCycle != pPreviousAdjustLayer->m_flCycle ) {
						resolverData.m_flLastCycleTriggerTime = pRecord->m_flAnimationTime;
					}

					// since animation layers update every time the enemies choke cycle restarts, see if 
					// the last cycle update time exceeds their choke time plus some tolerance
					if( TIME_TO_TICKS( fabs( resolverData.m_flLastCycleTriggerTime - pRecord->m_flAnimationTime ) )
						< pRecord->m_iChokeTicks + 2 ) {
						// cycle keeps changing, we can safely assume that this player is either
						// breaking lby over 120 delta or failing to break (to the left)
						resolverData.m_bBreakingLowerBody = true;
						resolverData.m_flLastUpdateTime = pRecord->m_flAnimationTime;

						// mark current breaker type as high delta.
						resolverData.m_eBodyBreakType = EBodyBreakTypes::BREAK_HIGH;
					}
				}
			}
			else {
				// not sure if this could even happen,
				// but i was changing some logic around and was weary
				// of this, so better be safe than sorry
				goto BREAKING_LOW_OR_NONE;
			}
		}
		// -- detect lby breakers under 120 delta
		else {
		BREAKING_LOW_OR_NONE:
			// the 979 animation hasn't played in a while. this can mean two things:
			// the enemy is no longer breaking lby, or the enemy is breaking lby but
			// supressing the 979 animation from restarting, meaning they're breaking under 120 delta.
			if( pCurrentAdjustLayer->m_flWeight == 0.f && pCurrentAdjustLayer->m_flCycle > 0.9f /*more like 0.96 but better be safe than sorry*/ ) {
				// so here's the problem. due to the nature of how lby breakers work, we can't really
				// detect an "lby update" (coz the whole point of LBY breakers is to keep your lby 
				// at one angle, and real at a different angle, as long as it's not in the same place).
				// so lby will always be the same. the check below will only be triggered when the player
				// fails to break lby, and when that happens only 2 things can happen, first one being
				// they trigger 979 and the first check owns them, second being they don't trigger 979
				// and this check owns them. either way, proper lby breakers will bypass this, so 
				// we have to improvise; (L236)

				// we detected a lowerbody update
				if( pRecord->m_flLowerBodyYawTarget != pPreviousRecord->m_flLowerBodyYawTarget ) {
					// check if the update we detected exceeds the "tolerance" body update delta 
					// that could mark this update as a lowerbody flick.
					if( fabs( Math::AngleDiff( pRecord->m_flLowerBodyYawTarget, pPreviousRecord->m_flLowerBodyYawTarget ) ) > XorFlt( 30.f ) ) {
						// since the lby angle change exceeded 35 degrees delta, it most likely means
						// that the enemy attempted to break lby. at this point we can assume that 
						// they're breaking lby.

						resolverData.m_bBreakingLowerBody = true;
						resolverData.m_flLastUpdateTime = pRecord->m_flAnimationTime;

						// mark current breaker type as low.
						resolverData.m_eBodyBreakType = EBodyBreakTypes::BREAK_LOW;
					}
				}
				// no body update was detected in a while..
				// they are either not breaking lby, or successfully breaking lby now (after failing before).
				else if( fabs( resolverData.m_flLastUpdateTime - pRecord->m_flAnimationTime ) > 1.125f ) {
					// notify the cheat that we're going to be using
					// logic in order to find out wtf is goin on
					resolverData.m_bHasntUpdated = true;

					const float flDeltaTolerance = 35.f;
					const bool bNearFreestandAngle = fabs( pRecord->m_flLowerBodyYawTarget - resolverData.m_flFreestandYaw ) <= flDeltaTolerance;
					const bool bNearLastMoveAngle = resolverData.m_vecLastMoveOrigin.IsZero( ) ? false :
						( fabs( pRecord->m_flLowerBodyYawTarget - resolverData.m_flLastMoveBody ) <= flDeltaTolerance &&
						  ( resolverData.m_vecLastMoveOrigin - pRecord->m_vecOrigin ).Length( ) <= XorFlt( 128.f ) );

					// their lby and logic angles are awfully close!! this 
					// can only mean one thing, they are either not breaking lby
					// or failing to break lby !!!

					// i decided not to use freestand angles here due to them
					// being based on the location relative to our local player,
					// and since we move around, they might change, and therefore
					// fail/give bad results, so just to be sure we should only
					// really use the last move angle here
					if( /*bNearFreestandAngle ||*/ bNearLastMoveAngle ) {
						if( /*bNearFreestandAngle &&*/ bNearLastMoveAngle ) {
							// there is absolutely no way that this guy is breaking lby,
							// both logical angles are closeby to each other and close
							// to the lby, he has to be here..
							const float flLogicAnglesDelta = fabs( resolverData.m_flFreestandYaw - resolverData.m_flLastMoveBody );
							if( /*flLogicAnglesDelta <= flDeltaTolerance*/true ) {
								// ( ... )
								resolverData.m_bBreakingLowerBody = false;

								// mark current breaker type as low.
								resolverData.m_eBodyBreakType = EBodyBreakTypes::BREAK_NONE;
							}
						}

						// this CAN FAIL SOMETIMES, look into improving this.

						// resolverData.m_bBreakingLowerBody = false;

						// mark current breaker type as low.
						// resolverData.m_eBodyBreakType = EBodyBreakTypes::BREAK_NONE;
					}
					// their lby is 35 degrees away than the logical angle
					else {
						// we're not using logic here, but the lowerbody angle
						// seems to be far further away from the logical angle
						// than needed. best to assume that he's breaking lby.

						resolverData.m_bBreakingLowerBody = true;

						// mark current breaker type as low.
						resolverData.m_eBodyBreakType = EBodyBreakTypes::BREAK_LOW;
					}
				}
			}
		}
	}

	// to make sure that everything went right incase something
	// before went wrong, we should see ifthe enemies lby hasn't
	// updated in a while, and if that is the case we can assume
	// that they are no longer breaking lby.

	// we don't want to run this when we're comparing the lby against
	// the logical freestand angle, since it will overwrite our results.
	if( !resolverData.m_bHasntUpdated ) {
		if( ( pRecord->m_fFlags & FL_ONGROUND ) && ( pRecord->m_vecVelocity.Length( ) <= 0.1f || ( pRecord->m_bIsFakewalking ) ) ) {
			if( fabs( resolverData.m_flLastUpdateTime - pRecord->m_flAnimationTime ) > 1.125f ) {
				resolverData.m_bBreakingLowerBody = false;
				resolverData.m_eBodyBreakType = EBodyBreakTypes::BREAK_NONE;
			}
		}
	}
}

void Resolver::ResolveHeight( AnimationRecord *pRecord ) {
	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	QAngle angAway;
	pLocal->IsDead( ) ? Vector( 0, 180, 0 ) : Math::VectorAngles( pLocal->m_vecOrigin( ) - pRecord->m_vecOrigin, angAway );

	switch( resolverData.m_iMissedShots % 4 ) {
		case 0:
			// if there is a valid edge angle, apply it to the player
			// shoutout to all supremacy edge users trololololol
			if( g_AntiAim.DoEdgeAntiAim( pRecord->m_pEntity, resolverData.m_angEdgeAngle ) ) {
				pRecord->m_angEyeAngles.y = resolverData.m_angEdgeAngle.y;
			}
			// if not, just do away + 180 (so head will face towards us)
			else {
				pRecord->m_angEyeAngles.y = angAway.y + 180.f;
			}
			break;
		case 1:
			// if there is a valid freestand angle, use it
			if( resolverData.m_flFreestandYaw != FLT_MAX ) {
				pRecord->m_angEyeAngles.y = resolverData.m_flFreestandYaw;
			}
			// if not, use away angle (head facing away from us)
			else {
				pRecord->m_angEyeAngles.y = angAway.y;
			}
			break;
			// from here on now it's just -+90.f to away angle (so sideways xd)
		case 2:
			pRecord->m_angEyeAngles.y = angAway.y - 90.f;
			break;
		case 3:
			pRecord->m_angEyeAngles.y = angAway.y + 90.f;
			break;
	}
}

void Resolver::ResolveStand( AnimationRecord *pRecord, AnimationRecord *pPreviousRecord ) {
	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	// we don't have any last move data, perform basic bruteforce
	if( resolverData.m_vecLastMoveOrigin.IsZero( ) ) {
		return ResolveBrute( pRecord );
	}

	// last move origin is too far away, the enemy has moved when we haven't seen them(?)
	if( ( resolverData.m_vecLastMoveOrigin - pRecord->m_vecOrigin ).Length( ) > XorFlt( 128.f ) ) {
		return ResolveBrute( pRecord );
	}

	// they're most likely not breaking, just force lby. XD
	if( resolverData.m_eBodyBreakType == EBodyBreakTypes::BREAK_NONE ) {
		pRecord->m_angEyeAngles.y = pRecord->m_flLowerBodyYawTarget;
		resolverData.m_iCurrentResolverType = 27;

		return;
	}

	float flAnimTimeDelta = abs( pRecord->m_flAnimationTime - resolverData.m_flLastMoveAnimTime );
	if( !resolverData.m_bSuppressDetectionResolvers ) {
		// their lby hasn't updated since they've stopped moving, they're probably still
		// here or they're being smart and breaking last moving lby
		if( resolverData.m_flLastMoveBody == pRecord->m_flLowerBodyYawTarget ) {
			pRecord->m_angEyeAngles.y = pRecord->m_flLowerBodyYawTarget;
			resolverData.m_iCurrentResolverType = 7;

			// i hope this is fine
			// pRecord->m_AnimationFlags |= ELagRecordFlags::RF_IsResolved;
			return;
		}

		// note - michal;
		// this can easily be flawed, let me explain why
		// say your head is at angle 0, and you're flicking to angle+120, so your lby
		// will be at 120. all good, but then you move your mouse so that your angle 
		// changes to 1, so now you're flicking to angle+120 (121), lby has updated,
		// your real has only moved one degree, and your lby has also only changed one 
		// degree. we then would normally miss cos obviously their head won't be there
	#if 1
			// their lby updated while they're standing, they've exposed their secrets!
			// note - maxwell; you need to make sure pPreviousRecord isn't null, it can be null sometimes because of
			// AnimationSystem.cpp line 574. maybe just not resolving if pPreviousRecord is null is a better idea?...
		if( pPreviousRecord && pRecord->m_flLowerBodyYawTarget != pPreviousRecord->m_flLowerBodyYawTarget ) {
			pRecord->m_angEyeAngles.y = pRecord->m_flLowerBodyYawTarget;
			resolverData.m_iCurrentResolverType = 8;

			// i hope this is fine
			// pRecord->m_AnimationFlags |= ELagRecordFlags::RF_IsResolved;
			return;
		}
	#endif

		// idea - maxwell; what if we 'verified' if we have an accurate reverse freestanding value by using their lby?
		// if they're breaking, we can check the delta between their reverse freestand yaw, and their lby, and compare it
		// with their current animations. if the delta is > 120 and they're also triggering balance adjust, the reverse
		// freestand is probably a decently accurate resolve. i hope this make sense. i wrote this while wasted.
		// this is unfinished, m_bIsBreakingLow and m_bIsBreakingHigh doesn't exist, but it's easy to add, i'll add it
		// tomorrow.
		if( resolverData.m_bBreakingLowerBody ) {
			// we're going to prioritize last moving lby since if it's a good resolve, it will be more accurate then
			// at targets reverse freestanding.
			if( g_Vars.rage.antiaim_resolver_plausible_last_moving && resolverData.m_flLastMoveBody != FLT_MAX && IsAnglePlausible( pRecord, resolverData.m_flLastMoveBody ) ) {
				pRecord->m_angEyeAngles.y = resolverData.m_flLastMoveBody;
				resolverData.m_iCurrentResolverType = 92;
				return;
			}

			if( resolverData.m_flFreestandYaw != FLT_MAX && IsAnglePlausible( pRecord, resolverData.m_flFreestandYaw ) ) {
				pRecord->m_angEyeAngles.y = resolverData.m_flFreestandYaw;
				resolverData.m_iCurrentResolverType = 91;
				return;
			}

			// last resort, this seems retarded but a lot of kids use edge when hugging walls when you have height on them.
			// if they're breaking, this is basically a perfect resolve. :)
			if( g_Vars.rage.antiaim_resolver_plausible_edge && IsAnglePlausible( pRecord, resolverData.m_angEdgeAngle.y ) ) {
				pRecord->m_angEyeAngles.y = resolverData.m_angEdgeAngle.y;
				resolverData.m_iCurrentResolverType = 93;
				return;
			}
		}
	}

	// the enemy still hasn't performed the first 0.22 lby flick
	// until that flick happens, we can force lby
	if( resolverData.m_iMissedShotsLBY < 2 ) {
		if( flAnimTimeDelta < XorFlt( 0.22f ) && resolverData.m_flLastMoveAnimTime != FLT_MAX ) {
			resolverData.m_iCurrentResolverType = 10;
			pRecord->m_angEyeAngles.y = resolverData.m_flLastMoveBody;
			//resolverData.m_bInMoveBodyStage = true;

			return;
		}
	}

	const float flMoveDelta = fabs( resolverData.m_flLastMoveBody - pRecord->m_flLowerBodyYawTarget );

	// we have valid move data, use it
	// note - michal;
	// todo, if miss one shot, and then we hit properly (head or feet) on the anti-freestand
	// stage then take note of this, and the next time anti-freestand for first shot instead of last move lby

	// of course if we then miss the anti-freestand shot, for next shot try shooting and last move lby
	const float flXored90 = XorFlt( 90.f );
	const float flXored115 = XorFlt( 115.f );
	const float flXored180 = XorFlt( 180.f );

	QAngle angAway;
	pLocal->IsDead( ) ? Vector( 0, 180, 0 ) : Math::VectorAngles( pLocal->m_vecOrigin( ) - pRecord->m_vecOrigin, angAway );

	// we have height advantage over this player
	// and we are basically above him, handle him separately
	/*if( resolverData.m_bHasHeightAdvantage ) {
		return ResolveHeight( pRecord );
	}*/

	switch( resolverData.m_iMissedShots % 6 ) {
		case 0:
			if( resolverData.m_flFreestandYaw != FLT_MAX ) {
				resolverData.m_iCurrentResolverType = 102;
				resolverData.m_bUsedFreestandPreviously = true;
				pRecord->m_angEyeAngles.y = resolverData.m_flFreestandYaw;
			}
			else {
				resolverData.m_iCurrentResolverType = 101;
				pRecord->m_angEyeAngles.y = resolverData.m_flLastMoveBody;
			}
			break;
		case 1:
			// we used anti-freestand resolver previously (case0),
			// don't shoot at the same angle twice
			if( resolverData.m_bUsedFreestandPreviously ) {
				// don't shoot at away angle twice in a row
				if( resolverData.m_bUsedAwayPreviously ) {
					resolverData.m_iCurrentResolverType = 123;

					// we won't reach m_iCurrentResolverType 122 after anyway, so why not :D
					pRecord->m_angEyeAngles.y = resolverData.m_flLastMoveBody + flXored180;
				}
				// resolve them as backwards, haven't shot it 
				// before so let's do it now
				else {
					resolverData.m_iCurrentResolverType = 121;
					pRecord->m_angEyeAngles.y = angAway.y;
				}
			}
			// we're free to shoot at the anti-freestand angle
			// when it's valid, if not force last move lby +180
			else {
				resolverData.m_iCurrentResolverType = 122;
				pRecord->m_angEyeAngles.y = resolverData.m_flFreestandYaw != FLT_MAX ? resolverData.m_flFreestandYaw : resolverData.m_flLastMoveBody + flXored180;
			}
			break;
		case 2:
			resolverData.m_iCurrentResolverType = 13;
			pRecord->m_angEyeAngles.y = SnapToClosestYaw( resolverData.m_flLastMoveBody - flXored90 );
			break;
		case 3:
			resolverData.m_iCurrentResolverType = 14;
			pRecord->m_angEyeAngles.y = SnapToClosestYaw( resolverData.m_flLastMoveBody + flXored90 );
			break;
		case 4:
			if( flMoveDelta > 35.f ) {
				resolverData.m_iCurrentResolverType = 15;
				pRecord->m_angEyeAngles.y = SnapToClosestYaw( resolverData.m_flLastMoveBody - flMoveDelta );
			}
			else {
				resolverData.m_iCurrentResolverType = 16;
				pRecord->m_angEyeAngles.y = SnapToClosestYaw( resolverData.m_flLastMoveBody - flXored115 );
			}
			break;
		case 5:
			if( flMoveDelta > 35.f ) {
				resolverData.m_iCurrentResolverType = 17;
				pRecord->m_angEyeAngles.y = SnapToClosestYaw( resolverData.m_flLastMoveBody + flMoveDelta );
			}
			else {
				resolverData.m_iCurrentResolverType = 18;
				pRecord->m_angEyeAngles.y = SnapToClosestYaw( resolverData.m_flLastMoveBody + flXored115 );
			}
			break;
	}
}

void Resolver::ResolveMove( AnimationRecord *pRecord ) {
	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );

	resolverData.m_flLastMoveBody = pRecord->m_flLowerBodyYawTarget;
	resolverData.m_vecLastMoveOrigin = pRecord->m_vecOrigin;
	resolverData.m_flLastMoveAnimTime = pRecord->m_flAnimationTime;

	resolverData.m_flLowerBodyRealignTimer = pRecord->m_flAnimationTime + XorFlt( 0.22f );

	// we saw him move again, let's repredict and allow shooting at lby flicks!
	if( pRecord->m_vecVelocity.Length2D( ) >= 35.f )
		resolverData.m_iMissedShotsLBY = 0;

	resolverData.m_iCurrentResolverType = 19;

	resolverData.m_bInPredictionStage = false;

	pRecord->m_angEyeAngles.y = pRecord->m_flLowerBodyYawTarget;
}

void Resolver::ResolveAir( AnimationRecord *pRecord ) {
	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );

	const float flVelocityDirYaw = RAD2DEG( std::atan2( pRecord->m_vecVelocity.x, pRecord->m_vecVelocity.y ) );

	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	QAngle angAway;
	pLocal->IsDead( ) ? Vector( 0, 180, 0 ) : Math::VectorAngles( pLocal->m_vecOrigin( ) - pRecord->m_vecOrigin, angAway );

	const float flXored180 = XorFlt( 180.f );

	switch( resolverData.m_iMissedShots % 5 ) {
		case 0:
			resolverData.m_iCurrentResolverType = 20;
			// RETARDDED XD
			// pRecord->m_angEyeAngles.y = pRecord->m_flLowerBodyYawTarget;
			pRecord->m_angEyeAngles.y = flVelocityDirYaw + flXored180;
			break;
		case 1:
			if( resolverData.m_flLastMoveBody < FLT_MAX && abs( Math::AngleDiff( pRecord->m_flLowerBodyYawTarget, resolverData.m_flLastMoveBody ) ) > 60.f ) {
				resolverData.m_iCurrentResolverType = 21;
				pRecord->m_angEyeAngles.y = resolverData.m_flLastMoveBody;
			}
			else {
				resolverData.m_iCurrentResolverType = 22;
				pRecord->m_angEyeAngles.y = angAway.y /*+ 180.f*/;
			}
			break;
		case 2:
			resolverData.m_iCurrentResolverType = 23;
			pRecord->m_angEyeAngles.y = angAway.y + flXored180;
			break;
		case 3:
			resolverData.m_iCurrentResolverType = 24;
			pRecord->m_angEyeAngles.y = flVelocityDirYaw - ( flXored180 / 2.f );
			break;
		case 4:
			resolverData.m_iCurrentResolverType = 24;
			pRecord->m_angEyeAngles.y = flVelocityDirYaw + ( flXored180 / 2.f );
			// pRecord->m_angEyeAngles.y = flVelocityDirYaw + ( flXored180 / 2.f );
			break;
	}
}

void Resolver::ResolveAirUntrusted( AnimationRecord *pRecord ) {
	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );

	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	QAngle angAway;
	pLocal->IsDead( ) ? Vector( 0, 180, 0 ) : Math::VectorAngles( pLocal->m_vecOrigin( ) - pRecord->m_vecOrigin, angAway );

	switch( resolverData.m_iMissedShots % 9 ) {
		case 0:
			pRecord->m_angEyeAngles.y = angAway.y + 180.f;
			resolverData.m_iCurrentResolverType = 40;
			break;
		case 1:
			pRecord->m_angEyeAngles.y = angAway.y + 150.f;
			resolverData.m_iCurrentResolverType = 41;
			break;
		case 2:
			pRecord->m_angEyeAngles.y = angAway.y - 150.f;
			resolverData.m_iCurrentResolverType = 42;
			break;
		case 3:
			pRecord->m_angEyeAngles.y = angAway.y + 165.f;
			resolverData.m_iCurrentResolverType = 43;
			break;
		case 4:
			pRecord->m_angEyeAngles.y = angAway.y - 165.f;
			resolverData.m_iCurrentResolverType = 44;
			break;
		case 5:
			pRecord->m_angEyeAngles.y = angAway.y + 135.f;
			resolverData.m_iCurrentResolverType = 45;
			break;
		case 6:
			pRecord->m_angEyeAngles.y = angAway.y - 135.f;
			resolverData.m_iCurrentResolverType = 46;
			break;
		case 7:
			pRecord->m_angEyeAngles.y = angAway.y + 90.f;
			resolverData.m_iCurrentResolverType = 47;
			break;
		case 8:
			pRecord->m_angEyeAngles.y = angAway.y - 90.f;
			resolverData.m_iCurrentResolverType = 48;
			break;
		default:
			break;
	}
}

void Resolver::ResolvePred( AnimationRecord *pRecord ) {
	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );

	resolverData.m_iCurrentResolverType = 25;
	pRecord->m_angEyeAngles.y = pRecord->m_flLowerBodyYawTarget;

	resolverData.m_bInPredictionStage = true;
}

void Resolver::ResolveLogic( AnimationRecord *pRecord ) {
	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );

	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	auto pState = pRecord->m_pEntity->m_PlayerAnimState( );
	if( !pState )
		return;

	resolverData.m_iCurrentResolverType = 420;

	// force lby here
	pRecord->m_angEyeAngles.y = pRecord->m_flLowerBodyYawTarget;

	// negate the footyaw
	const float flFootYawDelta = pState->m_flFootYaw - pState->m_flEyeYaw;

	// bruteforce incase our calculation went wrong
	switch( resolverData.m_iMissedShotsDesync % 2 ) {
		case 0:
			pState->m_flFootYaw = pState->m_flEyeYaw - flFootYawDelta;
			break;
		case 1:
			pState->m_flFootYaw = pState->m_flEyeYaw + flFootYawDelta;
			break;
	}
}

// FATALITY XDDDDDD
void Resolver::ResolveOverride( AnimationRecord *pRecord ) {
	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );
	resolverData.m_bOverriding = false;

	C_CSPlayer *pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( !g_Vars.rage.antiaim_resolver_override )
		return;

	static std::vector<C_CSPlayer *> targets;

	static auto weapon_recoil_scale = g_pCVar->FindVar( XorStr( "weapon_recoil_scale" ) );
	static auto last_checked = 0.f;

	QAngle viewangles;
	g_pEngine->GetViewAngles( viewangles );

	if( last_checked != g_pGlobalVars->curtime ) {
		last_checked = g_pGlobalVars->curtime;
		targets.clear( );

		const auto needed_fov = 20.f;
		for( auto i = 1; i <= g_pGlobalVars->maxClients; i++ ) {
			auto ent = ( C_CSPlayer * )g_pEntityList->GetClientEntity( i );
			if( !ent || ent->IsDead( ) || ent->IsDormant( ) )
				continue;

			const auto fov = Math::GetFov( viewangles, pLocal->GetEyePosition( ), ent->WorldSpaceCenter( ) );
			if( fov < needed_fov ) {
				targets.push_back( ent );
			}
		}
	}

	bool had_target = false;
	if( targets.empty( ) ) {
		had_target = false;
		return;
	}

	auto found = false;
	for( auto &target : targets ) {
		if( pRecord->m_pEntity == target ) {
			found = true;
			break;
		}
	}

	if( !found )
		return;

	static auto last_delta = 0.f;
	static auto last_angle = 0.f;

	//Vector angAway;
	//pLocal->IsDead( ) ? Vector( 0, 180, 0 ) : Math::VectorAngles( pLocal->m_vecOrigin( ) - pRecord->m_vecOrigin, angAway );

	const float at_target_yaw = Math::CalcAngle( pLocal->m_vecOrigin( ), pRecord->m_vecOrigin ).y;

	auto delta = Math::AngleNormalize( viewangles.y - at_target_yaw );

	if( had_target && fabsf( viewangles.y - last_angle ) < 0.1f ) {
		viewangles.y = last_angle;
		delta = last_delta;
	}

	had_target = true;

	//	g_EventLog.PushEvent( std::to_string( delta ), Color_f( 1.f, 1.f, 1.f, 1.f ), true );

	resolverData.m_iCurrentResolverType = 26;

	if( delta >= 4.0f )
		pRecord->m_angEyeAngles.y = Math::AngleNormalize( at_target_yaw + 90.f );
	else if( delta <= -4.0f )
		pRecord->m_angEyeAngles.y = Math::AngleNormalize( at_target_yaw - 90.f );
	else
		pRecord->m_angEyeAngles.y = Math::AngleNormalize( at_target_yaw );

	resolverData.m_bOverriding = true;

	last_angle = viewangles.y;
	last_delta = delta;
}

float Resolver::ResolveAntiFreestand( AnimationRecord *pRecord ) {
	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );

	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return FLT_MAX;

	const float flXored4 = XorFlt( 4.f );
	const float flXored32 = XorFlt( 32.f );
	const float flXored90 = XorFlt( 90.f );

	QAngle angAway;
	pLocal->IsDead( ) ? Vector( 0, 180, 0 ) : Math::VectorAngles( pLocal->m_vecOrigin( ) - pRecord->m_vecOrigin, angAway );

	auto enemy_eyepos = pRecord->m_pEntity->GetEyePosition( );

	// construct vector of angles to test.
	std::vector< AdaptiveAngle > angles{ };
	angles.emplace_back( angAway.y );
	angles.emplace_back( angAway.y + flXored90 );
	angles.emplace_back( angAway.y - flXored90 );

	// start the trace at the enemy shoot pos.
	auto start = pLocal->GetEyePosition( );

	// see if we got any valid result.
	// if this is false the path was not obstructed with anything.
	bool valid{ false };

	// iterate vector of angles.
	for( auto it = angles.begin( ); it != angles.end( ); ++it ) {

		// compute the 'rough' estimation of where our head will be.
		Vector end{ enemy_eyepos.x + std::cos( DEG2RAD( it->m_yaw ) ) * flXored32,
			enemy_eyepos.y + std::sin( DEG2RAD( it->m_yaw ) ) * flXored32,
			enemy_eyepos.z };

		// draw a line for debugging purposes.
		//g_csgo.m_debug_overlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.1f );

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
			it->m_dist += ( flXored4 * g_AntiAim.UpdateFreestandPriority( len, i ) );

			// mark that we found anything.
			valid = true;
		}
	}

	if( !valid ) {
		return FLT_MAX;
	}

	// put the most distance at the front of the container.
	std::sort( angles.begin( ), angles.end( ),
			   [ ] ( const AdaptiveAngle &a, const AdaptiveAngle &b ) {
		return a.m_dist > b.m_dist;
	} );

	// the best angle should be at the front now.
	AdaptiveAngle *best = &angles.front( );

	return Math::AngleNormalize( best->m_yaw );
}

void Resolver::OverrideResolver( AnimationRecord *pRecord ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );
	auto playerListInfo = g_PlayerList.GetSettings( pRecord->m_pEntity->GetSteamID( ) );

	if( playerListInfo.m_bForcePitch )
		pRecord->m_angEyeAngles.x = playerListInfo.m_flForcedPitch;

	// todo - maxwell; should edge correction or force yaw have priority?...
	/*if( playerListInfo.m_bEdgeCorrection ) {
		const auto bEdgeData = g_AntiAim.HandleEdge( pRecord->m_pEntity );
		const bool bOnGround = ( pRecord->m_fFlags & FL_ONGROUND );
		const bool bShouldEdge = bEdgeData.first && bEdgeData.second != FLT_MAX && bOnGround && pRecord->m_vecVelocity.Length2D() <= 0.1f;

		if( bShouldEdge ) {
			pRecord->m_angEyeAngles.y = bEdgeData.second;
			resolverData.m_iCurrentResolverType = 1337;
			return;
		}
	}*/

	if( !playerListInfo.m_iForceYaw ) {
		return;
	}

	// mark resolver type as override
	resolverData.m_iCurrentResolverType = 1337;

	// static
	if( playerListInfo.m_iForceYaw == 1 ) {
		pRecord->m_angEyeAngles.y = playerListInfo.m_flForcedYaw;
	}

	// away from me
	else if( playerListInfo.m_iForceYaw == 2 ) {
		const float yaw = Math::CalcAngle( pRecord->m_pEntity->m_vecOrigin( ), pLocal->m_vecOrigin( ) ).y;

		pRecord->m_angEyeAngles.y = yaw + playerListInfo.m_flForcedYaw;
	}

	// lower body yaw
	else if( playerListInfo.m_iForceYaw == 3 ) {
		pRecord->m_angEyeAngles.y = pRecord->m_flLowerBodyYawTarget + playerListInfo.m_flForcedYaw;
	}

	// nearest enemy
	else if( playerListInfo.m_iForceYaw == 4 ) {
		// todo - maxwell; what did this do?
	}

	// average enemy
	else if( playerListInfo.m_iForceYaw == 5 ) {
		// todo - maxwell; what did this do?
	}
}

void Resolver::ResolvePlayers( AnimationRecord *pRecord, AnimationRecord *pPreviousRecord, AnimationRecord *pPenultimateRecord ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( !pRecord )
		return;

	auto &resolverData = m_arrResolverData.at( pRecord->m_pEntity->EntIndex( ) );

	player_info_t info;
	g_pEngine->GetPlayerInfo( pRecord->m_pEntity->EntIndex( ), &info );
	if( !g_Vars.rage.antiaim_correction )
		return;

	if( !g_Vars.rage.antiaim_resolver )
		return;

	if( info.fakeplayer ) {
		pRecord->m_bIsResolved = true;
		return;
	}

	// todo - maxwell; add a toggle for this somewhere..
	if( pRecord->m_iChokeTicks <= 1 ) {
		//pRecord->m_AnimationFlags |= ELagRecordFlags::RF_IsResolved;
		return;
	}

	if( g_PlayerList.GetSettings( pRecord->m_pEntity->GetSteamID( ) ).m_bDisableResolver ) {
		//pRecord->m_AnimationFlags |= ELagRecordFlags::RF_IsResolved;
		return;
	}

	resolverData.m_flFreestandYaw = ResolveAntiFreestand( pRecord );
	g_AntiAim.DoEdgeAntiAim( pRecord->m_pEntity, resolverData.m_angEdgeAngle );

	const float flViewDelta = fabs( pRecord->m_pEntity->GetEyePosition( ).z < pLocal->m_vecOrigin( ).z );
	// make sure player is not too far away (for instance you're on upper stairs mirage, shooting at a
	// player who's jungle, we don't want to handle him separatly)
	const float flOriginDelta = pRecord->m_vecOrigin.Distance( pLocal->m_vecOrigin( ) );
	resolverData.m_bHasHeightAdvantage = flOriginDelta < 25.f && flViewDelta > 5.f && pRecord->m_pEntity->GetEyePosition( ).z < pLocal->m_vecOrigin( ).z &&
		!resolverData.m_bSuppressHeightResolver;

	// update desync flick detection
	UpdateDesyncDetection( pRecord, pPreviousRecord, pPenultimateRecord );

	// update lby breaker detection
	UpdateBodyDetection( pRecord, pPreviousRecord );

	// reset lby flick
	pRecord->m_bLBYFlicked = false;

	// handle lby prediction
	// note - michal;
	// i'll let lby prediction run when they're 'not breaking lby'
	// so that we can see when they fail to break lby, it's more accurate
	UpdateLBYPrediction( pRecord );

	// handle resolver stages
	UpdateResolverStage( pRecord );

	switch( resolverData.m_eCurrentStage ) {
		case EResolverStages::RESOLVE_MODE_STAND:
			ResolveStand( pRecord, pPreviousRecord );
			break;
		case EResolverStages::RESOLVE_MODE_MOVE:
			ResolveMove( pRecord );
			break;
		case EResolverStages::RESOLVE_MODE_AIR:
			if( g_Vars.misc.anti_untrusted )
				ResolveAir( pRecord );
			else
				ResolveAirUntrusted( pRecord );
			break;
		case EResolverStages::RESOLVE_MODE_PRED:
			ResolvePred( pRecord );
			break;
		case EResolverStages::RESOLVE_MODE_OVERRIDE:
			ResolveOverride( pRecord );
			break;
		case EResolverStages::RESOLVE_MODE_LOGIC:
			ResolveLogic( pRecord );
			break;
	}

	// note - maxwell; do something else eventually, this will work for now. lolol.
	if( !g_Vars.misc.anti_untrusted ) {
		switch( resolverData.m_iMissedShots % 5 ) {
			case 0:
			case 1:
				pRecord->m_angEyeAngles.x = 89.f;
				break;
			case 2:
			case 3:
				pRecord->m_angEyeAngles.x = -89.f;
				break;
			case 4:
				pRecord->m_angEyeAngles.x = 0.f;
				break;
		}
	}

	// note - maxwell; this has to be called last.
	OverrideResolver( pRecord );

	// run this last. ;)
#if 1
	if( resolverData.m_flServerYaw != FLT_MAX && ( resolverData.m_flLastReceivedCheatDataTime > 0.f && fabs( resolverData.m_flLastReceivedCheatDataTime - TICKS_TO_TIME( g_pGlobalVars->tickcount ) ) < 2.5f ) && resolverData.m_eCurrentStage != EResolverStages::RESOLVE_MODE_PRED
		&& resolverData.m_eCurrentStage != EResolverStages::RESOLVE_MODE_MOVE ) {
		resolverData.m_iCurrentResolverType = 979;

		pRecord->m_angEyeAngles.y = resolverData.m_flServerYaw;
	}
#endif

	// normalize the angle (because of the bruteforce!!!!)
	pRecord->m_angEyeAngles.y = Math::AngleNormalize( pRecord->m_angEyeAngles.y );

	// resolve player
	pRecord->m_pEntity->m_angEyeAngles( ).y = pRecord->m_angEyeAngles.y;
	pRecord->m_pEntity->m_angEyeAngles( ).x = pRecord->m_angEyeAngles.x;

	// mark this player as resolved when resolver deems so
	pRecord->m_bIsResolved =
		( resolverData.m_eCurrentStage == EResolverStages::RESOLVE_MODE_MOVE ||
		  resolverData.m_eCurrentStage == EResolverStages::RESOLVE_MODE_PRED && ( pRecord->m_pEntity->m_fFlags( ) & FL_ONGROUND ) ) ||
		resolverData.m_iCurrentResolverType == 979;
}