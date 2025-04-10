#include "../../source.hpp"

#include "Ragebot.hpp"
#include "../Rage/EnginePrediction.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../SDK/Valve/CBaseHandle.hpp"
#include "../../Utils/InputSys.hpp"
#include "../../Renderer/Render.hpp"
#include "../../Utils/Threading/threading.h"
#include <algorithm>
#include <atomic>
#include <thread>
#include "../../SDK/Displacement.hpp"
#include "Resolver.hpp"
#include "../Visuals/EventLogger.hpp"
#include "Resolver.hpp"
#include "ShotHandling.hpp"
#include "TickbaseShift.hpp"
#include "../visuals/visuals.hpp"
#include "../Visuals/Chams.hpp"
#include "../Miscellaneous/PlayerList.hpp"
#include "../Rage/FakeLag.hpp"
#include "../Scripting/Scripting.hpp"
#include "../Miscellaneous/Movement.hpp"

#include <sstream>

#include "../../Utils/Threading/shared_mutex.h"

Ragebot g_Ragebot;

#define MT_AWALL

static SharedMutex smtx;

CVariables::RAGE *Ragebot::GetRageSettings( ) {
	if( !m_AimbotInfo.m_pWeapon || !m_AimbotInfo.m_pWeaponInfo )
		return nullptr;

	CVariables::RAGE *current = nullptr;

	auto id = m_AimbotInfo.m_pWeapon->m_iItemDefinitionIndex( );

	// for now, I might do make aimbot also run on zeus
	if( id == WEAPON_ZEUS )
		return false;

	current = &g_Vars.rage_default;

	/*switch( m_AimbotInfo.m_pWeaponInfo->m_iWeaponType ) {
	case WEAPONTYPE_PISTOL:
		if( id == WEAPON_DEAGLE || id == WEAPON_REVOLVER )
			current = &g_Vars.rage_heavypistols;
		else
			current = &g_Vars.rage_pistols;
		break;
	case WEAPONTYPE_SUBMACHINEGUN:
		current = &g_Vars.rage_smgs;
		break;
	case WEAPONTYPE_RIFLE:
		current = &g_Vars.rage_rifles;
		break;
	case WEAPONTYPE_SHOTGUN:
		current = &g_Vars.rage_shotguns;
		break;
	case WEAPONTYPE_SNIPER_RIFLE:
		if( id == WEAPON_G3SG1 || id == WEAPON_SCAR20 )
			current = &g_Vars.rage_autosnipers;
		else
			current = ( id == WEAPON_AWP ) ? &g_Vars.rage_awp : &g_Vars.rage_scout;
		break;
	case WEAPONTYPE_MACHINEGUN:
		current = &g_Vars.rage_heavys;
		break;
	default:
		current = &g_Vars.rage_default;
		break;
	}*/

	if( !current )
		return nullptr;

	if( !current->override_default_config ) {
		current = &g_Vars.rage_default;
	}

	return current;
}

bool Ragebot::AutoStop( bool bBetweenShots ) {

	if( !m_AimbotInfo.m_pSettings->quick_stop )
		return true;

	if( g_Vars.rage_default.quick_stop_key.key > 0 ) {
		if( !g_Vars.rage_default.quick_stop_key.enabled )
			return true;
	}

	if( !( m_AimbotInfo.m_pLocal->m_fFlags( ) & FL_ONGROUND ) )
		return true;

	if( g_Vars.globals.m_bInsideFireRange && !g_Vars.rage_default.quick_stop_stop_in_fire )
		return true;

	// don't retry if we already tried to stop this tick..
	//if( m_AimbotInfo.m_bDidStop )
	//	return true;

	if( !m_AimbotInfo.m_pCmd || !m_AimbotInfo.m_pWeapon )
		return false;

	Vector vecVelocity = g_Prediction.m_vecVelocity;
	vecVelocity.z = 0.0f;

	float vecVelocityLength = vecVelocity.Length2D( );

	if( g_Vars.rage_default.quick_stop == 2 ) {
		m_AimbotInfo.m_pCmd->buttons |= IN_DUCK;
	}

	const float maxSpeed = m_AimbotInfo.m_pWeapon->GetMaxSpeed( ) * 0.3300000;
	if( bBetweenShots || vecVelocityLength <= maxSpeed ) {
		g_Movement.MovementControl( m_AimbotInfo.m_pCmd, maxSpeed );
	}
	else {
		if( vecVelocityLength > 15.f ) {
			QAngle dir;
			Math::VectorAngles( vecVelocity, dir );
			dir.y = m_AimbotInfo.m_pCmd->viewangles.y - dir.y;

			Vector new_move;
			Math::AngleVectors( dir, new_move );
			const auto max = std::max( std::fabs( m_AimbotInfo.m_pCmd->forwardmove ), std::fabs( m_AimbotInfo.m_pCmd->sidemove ) );
			const auto mult = 450.f / max;
			new_move *= -mult;

			m_AimbotInfo.m_pCmd->forwardmove = new_move.x;
			m_AimbotInfo.m_pCmd->sidemove = new_move.y;
		}
		else {
			m_AimbotInfo.m_pCmd->forwardmove = 0.f;
			m_AimbotInfo.m_pCmd->sidemove = 0.f;
		}
	}

	//m_AimbotInfo.m_bDidStop = true;
	return true;
}

bool Ragebot::AutoScope( ) {
	if( !m_AimbotInfo.m_pSettings->auto_scope )
		return false;

	if( !m_AimbotInfo.m_pCmd )
		return false;

	if( m_AimbotInfo.m_pWeaponInfo->m_iWeaponType != WEAPONTYPE_SNIPER_RIFLE )
		return false;

	if( m_AimbotInfo.m_pWeapon->m_zoomLevel( ) >= 1 )
		return false;

	if( !( m_AimbotInfo.m_pLocal->m_fFlags( ) & FL_ONGROUND ) || !( g_Prediction.m_fFlags & FL_ONGROUND ) )
		return false;

	// remove attack (so we can properly scope)
	m_AimbotInfo.m_pCmd->buttons &= ~IN_ATTACK;

	// set in_attack2 (scope nibba)
	m_AimbotInfo.m_pCmd->buttons |= IN_ATTACK2;

	return true;
}

int Ragebot::GetMinimalDamage( AimPoint_t *pPoint ) {
	if( !pPoint )
		return 420;

	if( !pPoint->m_pTarget )
		return 1337;

	if( !pPoint->m_pTarget->m_pEntity )
		return 420;

	if( !pPoint->m_pTarget->m_pRecord.player )
		return 1337420;

	const int nPlayerHP = pPoint->m_pTarget->m_pEntity->m_iHealth( );
	auto GetHPDamage = [ & ] ( int nDamage ) {
		return nDamage > 100 ? ( nPlayerHP + ( nDamage - 100 ) ) : nDamage;
	};

	int nMinDamage = m_AimbotInfo.m_pSettings->minimal_damage;

	if( g_Vars.rage.scale_damage_hp ) {
		nMinDamage *= ( nPlayerHP / 100.f );
	}

	if( g_Vars.rage_default.minimal_damage_override && g_Vars.rage_default.minimal_damage_override_key.enabled ) {
		nMinDamage = g_Vars.rage_default.minimal_damage_override_amt;
	}

	bool bAutomatic = nMinDamage == 0;
	int nDamage = GetHPDamage( nMinDamage );

	if( bAutomatic ) {
		const float flMaxBodyDamage = Autowall::ScaleDamage( pPoint->m_pTarget->m_pEntity, m_AimbotInfo.m_pWeaponInfo->m_iWeaponDamage, m_AimbotInfo.m_pWeaponInfo->m_flArmorRatio, Hitgroup_Stomach ) - 1;

		// force lethal shot
		if( flMaxBodyDamage >= pPoint->m_pTarget->m_pEntity->m_iHealth( ) ) {
			nDamage = flMaxBodyDamage;

			// in some cases the calculated point damage will be 81 
			// but flMaxBodyDamage will return 82 causing you to not shoot

			// idea behind magic number "10" is because of the factor it applies when penetrated (still remains at 6 damage, which should be fine through wall..)
			if( pPoint->m_iDamage >= 10 && pPoint->m_iDamage >= pPoint->m_pTarget->m_pEntity->m_iHealth( ) ) {
				nDamage = pPoint->m_iDamage;
			}
		}

		// cant kill with one lethal shot, aim to kill in 2 bullets

		// note - michal;
		// for the first shot it will probably hit for some damage above,
		// then the if statement will return false and other conditions
		// will be applied. not sure if i want it like this but i'll leave it for now
		if( pPoint->m_pTarget->m_pEntity->m_iHealth( ) > flMaxBodyDamage ) {
			int tempDamage = flMaxBodyDamage / 2;
			nDamage = flMaxBodyDamage / 2;
		}

		// shooting thru wall
		if( pPoint->m_pFireBulletData->m_iPenetrationCount < 4 )
			nDamage *= 0.6f;
	}

	g_Vars.globals.m_nCurrentMinDmg = nDamage;

	return nDamage;
}

C_LagRecord Ragebot::GetBestLagRecord( AimTarget_t *pTarget ) {
	auto pLagData = g_LagCompensation.GetLagData( pTarget->m_pEntity->m_entIndex );
	if( !pLagData || pLagData->m_Records.empty( ) )
		return { };

	C_LagRecord pFirstValid;
	bool bFoundFirstValid = false, bBrokeTeleportDist = false;

	for( const auto it : pLagData->m_Records ) {
		if( it.m_bIsInvalid || g_LagCompensation.IsRecordOutOfBounds( it ) )
			continue;

		if( !bFoundFirstValid ) {
			pFirstValid = it;
			bFoundFirstValid = true;
		}

		if( it.m_bBrokeTeleportDst ) {
			bBrokeTeleportDist = true;
			continue;
		}

		if( it.m_bIsResolved || it.m_bLBYFlicked )
			return it;
	}

	if( bFoundFirstValid || bBrokeTeleportDist )
		return pFirstValid;

	return { };
}

std::vector<Hitboxes> Ragebot::GetHitboxes( ) {
	std::vector<Hitboxes> vecHitboxes{ };

	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return { };

	const bool bIgnoreLimbs = m_AimbotInfo.m_pSettings->ignore_limbs_move && ( pLocal->m_vecVelocity( ).Length2D( ) > 0.1f && !g_Vars.globals.m_bFakeWalking ) && pLocal->m_fFlags( ) & FL_ONGROUND;

	if( g_Vars.rage.force_baim.enabled ) {
		if( m_AimbotInfo.m_pSettings->hitbox_chest ) {
			vecHitboxes.push_back( Hitboxes::HITBOX_UPPER_CHEST );
			vecHitboxes.push_back( Hitboxes::HITBOX_CHEST );
			vecHitboxes.push_back( Hitboxes::HITBOX_LOWER_CHEST );
		}

		if( m_AimbotInfo.m_pSettings->hitbox_pelvis ) {
			vecHitboxes.push_back( Hitboxes::HITBOX_PELVIS );
		}

		// maybe stomach and pelvis one option?
		if( m_AimbotInfo.m_pSettings->hitbox_stomach ) {
			vecHitboxes.push_back( Hitboxes::HITBOX_STOMACH );
		}
	}
	else {
		if( m_AimbotInfo.m_pSettings->hitbox_feet && !bIgnoreLimbs ) {
			vecHitboxes.push_back( Hitboxes::HITBOX_LEFT_FOOT );
			vecHitboxes.push_back( Hitboxes::HITBOX_RIGHT_FOOT );
		}

		if( m_AimbotInfo.m_pSettings->hitbox_legs && !bIgnoreLimbs ) {
			vecHitboxes.push_back( Hitboxes::HITBOX_LEFT_CALF );
			vecHitboxes.push_back( Hitboxes::HITBOX_RIGHT_CALF );

			vecHitboxes.push_back( Hitboxes::HITBOX_LEFT_THIGH );
			vecHitboxes.push_back( Hitboxes::HITBOX_RIGHT_THIGH );
		}

		if( m_AimbotInfo.m_pSettings->hitbox_arms && !bIgnoreLimbs ) {
			vecHitboxes.push_back( Hitboxes::HITBOX_LEFT_HAND );
			vecHitboxes.push_back( Hitboxes::HITBOX_RIGHT_HAND );

			vecHitboxes.push_back( Hitboxes::HITBOX_LEFT_UPPER_ARM );
			vecHitboxes.push_back( Hitboxes::HITBOX_RIGHT_UPPER_ARM );

			vecHitboxes.push_back( Hitboxes::HITBOX_LEFT_FOREARM );
			vecHitboxes.push_back( Hitboxes::HITBOX_RIGHT_FOREARM );
		}

		if( m_AimbotInfo.m_pSettings->hitbox_chest ) {
			vecHitboxes.push_back( Hitboxes::HITBOX_UPPER_CHEST );
			vecHitboxes.push_back( Hitboxes::HITBOX_CHEST );
			vecHitboxes.push_back( Hitboxes::HITBOX_LOWER_CHEST );
		}

		if( m_AimbotInfo.m_pSettings->hitbox_neck ) {
			vecHitboxes.push_back( Hitboxes::HITBOX_NECK );
			//vecHitboxes.push_back( Hitboxes::HITBOX_LOWER_NECK );
		}

		if( m_AimbotInfo.m_pSettings->hitbox_pelvis ) {
			vecHitboxes.push_back( Hitboxes::HITBOX_PELVIS );
		}

		// maybe stomach and pelvis one option?
		if( m_AimbotInfo.m_pSettings->hitbox_stomach ) {
			vecHitboxes.push_back( Hitboxes::HITBOX_STOMACH );
		}

		if( m_AimbotInfo.m_pSettings->hitbox_head ) {
			vecHitboxes.push_back( Hitboxes::HITBOX_HEAD );
		}
	}

	return vecHitboxes;
}

void Ragebot::RunAwall( AimPoint_t *pPoint ) {
	if( !g_pEngine.IsValid( ) )
		return;

	if( !g_pEngine->IsInGame( ) || !g_pEngine->IsConnected( ) )
		return;

	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( pLocal->IsDead( ) )
		return;

	if( !pPoint )
		return;

	if( pPoint->m_pFireBulletData == nullptr )
		return;

	if( !pPoint->m_pTarget )
		return;

	if( !pPoint->m_pTarget->m_pEntity )
		return;

	if( !pPoint->m_pTarget->m_pRecord.player )
		return;

	if( !pPoint->m_pTarget->m_pEntity->m_CachedBoneData( ).m_Memory.m_pMemory )
		return;

	smtx.rlock( );

	// runawall
	Autowall::FireBullets( pPoint->m_pFireBulletData );

	smtx.runlock( );

	pPoint->m_iTraceHitgroup = pPoint->m_pFireBulletData->m_iHitgroup;
	pPoint->m_iDamage = static_cast< int >( pPoint->m_pFireBulletData->m_flCurrentDamage );
	pPoint->m_bPenetrated = pPoint->m_pFireBulletData->m_iPenetrationCount < 4;

	delete pPoint->m_pFireBulletData;
}

bool Ragebot::RunHitscan( std::vector<Hitboxes> hitboxesToScan ) {
	auto vecHitboxes = hitboxesToScan.empty( ) ? GetHitboxes( ) : hitboxesToScan;
	if( vecHitboxes.empty( ) )
		return false;

	auto vecPlayers = FindTargets( );
	if( vecPlayers.empty( ) )
		return false;

	static std::vector<AimTarget_t> vecTargets{ };

	if( !vecTargets.empty( ) )
		vecTargets.clear( );

	for( auto player : vecPlayers ) {
		// somehow got null?
		if( !player ) {
			g_Visuals.AddDebugMessage( std::string( XorStr( __FUNCTION__ ) ).append( XorStr( " -> L" ) + std::to_string( __LINE__ ) ) );
			continue;
		}

		if( !player->IsPlayer( ) ) {
			g_Visuals.AddDebugMessage( std::string( XorStr( __FUNCTION__ ) ).append( XorStr( " -> L" ) + std::to_string( __LINE__ ) ) );
			continue;
		}

		// setup a new target
		AimTarget_t target{ };
		target.m_pEntity = player;

		// setup hitbox set
		if( auto pModel = player->GetModel( ); pModel ) {
			const auto pHdr = g_pModelInfo->GetStudiomodel( pModel );
			if( pHdr ) {
				target.m_pHitboxSet = pHdr->pHitboxSet( player->m_nHitboxSet( ) );
			}
		}

		// fack.
		if( !target.m_pHitboxSet ) {
			g_Visuals.AddDebugMessage( std::string( XorStr( __FUNCTION__ ) ).append( XorStr( " -> L" ) + std::to_string( __LINE__ ) ) );
			continue;
		}

		// get the best lagrecord
		target.m_pRecord = GetBestLagRecord( &target );

		// we couldn't find a proper record?
		// let's not even bother
		if( !target.m_pRecord.player || target.m_pRecord.player == nullptr ) {
			g_Visuals.AddDebugMessage( std::string( XorStr( __FUNCTION__ ) ).append( XorStr( " -> L" ) + std::to_string( __LINE__ ) ) );
			continue;
		}

		for( auto hitbox : vecHitboxes ) {

			if( g_Vars.misc.anti_untrusted ) {
				// note - maxwell; ignore head on jumping players for now.
				// maybe we should make this more conditional, like don't do this if we know we have them resolved, etc..
				if( !( target.m_pRecord.m_iFlags & FL_ONGROUND ) && hitbox == Hitboxes::HITBOX_HEAD )
					continue;
			}

			AddPoints( &target, hitbox );
		}

		vecTargets.push_back( target );
	}

	// run awall on points
	for( auto &target : vecTargets ) {
		for( auto &point : target.m_vecAimPoints ) {
			// allocate new firebulletdata so we can use it correctly
			// in the threading
			point.m_pFireBulletData = new Autowall::C_FireBulletData( );

			// setup firebulletdata
			if( point.m_pFireBulletData != nullptr ) {
				point.m_pFireBulletData->m_vecStart = m_AimbotInfo.m_vecEyePosition;
				point.m_pFireBulletData->m_vecPos = point.m_vecPosition;

				Vector vecDirection = ( point.m_pFireBulletData->m_vecPos - point.m_pFireBulletData->m_vecStart );
				vecDirection.Normalize( );

				point.m_pFireBulletData->m_vecDirection = vecDirection;
				point.m_pFireBulletData->m_iHitgroup = point.m_pHitbox->group;

				point.m_pFireBulletData->m_Player = m_AimbotInfo.m_pLocal;
				point.m_pFireBulletData->m_TargetPlayer = target.m_pEntity;
				point.m_pFireBulletData->m_Weapon = m_AimbotInfo.m_pWeapon;
				point.m_pFireBulletData->m_WeaponData = m_AimbotInfo.m_pWeaponInfo;

				point.m_pFireBulletData->m_bPenetration = m_AimbotInfo.m_pSettings->wall_penetration;

				target.m_pRecord.Apply( target.m_pEntity );

				// queue awall
			#ifdef MT_AWALL
				Threading::QueueJobRef( RunAwall, &point );
			#else
				RunAwall( &point );
			#endif
			}
		}
	}

#ifdef MT_AWALL
	// finish queue (let threads run their jobs)
	Threading::FinishQueue( true );
#endif

	// now filter points
	for( auto &target : vecTargets ) {

		// static so it only initializes once /shrug
		std::vector<AimPoint_t> m_vecFinalPoints{ };

		for( auto &point : target.m_vecAimPoints ) {
			if( point.m_pFireBulletData == nullptr )
				continue;

			const int iDamage = point.m_iDamage;
			const int iMinimalDamage = GetMinimalDamage( &point );

			// more stuff to be filled/added here
			// such as haslethalpoint,...
			if( iDamage >= iMinimalDamage ) {
				// store and convert damage from float to int..
				point.m_iDamage = iDamage;
				point.m_iMinimalDamage = iMinimalDamage;
				point.m_bBody = ( point.m_iTraceHitgroup == Hitgroup_Stomach /*|| point.m_iTraceHitgroup == Hitgroup_Chest*/ );
				point.m_bHead = point.m_iTraceHitgroup == Hitgroup_Head;
				point.m_bLethal = point.m_iDamage >= point.m_pTarget->m_pEntity->m_iHealth( ) && point.m_bBody;

				if( point.m_bBody ) {
					target.m_bHasBodyPoint = true;
				}

				if( point.m_bLethal ) {
					target.m_bHasLethalPoint = true;
				}

				// just to make sure :)
				point.m_pTarget = &target;

				m_vecFinalPoints.push_back( point );
			}
		}

		// override the points
		if( !m_vecFinalPoints.empty( ) ) {
			target.m_vecAimPoints = m_vecFinalPoints;

			// emplace target with valid points
			m_AimbotInfo.m_vecAimData.emplace_back( target );
		}

	}

	vecPlayers.clear( );
	vecHitboxes.clear( );

	// no valid aim data? fuck dude
	if( m_AimbotInfo.m_vecAimData.empty( ) ) {
		g_Visuals.AddDebugMessage( std::string( XorStr( __FUNCTION__ ) ).append( XorStr( " -> L" ) + std::to_string( __LINE__ ) ) );
		return false;
	}

	return true;
}

bool Ragebot::FinishAimbot( ) {
	// find best target
	AimTarget_t bestTarget;

	// if there is only one entry anyways, no need to iterate
	if( m_AimbotInfo.m_vecAimData.size( ) == 1 ) {
		bestTarget = m_AimbotInfo.m_vecAimData.at( 0 );
	}
	else {
		for( auto &data : m_AimbotInfo.m_vecAimData ) {
			// no target yet? we just take this one first.
			if( !bestTarget.m_pEntity ) {
				g_Visuals.AddDebugMessage( std::string( XorStr( __FUNCTION__ ) ).append( XorStr( " -> L" ) + std::to_string( __LINE__ ) ) );

				bestTarget = data;
				continue;
			}

			// prioritise this player
			// sometimes failes idk why

			// maybe it doesn't sometimes fail (bots all have the same steamid)
			if( g_PlayerList.GetSettings( data.m_pEntity->GetSteamID( ) ).m_bHighPriority ) {
				bestTarget = data;
				continue;
			}

			// is the current guys health lower than best? we try to kill it.
			if( data.m_pEntity->m_iHealth( ) < bestTarget.m_pEntity->m_iHealth( ) ) {
				bestTarget = data;
			}
			else {
				// get damage of all points
				int currentDamage{ }, bestDamage{ };

				for( auto &point : data.m_vecAimPoints ) {
					currentDamage += point.m_iDamage;
				}

				for( auto &point : bestTarget.m_vecAimPoints ) {
					bestDamage += point.m_iDamage;
				}

				// current damage higher?
				if( currentDamage > bestDamage ) {
					bestTarget = data;
				}
			}
		}
	}

	// apparently we were not able to find a good target..
	if( !bestTarget.m_pEntity ) {
		g_Visuals.AddDebugMessage( std::string( XorStr( __FUNCTION__ ) ).append( XorStr( " -> L" ) + std::to_string( __LINE__ ) ) );
		return false;
	}

	AimPoint_t bestPoint{ };
	bestPoint.m_bFirstPoint = true;

	float flMaxBodyDamage = Autowall::ScaleDamage( bestTarget.m_pEntity, m_AimbotInfo.m_pWeaponInfo->m_iWeaponDamage, m_AimbotInfo.m_pWeaponInfo->m_flArmorRatio, Hitgroup_Stomach );

	// main aimbot logic (prefer body, safepoint, etc.)
	for( auto &point : bestTarget.m_vecAimPoints ) {
		// if it's the first point we take it regardless
		if( bestPoint.m_bFirstPoint ) {
			bestPoint = point;
			continue;
		}

		// if a lethal point was found, and this point
		// is the lethal point, take it but keep looking
		if( bestTarget.m_bHasLethalPoint ) {
			if( point.m_bLethal ) {
				bestPoint = point;

				// if this point is also the center point,
				// take it and exit out, prob the best point we can shoot at
				if( point.m_bCenter )
					break;
			}
		}

		if( bestTarget.m_pEntity != nullptr ) {
			if( int( flMaxBodyDamage ) >= bestTarget.m_pEntity->m_iHealth( ) ) {
				if( point.m_bBody ) {
					// if body and we got enough damage to one hit
					// we take for now
					bestPoint = point;

					// if this is lethal, we want it..
					if( bestPoint.m_bLethal )
						break;
				}
			}
		}

		bool bShouldChooseBodyPoint = false;

		// we haven't found a body point yet
		if( !bestPoint.m_bBody ) {
			if( g_Vars.rage_default.prefer_body > 0 ) {
				// fake angles
				if( g_Vars.rage_default.prefer_body == 1 ) {
					bShouldChooseBodyPoint = !bestTarget.m_pRecord.m_bIsResolved;
				}
				// always
				else if( g_Vars.rage_default.prefer_body == 2 ) {
					bShouldChooseBodyPoint = true;
				}
				// aggressive
				else if( g_Vars.rage_default.prefer_body == 3 ) {
					// 2 shots will be enough to kill him in the body
					bShouldChooseBodyPoint = int( flMaxBodyDamage ) >= ( bestTarget.m_pEntity->m_iHealth( ) * 0.5f );
				}
				// high inaccuracy
				else if( g_Vars.rage_default.prefer_body == 4 ) {
					const bool bDucked = m_AimbotInfo.m_pLocal->m_fFlags( ) & FL_DUCKING;
					const bool bSniper = m_AimbotInfo.m_pWeaponInfo->m_iWeaponType == WEAPONTYPE_SNIPER_RIFLE;
					const auto fnRoundAccuracy = [ ] ( const float accuracy ) { return roundf( accuracy * 1000.f ) / 1000.f; };

					const float flRoundedInaccuracy = fnRoundAccuracy( g_Prediction.m_flInaccuracy );

					const bool bAccurate = flRoundedInaccuracy == (
						bDucked ? fnRoundAccuracy( bSniper ? m_AimbotInfo.m_pWeaponInfo->m_flInaccuracyCrouchAlt : m_AimbotInfo.m_pWeaponInfo->m_flInaccuracyCrouch ) :
						fnRoundAccuracy( bSniper ? m_AimbotInfo.m_pWeaponInfo->m_flInaccuracyStandAlt : m_AimbotInfo.m_pWeaponInfo->m_flInaccuracyStand ) );

					// 2 shots will be enough to kill him in the body
					bShouldChooseBodyPoint = !bAccurate || g_Resolver.m_arrResolverData[ bestTarget.m_pEntity->EntIndex( ) ].m_iMissedShotsSpread >= 2;
				}
			}

			// prefer body-aim in air when possible
			if( !( bestTarget.m_pEntity->m_fFlags( ) & FL_ONGROUND ) ) {
				bShouldChooseBodyPoint = true;
			}

			// yep we should baim and this point
			// is baim point lets go
			if( bShouldChooseBodyPoint && point.m_bBody ) {
				bestPoint = point;
			}
		}

		// sanity lol
		if( ( bestTarget.m_pEntity->m_fFlags( ) & FL_ONGROUND ) ) {
			// target is resolved, current best point isa body point,
			// found point is head, let's shoot it!
			if( bestTarget.m_pRecord.m_bIsResolved && bestPoint.m_bBody ) {
				if( point.m_bHead ) {
					bestPoint = point;
				}
			}
		}

		// damage higher and we don't already have a lethal point?
		if( ( point.m_iDamage > bestPoint.m_iDamage && !bestPoint.m_bLethal && ( ( !bestPoint.m_bBody && !bShouldChooseBodyPoint ) || g_Vars.rage_default.prefer_body == 0 ) ) ) {
			// printf( "found better point: (%i > %i), (%i -> %i), (%i)\n", point.m_iDamage, bestPoint.m_iDamage, bestPoint.m_iHitbox, point.m_iHitbox, int( bShouldChooseBodyPoint ) );
			bestPoint = point;
		}
		else {
			// printf( "couldn't find better point: (%i - %i), (%i - %i), (%i - %i)\n", point.m_iDamage, bestPoint.m_iDamage, bestPoint.m_iHitbox, point.m_iHitbox, int( bShouldChooseBodyPoint ), int( bestPoint.m_bLethal ) );
		}
	}

	bool bBetweenShots = m_AimbotInfo.m_pWeapon->m_iItemDefinitionIndex( ) != WEAPON_SSG08 && m_AimbotInfo.m_pWeapon->m_iItemDefinitionIndex( ) != WEAPON_AWP;

	bool noAimbot = !m_AimbotInfo.m_pLocal->CanShoot( ) || g_Vars.globals.m_bShotWhileChoking;
	if( noAimbot ) {
		if( bBetweenShots && TICKS_TO_TIME( m_AimbotInfo.m_pLocal->m_nTickBase( ) ) >= m_AimbotInfo.m_pLocal->m_flNextAttack( ) ) {
			AutoStop( true );
		}

		g_Visuals.AddDebugMessage( std::string( XorStr( __FUNCTION__ ) ).append( XorStr( " -> L" ) + std::to_string( __LINE__ ) ) );
		return false;
	}

	if( !bestPoint.m_pTarget || !bestPoint.m_pTarget->m_pEntity ) {
		g_Visuals.AddDebugMessage( std::string( XorStr( __FUNCTION__ ) ).append( XorStr( " -> L" ) + std::to_string( __LINE__ ) ) );
		return false;
	}

	// always stop in the current tick
	AutoStop( g_Vars.rage_default.quick_stop_between_shots );
	AutoScope( );

	float flHitchance{ };
	flHitchance = m_AimbotInfo.m_pSettings->hitchance_amount;

	// by 6 missed shots due to spread we'll have upped the hitchance by 35
	// increments by "5.83333333333" every missed spread shot 
	//flHitchance += 35.f * ( float( g_Resolver.m_arrResolverData.at( bestPoint.m_pTarget->m_pEntity->EntIndex( ) ).m_iMissedShotsSpread % 10 ) / 10.f );

	bool bHitchanced = RunHitchance( &bestPoint, flHitchance );

	// note - maxwell; i stole this from raxer. hopefully this makes us shoot faster at long ranges without sacrificing accuracy since we can't really
	// increase our hitchance here anyway...
	const auto RoundAccuracy = [ ] ( const float accuracy ) {
		return roundf( accuracy * 1000.f ) / 1000.f;
	};

	bool bAutoSniper = m_AimbotInfo.m_pWeapon->m_iItemDefinitionIndex( ) == WEAPON_SCAR20 || m_AimbotInfo.m_pWeapon->m_iItemDefinitionIndex( ) == WEAPON_G3SG1;

	if( m_AimbotInfo.m_pLocal->m_fFlags( ) & FL_DUCKING ) {
		if( RoundAccuracy( g_Prediction.m_flInaccuracy ) == RoundAccuracy( bAutoSniper ? m_AimbotInfo.m_pWeaponInfo->m_flInaccuracyCrouchAlt : m_AimbotInfo.m_pWeaponInfo->m_flInaccuracyCrouch ) )
			bHitchanced = true;
	}
	else {
		if( RoundAccuracy( g_Prediction.m_flInaccuracy ) == RoundAccuracy( bAutoSniper ? m_AimbotInfo.m_pWeaponInfo->m_flInaccuracyStandAlt : m_AimbotInfo.m_pWeaponInfo->m_flInaccuracyStand ) )
			bHitchanced = true;
	}

	// note - maxwell; jump scout accuracy idea stolen from philip, lol.
	if( m_AimbotInfo.m_pWeapon->m_iItemDefinitionIndex( ) == WEAPON_SSG08 && !( m_AimbotInfo.m_pLocal->m_fFlags( ) & FL_ONGROUND ) && g_Prediction.m_flInaccuracy < 0.009f )
		bHitchanced = true;

	if( bHitchanced ) {
		AimAtPoint( &bestPoint );
	}
	else {
		/*bool ret = false;
		if( AutoStop( ) ) {
			ret = true;
		}*/

		//if( ret ) {
		//	g_Visuals.AddDebugMessage( std::string( XorStr( __FUNCTION__ ) ).append( XorStr( " -> L" ) + std::to_string( __LINE__ ) ) );
		return false;
		//}
	}

	if( m_AimbotInfo.m_pCmd->buttons & IN_ATTACK ) {
		PostAimbot( &bestPoint );
		last_shot_command = m_AimbotInfo.m_pCmd->command_number;
	}

	return true;
}

bool Ragebot::RunHitchance( AimPoint_t *pPoint, float chance ) {
	if( !m_AimbotInfo.m_pSettings->hitchance )
		return true;

	if( chance <= 0.0f )
		return true;

	if( !pPoint )
		return false;

	if( !pPoint->m_pTarget )
		return false;

	if( !pPoint->m_pTarget->m_pRecord.player )
		return false;

	constexpr float HITCHANCE_MAX = 100.f;
	constexpr int   SEED_MAX = 255;

	Vector     start{ m_AimbotInfo.m_vecEyePosition }, end, fwd, right, up, dir, wep_spread;
	float      inaccuracy, spread;
	CGameTrace tr;
	size_t     total_hits{ }, needed_hits{ ( size_t )std::ceil( ( chance * SEED_MAX ) / HITCHANCE_MAX ) };

	// nigger.
	Vector delta = pPoint->m_vecPosition - m_AimbotInfo.m_vecEyePosition;
	delta.Normalize( );

	// we do a little pasting.
	QAngle aimAngles;
	Math::VectorAngles( delta, aimAngles );

	// get needed directional vectors.
	Math::AngleVectors( aimAngles + m_AimbotInfo.m_pLocal->m_aimPunchAngle( ) * 2.f, fwd, right, up );

	// store off inaccuracy / spread ( these functions are quite intensive and we only need them once ).
	inaccuracy = g_Prediction.m_flInaccuracy;
	spread = g_Prediction.m_flSpread;

	// iterate all possible seeds.
	for( int i{ }; i <= SEED_MAX; ++i ) {
		// get spread.				
		wep_spread = m_AimbotInfo.m_pWeapon->CalculateSpread( i, inaccuracy, spread );

		// get spread direction.
		dir = ( fwd + ( right * wep_spread.x ) + ( up * wep_spread.y ) ).Normalized( );

		// get end of trace.
		end = start + ( dir * m_AimbotInfo.m_pWeaponInfo->m_flWeaponRange );

		// setup ray and trace.
		g_pEngineTrace->ClipRayToEntity( Ray_t( start, end ), MASK_SHOT, pPoint->m_pTarget->m_pRecord.player, &tr );

		// check if we hit a valid player / hitgroup on the player and increment total hits.
		if( tr.hit_entity == pPoint->m_pTarget->m_pRecord.player /*&& game::IsValidHitgroup( tr.m_hitgroup )*/ )
			++total_hits;

		// we made it.
		if( total_hits >= needed_hits )
			return true;

		// we cant make it anymore.
		if( ( SEED_MAX - i + total_hits ) < needed_hits )
			return false;
	}

	return false;
}

std::vector<C_CSPlayer *> Ragebot::FindTargets( ) {
	std::vector<C_CSPlayer *> vecPlayers{ };

	for( int i = 1; i <= g_pGlobalVars->maxClients; ++i ) {
		const auto player = C_CSPlayer::GetPlayerByIndex( i );
		if( !player )
			continue;

		if( player->IsDead( ) || player->m_bGunGameImmunity( ) || player->IsDormant( ) )
			continue;

		if( player->IsTeammate( m_AimbotInfo.m_pLocal ) )
			continue;

		if( g_PlayerList.GetSettings( player->GetSteamID( ) ).m_bAddToWhitelist )
			continue;

		vecPlayers.push_back( player );
	}

	return vecPlayers;
}

void Ragebot::AddPoint( AimTarget_t *pTarget, mstudiobbox_t *pHitbox, Vector vecPosition, int iHitbox, bool center ) {
	if( !pTarget )
		return;

	if( !pHitbox )
		return;

	AimPoint_t aimPoint{ };
	aimPoint.m_pHitbox = pHitbox;
	aimPoint.m_vecPosition = vecPosition;
	aimPoint.m_pTarget = pTarget;
	aimPoint.m_bCenter = center;
	aimPoint.m_iHitbox = iHitbox;

	pTarget->m_vecAimPoints.push_back( aimPoint );
}

void Ragebot::AddPoints( AimTarget_t *pTarget, Hitboxes hitbox ) {
	if( !pTarget )
		return;

	const auto pMatrix = pTarget->m_pRecord.m_pMatrix;
	if( !pMatrix )
		return;

	const auto pHitbox = pTarget->m_pHitboxSet->pHitbox( hitbox );
	if( !pHitbox )
		return;

	Vector vecCenter = ( pHitbox->bbmax + pHitbox->bbmin ) * 0.5f;
	AddPoint( pTarget, pHitbox, vecCenter.Transform( pMatrix[ pHitbox->bone ] ), hitbox, true );

	float pointScale = m_AimbotInfo.m_pSettings->pointscale;
	if( pointScale <= 0.0f )
		return;

	// yup
	pointScale *= 0.01f;

	bool bHead = true,
		bStomach = true,
		bChest = true,
		bLegs = true,
		bFeet = true;

	// vitals shouldn't multipoint these
	if( g_Vars.rage_default.multipoint == 0 ) {
		pointScale = 0.f;
	}
	// low 
	else if( g_Vars.rage_default.multipoint == 1 ) {
		pointScale *= ( 25.f / 100.f );
	}
	// medium
	else if( g_Vars.rage_default.multipoint == 2 ) {
		pointScale *= ( 65.f / 100.f );
	}
	// high
	else if( g_Vars.rage_default.multipoint == 3 ) {
		pointScale *= ( 100.f / 100.f ); // lol
	}
	// vitals
	else if( g_Vars.rage_default.multipoint == 4 ) {
		pointScale *= ( 85.f / 100.f ); // lol
		bLegs = bFeet = false;
	}

	const bool bCapsule = pHitbox->m_flRadius != -1.0f;
	if( bCapsule ) {
		float radiusScaled = pHitbox->m_flRadius * pointScale;

		// head multipoints
		if( hitbox == Hitboxes::HITBOX_HEAD && bHead ) {
			Vector vecRight{ pHitbox->bbmax.x, pHitbox->bbmax.y, pHitbox->bbmax.z + ( pHitbox->m_flRadius * 0.5f ) };
			AddPoint( pTarget, pHitbox, vecRight.Transform( pMatrix[ pHitbox->bone ] ), hitbox );

			Vector vecLeft{ pHitbox->bbmax.x, pHitbox->bbmax.y, pHitbox->bbmax.z - ( pHitbox->m_flRadius * 0.5f ) };
			AddPoint( pTarget, pHitbox, vecLeft.Transform( pMatrix[ pHitbox->bone ] ), hitbox );

			// top/back 45 deg.
			constexpr float rotation = 0.70710678f;

			pointScale = std::clamp( pointScale, 0.1f, 0.82f );
			radiusScaled = pHitbox->m_flRadius * pointScale;

			Vector vecTopBack{ pHitbox->bbmax.x + ( rotation * radiusScaled ), pHitbox->bbmax.y + ( -rotation * radiusScaled ), pHitbox->bbmax.z };
			AddPoint( pTarget, pHitbox, vecTopBack.Transform( pMatrix[ pHitbox->bone ] ), hitbox );

			pointScale = std::clamp( pointScale, 0.1f, 0.7f );
			radiusScaled = pHitbox->m_flRadius * pointScale;

			Vector vecBack{ vecCenter.x, pHitbox->bbmax.y - radiusScaled, vecCenter.z };
			AddPoint( pTarget, pHitbox, vecBack.Transform( pMatrix[ pHitbox->bone ] ), hitbox );
		}
		else if( hitbox == Hitboxes::HITBOX_STOMACH && bStomach ) {
			// apparently stomach hitbox is also fucked
			Vector vecRight{ pHitbox->bbmax.x, pHitbox->bbmax.y, pHitbox->bbmax.z + ( pHitbox->m_flRadius * 1.5f ) };
			AddPoint( pTarget, pHitbox, vecRight.Transform( pMatrix[ pHitbox->bone ] ), hitbox );

			Vector vecLeft{ pHitbox->bbmax.x, pHitbox->bbmax.y, pHitbox->bbmax.z - ( pHitbox->m_flRadius * 0.5f ) };
			AddPoint( pTarget, pHitbox, vecLeft.Transform( pMatrix[ pHitbox->bone ] ), hitbox );

			pointScale = std::clamp( pointScale, 0.1f, 0.7f );
			radiusScaled = pHitbox->m_flRadius * pointScale;

			Vector vecBack{ vecCenter.x, pHitbox->bbmax.y - radiusScaled, vecCenter.z };
			AddPoint( pTarget, pHitbox, vecBack.Transform( pMatrix[ pHitbox->bone ] ), hitbox );
		}
		else if( hitbox == Hitboxes::HITBOX_LOWER_CHEST || hitbox == Hitboxes::HITBOX_CHEST && bChest ) {
			pointScale = std::clamp( pointScale, 0.1f, 0.75f );
			radiusScaled = pHitbox->m_flRadius * pointScale;

			Vector vecBack{ vecCenter.x, pHitbox->bbmax.y - radiusScaled, vecCenter.z };
			AddPoint( pTarget, pHitbox, vecBack.Transform( pMatrix[ pHitbox->bone ] ), hitbox );
		}

		if( bLegs ) {
			if( hitbox == Hitboxes::HITBOX_RIGHT_THIGH || hitbox == Hitboxes::HITBOX_LEFT_THIGH ) {
				Vector vecHalfBottom{ pHitbox->bbmax.x - ( pHitbox->m_flRadius * ( 0.5f * pointScale ) ), pHitbox->bbmax.y, pHitbox->bbmax.z };
				AddPoint( pTarget, pHitbox, vecHalfBottom.Transform( pMatrix[ pHitbox->bone ] ), hitbox );
			}
			else if( hitbox == Hitboxes::HITBOX_RIGHT_CALF || hitbox == Hitboxes::HITBOX_LEFT_CALF ) {
				Vector vecHalfTop{ pHitbox->bbmax.x - ( pHitbox->m_flRadius * ( 0.5f * pointScale ) ), pHitbox->bbmax.y, pHitbox->bbmax.z };
				AddPoint( pTarget, pHitbox, vecHalfTop.Transform( pMatrix[ pHitbox->bone ] ), hitbox );
			}
		}
	}
	else {
		// feet multipoints (this shit is so fucked seriously)
		if( bFeet ) {
			if( hitbox == Hitboxes::HITBOX_RIGHT_FOOT || hitbox == Hitboxes::HITBOX_LEFT_FOOT ) {
				pointScale = std::clamp( pointScale, 0.1f, 0.80f );

				if( hitbox == Hitboxes::HITBOX_LEFT_FOOT ) {
					Vector vecHeel{ vecCenter.x + ( ( pHitbox->bbmax.x - vecCenter.x ) * pointScale ), vecCenter.y, vecCenter.z };
					AddPoint( pTarget, pHitbox, vecHeel.Transform( pMatrix[ pHitbox->bone ] ), hitbox );
				}

				if( hitbox == Hitboxes::HITBOX_RIGHT_FOOT ) {
					Vector vecHeel{ vecCenter.x + ( ( pHitbox->bbmin.x - vecCenter.x ) * pointScale ), vecCenter.y, vecCenter.z };
					AddPoint( pTarget, pHitbox, vecHeel.Transform( pMatrix[ pHitbox->bone ] ), hitbox );
				}
			}
		}
	}
}

bool Ragebot::AimAtPoint( AimPoint_t *pPoint ) {
	if( !pPoint ) {
		g_Visuals.AddDebugMessage( std::string( XorStr( __FUNCTION__ ) ).append( XorStr( " -> L" ) + std::to_string( __LINE__ ) ) );
		return false;
	}

	if( !pPoint->m_pTarget ) {
		g_Visuals.AddDebugMessage( std::string( XorStr( __FUNCTION__ ) ).append( XorStr( " -> L" ) + std::to_string( __LINE__ ) ) );
		return false;
	}

	if( !( m_AimbotInfo.m_pCmd->buttons & IN_USE ) ) {
		if( !g_pClientState->m_nChokedCommands( ) && g_Vars.rage.fake_lag_shot ) {
			g_Visuals.AddDebugMessage( std::string( XorStr( __FUNCTION__ ) ).append( XorStr( " -> L" ) + std::to_string( __LINE__ ) ) );
			return false;
		}
	}

	if( g_Vars.rage.delay_shot ) {
		bool bInUnduck = false;

		// we're ducking
		if( m_AimbotInfo.m_pLocal->m_flDuckAmount( ) > 0.f ) {
			// we're not fully ducked
			if( m_AimbotInfo.m_pLocal->m_flDuckAmount( ) < 1.0f ) {
				static float flPreviousDuck = m_AimbotInfo.m_pLocal->m_flDuckAmount( );

				// duck amount is changing, and current duck amount is lower than last duck amount
				if( flPreviousDuck != m_AimbotInfo.m_pLocal->m_flDuckAmount( ) ) {
					if( m_AimbotInfo.m_pLocal->m_flDuckAmount( ) < flPreviousDuck ) {
						bInUnduck = true;
						//g_pCVar->ConsoleColorPrintf( Color::White( ), "Unducking; %.2f - %.2f\n", flPreviousDuck, m_AimbotInfo.m_pLocal->m_flDuckAmount( ) );
					}
					flPreviousDuck = m_AimbotInfo.m_pLocal->m_flDuckAmount( );
				}
			}
		}

		if( bInUnduck ) {
			if( m_AimbotInfo.m_pLocal->m_flDuckAmount( ) >= 0.15f ) {
				//g_pCVar->ConsoleColorPrintf( Color::White( ), "Returning at; %.2f\n", m_AimbotInfo.m_pLocal->m_flDuckAmount( ) );
				return false;
			}
		}
	}

	// remove use flag from usercmd
	m_AimbotInfo.m_pCmd->buttons &= ~IN_USE;

	Vector delta = pPoint->m_vecPosition - m_AimbotInfo.m_vecEyePosition;
	delta.Normalize( );

	QAngle aimAngles;
	Math::VectorAngles( delta, aimAngles );

	if( g_Vars.rage.auto_fire ) {
		m_AimbotInfo.m_pCmd->buttons |= IN_ATTACK;
	}

	if( m_AimbotInfo.m_pCmd->buttons & IN_ATTACK ) {
		*m_AimbotInfo.m_pSendPacket = false;

		bool bShouldLagCompensate = true;

		if( g_Vars.rage.fakelag_correction == 1 && g_LagCompensation.BrokeTeleportDistance( pPoint->m_pTarget->m_pEntity->m_entIndex ) )
			bShouldLagCompensate = false;

		if( bShouldLagCompensate )
			m_AimbotInfo.m_pCmd->tick_count = TIME_TO_TICKS( pPoint->m_pTarget->m_pRecord.m_flSimulationTime ) + TIME_TO_TICKS( g_LagCompensation.GetLerp( ) );

		m_AimbotInfo.m_pCmd->viewangles = aimAngles;

		g_Vars.globals.m_bSendNextCommand = m_AimbotInfo.m_pWeapon->m_iItemDefinitionIndex( ) != WEAPON_REVOLVER;
	}

	if( !g_Vars.rage.silent_aim ) {
		g_pEngine->SetViewAngles( aimAngles );
	}

	return true;
}

void Ragebot::SetupEyePosition( ) {
	if( !m_AimbotInfo.m_pLocal )
		return;

	auto animState = m_AimbotInfo.m_pLocal->m_PlayerAnimState( );
	if( !animState )
		return;

	// backup animation shit
	const auto animStateBackup = *m_AimbotInfo.m_pLocal->m_PlayerAnimState( );
	float poses[ 20 ];
	C_AnimationLayer layers[ 13 ];
	matrix3x4_t matrix[ 128 ];
	std::memcpy( layers, m_AimbotInfo.m_pLocal->m_AnimOverlay( ).Base( ), sizeof( layers ) );
	std::memcpy( poses, m_AimbotInfo.m_pLocal->m_flPoseParameter( ), sizeof( poses ) );
	std::memcpy( matrix, m_AimbotInfo.m_pLocal->m_CachedBoneData( ).Base( ), m_AimbotInfo.m_pLocal->m_CachedBoneData( ).Count( ) * sizeof( matrix3x4_t ) );

	// allow reanimating
	if( animState->m_nLastUpdateFrame == g_pGlobalVars->framecount )
		animState->m_nLastUpdateFrame = g_pGlobalVars->framecount - 1;

	const float PitchPosBackup = *( float * )( uintptr_t( m_AimbotInfo.m_pLocal ) + Engine::Displacement.DT_BaseAnimating.m_flPoseParameter + 48 );
	*( float * )( uintptr_t( m_AimbotInfo.m_pLocal ) + Engine::Displacement.DT_BaseAnimating.m_flPoseParameter + 48 ) = 0.5f;

	m_AimbotInfo.m_pLocal->SetupBones( nullptr, 128, BONE_USED_BY_ANYTHING, m_AimbotInfo.m_pLocal->m_flSimulationTime( ) );

	*( float * )( uintptr_t( m_AimbotInfo.m_pLocal ) + Engine::Displacement.DT_BaseAnimating.m_flPoseParameter + 48 ) = PitchPosBackup;

	// setup eyepos.
	m_AimbotInfo.m_vecEyePosition = m_AimbotInfo.m_pLocal->GetEyePosition( );

	std::memcpy( m_AimbotInfo.m_pLocal->m_CachedBoneData( ).Base( ), matrix, m_AimbotInfo.m_pLocal->m_CachedBoneData( ).Count( ) * sizeof( matrix3x4_t ) );

	// restore animation shit
	std::memcpy( m_AimbotInfo.m_pLocal->m_AnimOverlay( ).Base( ), layers, sizeof( layers ) );
	std::memcpy( m_AimbotInfo.m_pLocal->m_flPoseParameter( ), poses, sizeof( poses ) );
	*m_AimbotInfo.m_pLocal->m_PlayerAnimState( ) = animStateBackup;
}

void Ragebot::CockRevolver( ) {
	if( !g_Vars.misc.auto_weapons )
		return;

	if( m_AimbotInfo.m_pWeapon->m_iItemDefinitionIndex( ) != WEAPON_REVOLVER )
		return;

	if( !( m_AimbotInfo.m_pCmd->buttons & IN_RELOAD ) && m_AimbotInfo.m_pWeapon->m_iClip1( ) ) {
		static float cockTime = 0.f;

		const float curtime = m_AimbotInfo.m_pLocal->m_nTickBase( ) * g_pGlobalVars->interval_per_tick;

		m_AimbotInfo.m_pCmd->buttons &= ~IN_ATTACK2;

		if( m_AimbotInfo.m_pLocal->CanShoot( true ) ) {
			if( cockTime <= curtime ) {
				if( m_AimbotInfo.m_pWeapon->m_flNextSecondaryAttack( ) <= curtime ) {
					cockTime = curtime + 0.234375f;
				}
				else {
					m_AimbotInfo.m_pCmd->buttons |= IN_ATTACK2;
				}
			}
			else {
				m_AimbotInfo.m_pCmd->buttons |= IN_ATTACK;
			}
		}
		else {
			cockTime = curtime + 0.234375f;
			m_AimbotInfo.m_pCmd->buttons &= ~IN_ATTACK;
		}
	}
}

std::string hitbox_to_string( int h ) {
	switch( h ) {
		case 0:
			return "head";
			break;
		case 1:
			return "neck";
			break;
		case HITBOX_RIGHT_FOOT:
			return "right foot";
			break;
		case HITBOX_LEFT_FOOT:
			return "left foot";
			break;
		case HITBOX_RIGHT_HAND:
			return "right hand";
			break;
		case HITBOX_LEFT_HAND:
			return "left hand";
			break;
		default:
			return "body";
			break;
	}
}

void Ragebot::PostAimbot( AimPoint_t *pPoint ) {
	if( !pPoint )
		return;

	if( !pPoint->m_pTarget )
		return;

	if( !pPoint->m_pTarget->m_pEntity )
		return;

	if( !pPoint->m_pTarget->m_pHitboxSet )
		return;

	if( !pPoint->m_pTarget->m_pRecord.player || pPoint->m_pTarget->m_pRecord.player == nullptr )
		return;

	// if we got here, let's increment the current shotid
	++m_AimbotInfo.m_iShotID;

	player_info_t info;
	if( g_pEngine->GetPlayerInfo( pPoint->m_pTarget->m_pEntity->EntIndex( ), &info ) ) {
		auto &resolverData = g_Resolver.m_arrResolverData.at( pPoint->m_pTarget->m_pEntity->EntIndex( ) );

		auto FixedStrLength = [ ] ( std::string str ) -> std::string {
			if( ( int )str[ 0 ] > 255 )
				return XorStr( "" );

			if( str.size( ) < 15 )
				return str;

			std::string result;
			for( size_t i = 0; i < 15u; i++ )
				result.push_back( str.at( i ) );
			return result;
		};

		auto HitgroupToString = [ ] ( int hitgroup ) -> std::string {
			switch( hitgroup ) {
				case Hitgroup_Generic:
					return XorStr( "generic" );
				case Hitgroup_Head:
					return XorStr( "head" );
				case Hitgroup_Chest:
					return XorStr( "chest" );
				case Hitgroup_Stomach:
					return XorStr( "stomach" );
				case Hitgroup_LeftArm:
					return XorStr( "left arm" );
				case Hitgroup_RightArm:
					return XorStr( "right arm" );
				case Hitgroup_LeftLeg:
					return XorStr( "left leg" );
				case Hitgroup_RightLeg:
					return XorStr( "right leg" );
				case Hitgroup_Neck:
					return XorStr( "neck" );
			}
			return XorStr( "generic" );
		};

		std::stringstream msg{ };

		std::vector<std::string> flags{ };
		if( pPoint->m_bPenetrated ) {
			flags.push_back( XorStr( "awall" ) );
		}
		if( pPoint->m_bBody ) {
			flags.push_back( XorStr( "body" ) );
		}
		if( pPoint->m_bCenter ) {
			flags.push_back( XorStr( "center" ) );
		}
		if( pPoint->m_bLethal ) {
			flags.push_back( XorStr( "lethal" ) );
		}
		if( pPoint->m_pTarget->m_pRecord.m_bIsResolved ) {
			flags.push_back( XorStr( "reslvd" ) );
		}
		if( resolverData.m_bUsedFreestandPreviously ) {
			flags.push_back( XorStr( "usedfs" ) );
		}
		if( resolverData.m_bSuppressDetectionResolvers ) {
			flags.push_back( XorStr( "supprde" ) );
		}
		if( resolverData.m_bHasHeightAdvantage ) {
			flags.push_back( XorStr( "hasha" ) );
		}
		if( resolverData.m_bSuppressHeightResolver ) {
			flags.push_back( XorStr( "supprha" ) );
		}
		if( g_LagCompensation.BrokeTeleportDistance( pPoint->m_pTarget->m_pEntity->m_entIndex ) ) {
			flags.push_back( XorStr( "blc" ) );
		}
		if( resolverData.m_bDesyncFlicking ) {
			flags.push_back( XorStr( "dfl" ) );
		}

		std::string buffer{ };
		if( !flags.empty( ) ) {
			for( auto n = 0; n < flags.size( ); ++n ) {
				auto &it = flags.at( n );

				buffer.append( it );
				if( n != flags.size( ) - 1 && flags.size( ) > 1 )
					buffer.append( XorStr( ":" ) );
			}
		}
		else {
			buffer = XorStr( "none" );
		}

		const int nHistoryTicks = TIME_TO_TICKS( pPoint->m_pTarget->m_pEntity->m_flSimulationTime( ) - pPoint->m_pTarget->m_pRecord.m_flSimulationTime );

		if( g_Vars.esp.target_capsules > 0 ) {
			if( pPoint->m_pTarget->m_pRecord.m_pMatrix )
				pPoint->m_pTarget->m_pEntity->DrawHitboxMatrix( pPoint->m_pTarget->m_pRecord.m_pMatrix,
																g_Vars.esp.target_capsules == 1 ? pPoint->m_pHitbox : nullptr );
		}
	
		auto hdr = g_pModelInfo->GetStudiomodel( pPoint->m_pTarget->m_pEntity->GetModel( ) );
		if( hdr ) {
			auto hitboxSet = hdr->pHitboxSet( pPoint->m_pTarget->m_pEntity->m_nHitboxSet( ) );
			if( hitboxSet ) {
				for( int i = 0; i < hitboxSet->numhitboxes; ++i ) {
					auto hitbox = hitboxSet->pHitbox( i );
					if( !hitbox )
						continue;

					if( hitbox->m_flRadius <= 0.f )
						continue;

					auto min = hitbox->bbmin.Transform( pPoint->m_pTarget->m_pRecord.m_pMatrix[ hitbox->bone ] );
					auto max = hitbox->bbmax.Transform( pPoint->m_pTarget->m_pRecord.m_pMatrix[ hitbox->bone ] );

					g_pDebugOverlay->AddCapsuleOverlay( min, max, hitbox->m_flRadius, 255, 255, 255, 255,
														g_pCVar->FindVar( XorStr( "sv_showlagcompensation_duration" ) )->GetFloat( ) );
				}
			}
		}
	
		ShotInformation_t shot;
		shot.m_iPredictedDamage = pPoint->m_iDamage;
		shot.m_flTime = g_pGlobalVars->realtime;
		shot.m_iTargetIndex = pPoint->m_pTarget->m_pEntity->EntIndex( );
		shot.m_bTapShot = g_TickbaseController.m_bTapShot;
		shot.m_vecStart = m_AimbotInfo.m_vecEyePosition;
		shot.m_pHitbox = pPoint->m_pHitbox;
		shot.m_pRecord = pPoint->m_pTarget->m_pRecord;
		shot.m_vecEnd = pPoint->m_vecPosition;
		shot.m_bWasLBYFlick = pPoint->m_pTarget->m_pRecord.m_bLBYFlicked;
		shot.m_bHadPredError = nHistoryTicks < 0; // happened a few times, always had a pred error then
		g_ShotHandling.RegisterShot( shot );

		std::string new_nick( info.szName ); new_nick.resize( 15 );

		C_AnimationLayer *layer_3 = &pPoint->m_pTarget->m_pEntity->m_AnimOverlay( )[ 3 ];
		if( layer_3 ) {
			int layer_3_activity = pPoint->m_pTarget->m_pEntity->GetSequenceActivity( layer_3->m_nSequence );

			const float flLatestMovePlayback = std::get<0>( resolverData.m_tLoggedPlaybackRates );
			const float flPreviousMovePlayback = std::get<1>( resolverData.m_tLoggedPlaybackRates );
			const float flPenultimateMovePlayback = std::get<2>( resolverData.m_tLoggedPlaybackRates );

			// fired shot at name in hitbox (hitbox id), r:[choked ticks|backtrack_ticks|onshot='s' lbyflick ='lby' fakewalk='fw' nothing='?'|velocity]:[inair='air' duck='duck' neither='?']
			char buf[ 512 ] = { };
			sprintf( buf,
					 // lol 
					 std::string( g_Vars.menu.polish_mode ? XorStr( "strzelono strzal | " ) : XorStr( "fired shot | " ) )
					 .append( "idx: %i, pl: %s, hb: %s (%i, %i dmg, %i mdmg), lag: %i, bt: %i, rfl: %s, vel: %.2f, pfl: %s, act: %i, res: %i, ms: %i, msl: %i, ey: %.1f, bdy: %.1f, afbd: %.1f, lmbd: %.1f, (%.2f, %.2f, %.2f) | %s" ).data( ),
					 pPoint->m_pTarget->m_pEntity->EntIndex( ),
					 new_nick.c_str( ),
					 hitbox_to_string( pPoint->m_iHitbox ).c_str( ),
					 pPoint->m_iHitbox,
					 pPoint->m_iDamage,
					 pPoint->m_iMinimalDamage,
					 pPoint->m_pTarget->m_pRecord.m_iLaggedTicks,
					 g_pGlobalVars->tickcount - TIME_TO_TICKS( pPoint->m_pTarget->m_pRecord.m_flSimulationTime ),
					 ( shot.m_bWasLBYFlick ? ( g_Vars.menu.polish_mode ? XorStr("trzepniecie") : XorStr("flick") ) : ( ( pPoint->m_pTarget->m_pRecord.m_bIsFakewalking ) != 0 ? ( g_Vars.menu.polish_mode ? XorStr("falszywy ruch") : XorStr("fake walk") ) : "?" ) ),
					 pPoint->m_pTarget->m_pRecord.m_vecVelocity.Length( ),
					 ( pPoint->m_pTarget->m_pRecord.m_iFlags & FL_ONGROUND ) == 0 ? ( g_Vars.menu.polish_mode ? XorStr( "powietrze" ) : XorStr( "air" ) ) : ( ( pPoint->m_pTarget->m_pRecord.m_iFlags & 6 ) != 0 ? g_Vars.menu.polish_mode ? XorStr( "kaczka" ) : XorStr("duck") : "?" ),
					 layer_3_activity,
					 resolverData.m_iCurrentResolverType,
					 resolverData.m_iMissedShots,
					 resolverData.m_iMissedShotsLBY,
					 pPoint->m_pTarget->m_pRecord.m_angEyeAngles.y,
					 pPoint->m_pTarget->m_pRecord.m_flLowerBodyYawTarget,
					 resolverData.m_flFreestandYaw != FLT_MAX ? fabsf( resolverData.m_flFreestandYaw - pPoint->m_pTarget->m_pRecord.m_flLowerBodyYawTarget ) : 0.f,
					 resolverData.m_flLastMoveBody != FLT_MAX ? fabsf( resolverData.m_flLastMoveBody - pPoint->m_pTarget->m_pRecord.m_flLowerBodyYawTarget ) : 0.f,
					 flLatestMovePlayback == FLT_MAX ? 6.9f : flLatestMovePlayback,
					 flPreviousMovePlayback == FLT_MAX ? 6.9f : flPreviousMovePlayback,
					 flPenultimateMovePlayback == FLT_MAX ? 6.9f : flPenultimateMovePlayback,
					 buffer.data( ) );

			g_EventLog.PushEvent( XorStr( "[dbg]" ), buf, Color( 255, 85, 85, 255 ), Color( 185, 185, 185, 255 ), 5.f, false );
		}
	}
}

bool Ragebot::RunInternal( ) {
	// early autostop here

	if( RunHitscan( ) ) {
		return FinishAimbot( );
	}

	g_Visuals.AddDebugMessage( std::string( XorStr( __FUNCTION__ ) ).append( XorStr( " -> L" ) + std::to_string( __LINE__ ) ) );
	return false;
}

bool Ragebot::Run( bool *bSendPacket, CUserCmd *pCmd ) {
	if( !g_Vars.rage.enabled || !g_Vars.rage.key.enabled ) {
		return false;
	}

	if( !g_Vars.globals.m_bInitRandomSeed ) {
		return false;
	}

	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return false;

	if( pLocal->IsDead( ) ) {
		return false;
	}

	auto pWeapon = reinterpret_cast< C_WeaponCSBaseGun * >( pLocal->m_hActiveWeapon( ).Get( ) );
	if( !pWeapon ) {
		return false;
	}

	auto pWeaponInfo = pWeapon->GetCSWeaponData( );
	if( !pWeaponInfo.IsValid( ) ) {
		return false;
	}

	if( pWeapon->m_iItemDefinitionIndex( ) != WEAPON_ZEUS ) {
		if( pWeaponInfo->m_iWeaponType == WEAPONTYPE_KNIFE || pWeaponInfo->m_iWeaponType == WEAPONTYPE_GRENADE || pWeaponInfo->m_iWeaponType == WEAPONTYPE_C4 ) {
			return false;
		}
	}

	// setup aimbot info variables
	m_AimbotInfo.m_pLocal = pLocal;
	m_AimbotInfo.m_pWeaponInfo = pWeaponInfo.Xor( );
	m_AimbotInfo.m_pWeapon = pWeapon;
	m_AimbotInfo.m_pCmd = pCmd;
	m_AimbotInfo.m_pSendPacket = bSendPacket;

	// setup eyepos
	SetupEyePosition( );

	m_AimbotInfo.m_pSettings = GetRageSettings( );
	if( !m_AimbotInfo.m_pSettings )
		return false;

	// cock the volverr
	CockRevolver( );

	// reset data from last tick
	m_AimbotInfo.m_bDidStop = false;

	if( !m_AimbotInfo.m_vecAimData.empty( ) )
		m_AimbotInfo.m_vecAimData.clear( );

	auto bRet = RunInternal( );

	if( g_Visuals.debug_messages.size( ) )
		g_Visuals.debug_messages_sane = std::move( g_Visuals.debug_messages );

	g_Visuals.debug_messages.clear( );
	return bRet;
}