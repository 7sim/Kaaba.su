#include "../Hooked.hpp"
#include "../../Features/Rage/LagCompensation.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../SDK/Valve/CBaseHandle.hpp"
#include "../../Renderer/Render.hpp"
#include "../../Features/Visuals/GrenadePrediction.hpp"
#include "../../Features/Visuals/EventLogger.hpp"
#include "../../Features/Rage/TickbaseShift.hpp"
#include "../../Features/Scripting/Scripting.hpp"
#include "../../Utils/Config.hpp"
#include "../../Features/Visuals/Visuals.hpp"
#include "../../Utils/LogSystem.hpp"
#include "../../Features/Visuals/Hitmarker.hpp"
#include "../../Features/Miscellaneous/AutomaticPurchase.hpp"
#include <fstream>

static constexpr auto HzoomPanel = hash_32_fnv1a_const( "HudZoom" );

void __fastcall Hooked::PaintTraverse( void* ecx, void* edx, unsigned int vguiPanel, bool forceRepaint, bool allowForce ) {
	Render::Engine::Initialise( );

	g_Vars.globals.szLastHookCalled = XorStr( "21" );

	static unsigned int zoom{ };

	// cache CHudZoom panel once.
	if( !zoom && hash_32_fnv1a_const( g_pPanel->GetName( vguiPanel ) ) == HzoomPanel )
		zoom = vguiPanel;

	g_AutomaticPurchase.HandleRestrictedWeapon( );
	if( vguiPanel == zoom && g_Vars.esp.remove_scope )
		return;

	oPaintTraverse( ecx, vguiPanel, forceRepaint, allowForce );
}

void __stdcall Hooked::EngineVGUI_Paint( int mode )
{
	oPaint( g_pEngineVGui.Xor( ), mode );

	typedef void( __thiscall* StartDrawing_t )( void* );
	typedef void( __thiscall* FinishDrawing_t )( void* );

	static StartDrawing_t StartDrawFn = ( StartDrawing_t )Memory::Scan( XorStr( "vguimatsurface.dll" ), XorStr( "55 8B EC 83 E4 C0 83 EC 38" ) );
	static FinishDrawing_t EndDrawFn = ( FinishDrawing_t )Memory::Scan( XorStr( "vguimatsurface.dll" ), XorStr( "8B 0D ? ? ? ? 56 C6 05" ) );

	if( mode & 1 ) {
		StartDrawFn( g_pSurface.Xor( ) );

		// chat isn't open && console isn't open
		if( g_pClient.IsValid( ) && g_pEngine.IsValid( ) ) {
			g_Vars.globals.m_bConsoleOpen = g_pEngine->Con_IsVisible( );
			g_Vars.globals.m_bChatOpen = g_pClient->IsChatRaised( );
		}

		//draw here
		g_EventLog.Main( );
		g_GrenadePrediction.Paint( );

		g_Visuals.Draw( );
		g_Visuals.DrawWatermark( );

		g_Hitmarker.Draw( );

		// gotta handle this here now so hotkeys can bet set..

		EndDrawFn( g_pSurface.Xor( ) );
	}
}