#include "../Hooked.hpp"
#include "../../SDK/Displacement.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../Features/Rage/EnginePrediction.hpp"
#include <intrin.h>
#include "../../Features/Rage/Ragebot.hpp"
#include "../../Utils/InputSys.hpp"
#include "../../SDK/Classes/Exploits.hpp"
#include "../../Features/Rage/LagCompensation.hpp"
#include "../../Utils/Threading/threading.h"
#include "../../Features/Rage/Resolver.hpp"
#include <thread>
#include "../../Features/Visuals/GrenadePrediction.hpp"
#include "../../Features/Rage/TickbaseShift.hpp"
#include "../../Features/Visuals/EventLogger.hpp"
#include "../../Features/Miscellaneous/Movement.hpp"
#include "../../Features/Rage/ServerAnimations.hpp"
#include "../../Features/Rage/FakeLag.hpp"
#include "../../Features/Rage/AntiAim.hpp"

#include "../../Features/Miscellaneous/Miscellaneous.hpp"

namespace Hooked {
	bool CreateMoveHandler( float ft, CUserCmd *_cmd, bool *bSendPacket, bool *bFinalTick, bool original ) {
		g_Vars.globals.m_bInCreateMove = true;

		g_LagCompensation.SetupLerpTime( );

		auto pLocal = C_CSPlayer::GetLocalPlayer( );
		if( !pLocal || pLocal->IsDead( ) ) {
			g_Vars.globals.m_bShotAutopeek = false;
			g_Movement.m_vecAutoPeekPos.Init( );

			g_Vars.globals.m_bInCreateMove = false;
			return original;
		}

		auto pWeapon = reinterpret_cast< C_WeaponCSBaseGun * >( pLocal->m_hActiveWeapon( ).Get( ) );
		if( !pWeapon ) {
			g_Vars.globals.m_bInCreateMove = false;

			return original;
		}

		auto pWeaponData = pWeapon->GetCSWeaponData( );
		if( !pWeaponData.IsValid( ) ) {
			g_Vars.globals.m_bInCreateMove = false;

			return original;
		}

		Encrypted_t<CUserCmd> cmd( _cmd );
		if( !cmd.IsValid( ) )
			return original;

		if( g_Vars.globals.menuOpen ) {
			// just looks nicer
			auto RemoveButtons = [ & ] ( int key ) { cmd->buttons &= ~key; };
			RemoveButtons( IN_ATTACK );
			RemoveButtons( IN_ATTACK2 );
			RemoveButtons( IN_USE );

			if( GUI::ctx->typing ) {
				RemoveButtons( IN_MOVERIGHT );
				RemoveButtons( IN_MOVELEFT );
				RemoveButtons( IN_FORWARD );
				RemoveButtons( IN_BACK );
			}
		}

		if( g_Vars.misc.fastduck ) {
			cmd->buttons |= IN_BULLRUSH;
		}

		// store the original viewangles
		// fix movement with these later
		g_Movement.m_vecMovementAngles = cmd->viewangles;

		g_Misc.ForceCrosshair( );

		g_Prediction.PreThink( cmd.Xor( ) );
		g_Movement.PrePrediction( cmd.Xor( ), bSendPacket );

		g_Prediction.Think( cmd.Xor( ) );
		{
			g_LagCompensation.BackupOperation( );

			g_GrenadePrediction.Tick( cmd->buttons );

			if( g_Vars.misc.auto_weapons ) {
				if( pWeapon->m_iItemDefinitionIndex( ) != WEAPON_REVOLVER && pWeaponData->m_iWeaponType > WEAPONTYPE_KNIFE && pWeaponData->m_iWeaponType <= WEAPONTYPE_MACHINEGUN ) {
					if( !pLocal->CanShoot( ) ) {
						cmd->buttons &= ~IN_ATTACK;
					}
				}
			}

			g_Movement.InPrediction( cmd.Xor( ) );

			g_FakeLag.HandleFakeLag( bSendPacket, cmd.Xor( ) );

			if( g_Vars.globals.m_bSendNextCommand ) {
				// don't send 2 consecutive ticks in a row
				if( /*!*bSendPacket && */!g_Vars.globals.m_bFakeWalking )
					*bSendPacket = true;

				g_Vars.globals.m_bSendNextCommand = false;
			}

			g_Movement.LagWalk( cmd.Xor( ), bSendPacket );

			g_Ragebot.Run( bSendPacket, cmd.Xor( ) );

			g_AntiAim.Think( cmd.Xor( ), bSendPacket, bFinalTick );

			g_TickbaseController.RunExploits( bSendPacket, cmd.Xor( ) );

			if( cmd->buttons & IN_ATTACK
				&& pWeapon->m_iItemDefinitionIndex( ) != WEAPON_C4
				&& pWeaponData->m_iWeaponType >= WEAPONTYPE_KNIFE
				&& pWeaponData->m_iWeaponType <= WEAPONTYPE_MACHINEGUN
				&& pLocal->CanShoot( ) ) 			{
				g_TickbaseController.m_flLastExploitTime = g_pGlobalVars->realtime;

				if( pWeapon->m_iItemDefinitionIndex( ) != WEAPON_REVOLVER )
					g_Vars.globals.m_bShotWhileChoking = !( *bSendPacket );

				if( pWeaponData->m_iWeaponType != WEAPONTYPE_KNIFE )
					g_Vars.globals.m_bShotAutopeek = true;

				if( g_TickbaseController.m_bCMFix ) {
					if( g_TickbaseController.m_iFixAmount ) {
						g_TickbaseController.m_iFixCommand = cmd->command_number;
					}

					g_TickbaseController.m_bCMFix = false;
				}

				g_ServerAnimations.m_angChokedShotAngle = cmd->viewangles;

				cmd->viewangles -= pLocal->m_aimPunchAngle( ) * g_Vars.weapon_recoil_scale->GetFloat( );
			}

			g_Vars.globals.m_iWeaponIndex = pWeapon->m_iItemDefinitionIndex( );

			// fix animations after all movement related functions have been called
			g_ServerAnimations.HandleAnimations( bSendPacket, cmd.Xor( ) );
		}
		g_Prediction.PostThink( cmd.Xor( ), bSendPacket );
		g_LagCompensation.BackupOperation( true );

		g_Movement.PostPrediction( cmd.Xor( ) );

		// this is for resetting stuff
		// that e.g. was previously set to be run while not sending packet
		if( *bSendPacket ) {
			g_Vars.globals.m_bShotWhileChoking = false;

			g_FakeLag.m_iLastChokedCommands = g_pClientState->m_nChokedCommands( );

			g_ServerAnimations.m_uServerAnimations.m_bBreakingTeleportDst = ( pLocal->m_vecOrigin( ) - g_ServerAnimations.m_uServerAnimations.m_vecLastOrigin ).LengthSquared( ) > 4096.f;
			g_ServerAnimations.m_uServerAnimations.m_vecLastOrigin = pLocal->m_vecOrigin( );

			g_AntiAim.angRadarAngle = cmd->viewangles;
		}

		if( g_TickbaseController.m_bInShift ) {
			*bSendPacket = false;
		}

		cmd->viewangles.Clamp( );
		g_Vars.globals.m_bInCreateMove = false;
		return false;
	}

	bool __stdcall CreateMove( float ft, CUserCmd *_cmd ) {
		auto pLocal = C_CSPlayer::GetLocalPlayer( );

		g_Vars.globals.szLastHookCalled = XorStr( "2" );

		auto original_bs = oCreateMove( ft, _cmd );

		if( !_cmd || !_cmd->command_number || !pLocal )
			return original_bs;

		if( !pLocal )
			return original_bs;

		if( original_bs ) 		{
			g_pEngine->SetViewAngles( _cmd->viewangles );
			g_pPrediction->SetLocalViewAngles( _cmd->viewangles );
		}

		if( g_Vars.cl_csm_shadows && g_Vars.cl_csm_shadows->GetInt( ) != 0 )
			g_Vars.cl_csm_shadows->SetValue( 0 );

		if( g_Vars.engine_no_focus_sleep && g_Vars.engine_no_focus_sleep->GetInt( ) != 0 )
			g_Vars.engine_no_focus_sleep->SetValue( 0 );

		if( g_Vars.cl_extrapolate && g_Vars.cl_extrapolate->GetInt( ) != 0 )
			g_Vars.cl_extrapolate->SetValue( 0 );

		g_Misc.PreserveKillfeed( );

		Encrypted_t<uintptr_t> pAddrOfRetAddr( ( uintptr_t * )_AddressOfReturnAddress( ) );
		bool *bFinalTick = reinterpret_cast< bool * >( uintptr_t( pAddrOfRetAddr.Xor( ) ) + 0x15 );
		bool *bSendPacket = reinterpret_cast< bool * >( uintptr_t( pAddrOfRetAddr.Xor( ) ) + 0x14 );

		if( !bSendPacket || !bFinalTick ) {
			return original_bs;
		}

		if( !( *bSendPacket ) )
			*bSendPacket = true;

		auto result = CreateMoveHandler( ft, _cmd, bSendPacket, bFinalTick, original_bs );

		pLocal = C_CSPlayer::GetLocalPlayer( );
		if( !pLocal || !g_pEngine->IsInGame( ) ) {
			return original_bs;
		}

		return result;
	}

	bool __cdecl ReportHit( Hit_t *hit ) {
		return oReportHit( hit );
	}

	bool __cdecl IsUsingStaticPropDebugMode( ) {
		if( g_pEngine.IsValid( ) && !g_pEngine->IsInGame( ) )
			return oIsUsingStaticPropDebugMode( );

		// note - maxwell; i guess we're just going to return true here...
		// maybe this isn't a good idea because this does use a slower rendering
		// pipeline then the normal. but, i'm lazy and this gets transparent props to
		// work without night mode being enabled.
		return true;
	}

	void __vectorcall CL_Move( bool bFinalTick, float accumulated_extra_samples ) {
		g_TickbaseController.CL_Move( bFinalTick, accumulated_extra_samples );
	}
}
