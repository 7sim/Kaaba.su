#include "../Hooked.hpp"
#include "../../Features/Rage/EnginePrediction.hpp"
#include "../../SDK/Displacement.hpp"
#include "../../SDK/sdk.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../Features/Visuals/EventLogger.hpp"
#include "../../Features/Visuals/GrenadePrediction.hpp"
#include "../../Renderer/Render.hpp"
#include "../../Utils/InputSys.hpp"
#include "../../SDK/Classes/Exploits.hpp"
#include "../../Features/Rage/LagCompensation.hpp"
#include "../../Utils/Threading/threading.h"
#include "../../Features/Rage/AnimationSystem.hpp"
#include "../../Features/Rage/Resolver.hpp"
#include "../../Features/Rage/ShotHandling.hpp"
#include "../../Features/Rage/TickbaseShift.hpp"
#include "../../Features/Visuals/Chams.hpp"
#include "../../Features/Scripting/Scripting.hpp"
#include "../../Features/Rage/ServerAnimations.hpp"
#include "../../Features/Miscellaneous/Miscellaneous.hpp"
#include "../../Features/Miscellaneous/PlayerList.hpp"
#include "../../Features/Rage/AntiAim.hpp"

void draw_server_hitboxes( int index ) {
	auto get_player_by_index = [ ] ( int index ) -> C_CSPlayer * { //i dont need this shit func for anything else so it can be lambda
		typedef C_CSPlayer *( __fastcall *player_by_index )( int );
		static auto player_index = reinterpret_cast< player_by_index >( Memory::Scan( XorStr( "server.dll" ), XorStr( "85 C9 7E 2A A1" ) ) );

		if( !player_index )
			return false;

		return player_index( index );
	};

	static auto fn = Memory::Scan( XorStr( "server.dll" ), XorStr( "55 8B EC 81 EC ? ? ? ? 53 56 8B 35 ? ? ? ? 8B D9 57 8B CE" ) );
	auto duration = -1.f;
	PVOID entity = nullptr;

	entity = get_player_by_index( index );

	if( !entity )
		return;

	__asm {
		pushad
		movss xmm1, duration
		push 0 // 0 - colored, 1 - blue
		mov ecx, entity
		call fn
		popad
	}
}

void __fastcall Hooked::FrameStageNotify( void *ecx, void *edx, ClientFrameStage_t stage ) {
	g_Vars.globals.szLastHookCalled = XorStr( "9" );

	static auto extend_fakelag_packets = [ ] ( ) -> void {
		static bool noob = false;

		if( noob )
			return;

		if( !noob ) {
			static DWORD lol = Memory::Scan( XorStr( "engine.dll" ), XorStr( "B8 ? ? ? ? 3B F0 0F 4F F0 89 5D FC" ) ) + 1;

			DWORD old;
			VirtualProtect( ( LPVOID )lol, 1, PAGE_READWRITE, &old );
			*( int * )lol = 62;
			VirtualProtect( ( LPVOID )lol, 1, old, &old );
			noob = true;
		}
	};

	extend_fakelag_packets( );

	if( g_Vars.globals.m_bForceFullUpdate ) {
		g_pClientState->m_nDeltaTick( ) = -1;

		g_Vars.globals.m_bForceFullUpdate = false;

		return oFrameStageNotify( ecx, stage );
	}

	auto pLocal = C_CSPlayer::GetLocalPlayer( );

	static const auto ppGameRulesProxy = *reinterpret_cast< CCSGameRules *** >( Memory::Scan( XorStr( "client.dll" ), XorStr( "8B 0D ?? ?? ?? ?? 85 C0 74 0A 8B 01 FF 50 78 83 C0 54" ) ) + 2 );
	if( *ppGameRulesProxy ) {
		g_pGameRules = *ppGameRulesProxy;
	}

#if defined(LUA_SCRIPTING)
	if( !g_pEngine->IsDrawingLoadingImage( ) && pLocal ) {
		Scripting::Script::DoCallback( hash_32_fnv1a_const( "framestage" ), stage );
	}
#endif

	static auto net_earliertempents = g_pCVar->FindVar( XorStr( "net_earliertempents" ) );
	static auto cl_ignorepackets = g_pCVar->FindVar( XorStr( "cl_ignorepackets" ) );

	// https://github.com/perilouswithadollarsign/cstrike15_src/blob/f82112a2388b841d72cb62ca48ab1846dfcc11c8/engine/cl_main.cpp#L509-L517
	if( net_earliertempents && cl_ignorepackets ) {
		if( net_earliertempents->fnChangeCallback.m_Size != 0 ) {
			net_earliertempents->fnChangeCallback.m_Size = 0;
		}

		if( cl_ignorepackets->fnChangeCallback.m_Size != 0 ) {
			cl_ignorepackets->fnChangeCallback.m_Size = 0;
		}

		net_earliertempents->SetValueInt( 1 );
		cl_ignorepackets->SetValueInt( 0 );
	}

	// cache random values cuz valve random system cause performance issues
	if( !g_Vars.globals.m_bInitRandomSeed ) {
		for( auto i = 0; i <= 255; i++ ) {
			RandomSeed( i + 1 );

			g_Vars.globals.SpreadRandom[ i ].flRand1 = RandomFloat( 0.0f, 1.0f );
			g_Vars.globals.SpreadRandom[ i ].flRandPi1 = RandomFloat( 0.0f, 6.2831855f );
			g_Vars.globals.SpreadRandom[ i ].flRand2 = RandomFloat( 0.0f, 1.0f );
			g_Vars.globals.SpreadRandom[ i ].flRandPi2 = RandomFloat( 0.0f, 6.2831855f );
		}

		g_Vars.globals.m_bInitRandomSeed = true;
	}

	// modulation thing.
	static bool bPendingModulation = true;

	static bool bResetResolver = false;
	if( !pLocal || !g_pEngine->IsConnected( ) || g_pEngine->IsDrawingLoadingImage( ) ) {
		bPendingModulation = true;

		if( bResetResolver ) {
			for( int i = 1; i <= 64; ++i ) {
				g_Resolver.m_arrResolverData.at( i ).ResetLogicData( );

				if( i >= 64 )
					bResetResolver = false;
			}
		}
	}

	if( stage == FRAME_RENDER_START ) {
		static auto mat_force_tonemap_scale = g_pCVar->FindVar( XorStr( "mat_force_tonemap_scale" ) );
		if( mat_force_tonemap_scale->GetFloat( ) != 1.f ) {
			mat_force_tonemap_scale->SetValueFloat( 1.f );
		}
		g_PlayerList.UpdatePlayerList( );

		g_Misc.RemoveSmoke( );

		if( pLocal ) {
			g_Vars.globals.m_nLocalIndex = pLocal->EntIndex( );

			if( g_Vars.globals.m_nLocalSteamID == -2 && g_Vars.globals.m_nLocalIndex > -1 ) {
				player_info_t info;
				if( g_pEngine->GetPlayerInfo( g_Vars.globals.m_nLocalIndex, &info ) ) {
					g_Vars.globals.m_nLocalSteamID = info.steamID64;
				}
			}

		}

		//if( pLocal )
		//	if( g_pInput->m_fCameraInThirdPerson )
		//		draw_server_hitboxes( pLocal->EntIndex( ) );

		// modulation cache.
		float uNightModeHash =
			float( g_Vars.esp.wall_color.r + g_Vars.esp.wall_color.g + g_Vars.esp.wall_color.b +
				   g_Vars.esp.props_color.r + g_Vars.esp.props_color.g + g_Vars.esp.props_color.b +
				   float( g_Vars.esp.brightness_adjustment + g_Vars.esp.transparent_props + g_Vars.esp.transparent_walls +
						  g_Vars.esp.modulate_world + g_Vars.esp.modulate_props ) );

		static float uLastModeHash = uNightModeHash;

		if( bPendingModulation && g_pEngine->IsConnected( ) && !g_pEngine->IsDrawingLoadingImage( ) ) {
			bPendingModulation = false;

			g_Misc.Modulation( );
			g_Misc.SkyBoxChanger( );
		}

		if( uLastModeHash != uNightModeHash ) {
			uLastModeHash = uNightModeHash;

			g_Misc.Modulation( );
			g_Misc.SkyBoxChanger( );
		}

		static bool bSetClantag = false;

		static auto fnClantagChanged = ( int( __fastcall * )( const char *, const char * ) ) Engine::Displacement.Function.m_uClanTagChange;
		if( g_Vars.misc.clantag_changer ) {
			static int prev_time = ( int )g_pGlobalVars->curtime;

			if( prev_time != ( int )g_pGlobalVars->curtime ) {
				fnClantagChanged( XorStr( "kaaba.su" ), XorStr( "kaaba.su" ) );
				prev_time = ( int )g_pGlobalVars->curtime;
			}

			bSetClantag = true;
		}
		else {
			if( bSetClantag ) {
				fnClantagChanged( XorStr( "" ), XorStr( "" ) );
				bSetClantag = false;
			}
		}

		if( g_pEngine->IsConnected( ) ) {
			if( pLocal ) {
				bResetResolver = true;

				static bool bHasChanged = false;
				static bool bFlip = false;

				if( g_Misc.m_bHoldChatHostage ) {
					if( !bHasChanged ) {
						g_Vars.name->SetValue( "\x81 kaaba owns me and all" );
						bHasChanged = true;
					}

					g_Vars.voice_loopback->SetValue( bFlip );
					bFlip = !bFlip;
				}

				g_ShotHandling.ProcessShots( );

				if( g_Vars.esp.remove_smoke )
					*( int * )Engine::Displacement.Data.m_uSmokeCount = 0;

				auto aim_punch = pLocal->m_aimPunchAngle( ) * g_Vars.weapon_recoil_scale->GetFloat( ) * g_Vars.view_recoil_tracking->GetFloat( );
				if( g_Vars.esp.remove_recoil_shake )
					pLocal->pl( ).v_angle -= pLocal->m_viewPunchAngle( );

				if( g_Vars.esp.remove_recoil_punch )
					pLocal->pl( ).v_angle -= aim_punch;

				pLocal->pl( ).v_angle.Normalize( );

				if( g_Vars.esp.remove_flash ) {
					pLocal->m_flFlashDuration( ) = 0.f;
				}
			}
		}
	}

	if( stage == FRAME_RENDER_END ) {
		if( pLocal ) {
			g_Vars.globals.RenderIsReady = true;
		}
	}

	g_Prediction.CorrectViewModel( stage );

	g_Prediction.m_nLastFrameStage = stage;

	oFrameStageNotify( ecx, stage );

	if( pLocal && !pLocal->IsDead( ) ) {
		pLocal->pl( ).v_angle = g_AntiAim.angRadarAngle;
	}

	if( stage == FRAME_NET_UPDATE_END ) {
		g_Prediction.ApplyCompressionNetvars( );

		if( g_pEngine->IsConnected( ) ) {
			g_AnimationSystem.FrameStage( );

			// finish up the rest of the queued bonesetups
			// we need this data immediately, always
			// Threading::FinishQueue( true );

			g_LagCompensation.Update( );
		}

		Hooked::CL_FireEvents( );
	}
}