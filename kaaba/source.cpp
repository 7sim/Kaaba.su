#include "source.hpp"

#include "Hooking/Hooked.hpp"
#include "Utils/InputSys.hpp"

#include "SDK/Classes/PropManager.hpp"
#include "SDK/Displacement.hpp"

#include "SDK/Classes/Player.hpp"

#include "Features/Miscellaneous/GameEvent.hpp"

#include "Renderer/Render.hpp"
#include "Features/Miscellaneous/SkinChanger.hpp"
#include "Features/Miscellaneous/KitParser.hpp"
#include "Features/Visuals/Glow.hpp"

#include "Hooking/hooker.hpp"

#include "Features/Visuals/Chams.hpp"

#include "Features/Rage/EnginePrediction.hpp"
#include "Loader/Exports.h"
#include "Renderer/Textures/fuckoff.h"
#include <fstream>

#include "Utils/LogSystem.hpp"

extern ClientClass* CCSPlayerClass;
extern CreateClientClassFn oCreateCCSPlayer;

bool __fastcall Hooked::CheckAchievementsEnabled( void* ecx, void* edx ) {
	return true;
}

using CHudElement__ShouldDrawFn = bool( __thiscall* )( void* );
CHudElement__ShouldDrawFn oCHudElement__ShouldDraw;

// client.dll E8 ? ? ? ? 84 C0 74 F3
bool __fastcall CHudElement__ShouldDraw( void* ecx, void* edx ) {
	return false;
}

using CHudScope__PaintFn = bool( __thiscall* )( void* );
CHudScope__PaintFn oCHudScope__Paint;

// client.dll 55 8B EC 83 E4 F8 83 EC 78 56 57 8B 3D
void __fastcall CHudScope__Paint( void* ecx, void* edx ) {

}

using ForceTonemapScaleFn = float( __thiscall * )( void * );
ForceTonemapScaleFn oForceTonemapScale;
float __fastcall ForceTonemapScaleGetFloat( void *ecx, void *edx ) {
	return 1.f;
}

using SVCMsg_VoiceData_t = void( __thiscall* )( void*, void* );
SVCMsg_VoiceData_t oSVCMsg_VoiceData;
void __fastcall SVCMsg_VoiceData( void* ecx, void*, void* msg ) {
	if( !msg )
		return;


}

Encrypted_t<IBaseClientDLL> g_pClient = nullptr;
Encrypted_t<IClientEntityList> g_pEntityList = nullptr;
Encrypted_t<IGameMovement> g_pGameMovement = nullptr;
Encrypted_t<IPrediction> g_pPrediction = nullptr;
Encrypted_t<IMoveHelper> g_pMoveHelper = nullptr;
Encrypted_t<IInput> g_pInput = nullptr;
Encrypted_t<CGlobalVars>  g_pGlobalVars = nullptr;
Encrypted_t<ISurface> g_pSurface = nullptr;
Encrypted_t<IVEngineClient> g_pEngine = nullptr;
Encrypted_t<IClientMode> g_pClientMode = nullptr;
Encrypted_t<ICVar> g_pCVar = nullptr;
Encrypted_t<IPanel> g_pPanel = nullptr;
Encrypted_t<IGameEventManager> g_pGameEvent = nullptr;
Encrypted_t<IVModelRender> g_pModelRender = nullptr;
Encrypted_t<IMaterialSystem> g_pMaterialSystem = nullptr;
Encrypted_t<ISteamClient> g_pSteamClient = nullptr;
Encrypted_t<ISteamGameCoordinator> g_pSteamGameCoordinator = nullptr;
Encrypted_t<ISteamMatchmaking> g_pSteamMatchmaking = nullptr;
Encrypted_t<ISteamUser> g_pSteamUser = nullptr;
Encrypted_t<ISteamFriends> g_pSteamFriends = nullptr;
Encrypted_t<IPhysicsSurfaceProps> g_pPhysSurface = nullptr;
Encrypted_t<IEngineTrace> g_pEngineTrace = nullptr;
Encrypted_t<CGlowObjectManager> g_pGlowObjManager = nullptr;
Encrypted_t<IVModelInfo> g_pModelInfo = nullptr;
Encrypted_t<CClientState>  g_pClientState = nullptr;
Encrypted_t<IVDebugOverlay> g_pDebugOverlay = nullptr;
Encrypted_t<IEngineSound> g_pEngineSound = nullptr;
Encrypted_t<IMemAlloc> g_pMemAlloc = nullptr;
Encrypted_t<IViewRenderBeams> g_pRenderBeams = nullptr;
Encrypted_t<ILocalize> g_pLocalize = nullptr;
Encrypted_t<IStudioRender> g_pStudioRender = nullptr;
Encrypted_t<ICenterPrint> g_pCenterPrint = nullptr;
Encrypted_t<IVRenderView> g_pRenderView = nullptr;
Encrypted_t<IClientLeafSystem> g_pClientLeafSystem = nullptr;
Encrypted_t<IMDLCache> g_pMDLCache = nullptr;
Encrypted_t<IEngineVGui> g_pEngineVGui = nullptr;
Encrypted_t<IViewRender> g_pViewRender = nullptr;
Encrypted_t<IInputSystem> g_pInputSystem = nullptr;
Encrypted_t<INetGraphPanel> g_pNetGraphPanel = nullptr;
Encrypted_t<CCSGameRules> g_pGameRules = nullptr;
Encrypted_t<CFontManager> g_pFontManager = nullptr;
Encrypted_t<CSPlayerResource*> g_pPlayerResource = nullptr;
Encrypted_t<IWeaponSystem> g_pWeaponSystem = nullptr;
Encrypted_t<SFHudDeathNoticeAndBotStatus> g_pDeathNotices = nullptr;
Encrypted_t<IClientShadowMgr> g_pClientShadowMgr = nullptr;
Encrypted_t<CHud> g_pHud = nullptr;

RecvPropHook::Shared m_pDidSmokeEffectSwap = nullptr;
RecvPropHook::Shared m_bClientSideAnimationSwap = nullptr;
RecvPropHook::Shared m_flVelocityModifierSwap = nullptr;

RecvPropHook::Shared m_bNightVisionOnSwap = nullptr;
RecvPropHook::Shared m_angEyeAnglesSwap = nullptr;

namespace Interfaces
{
	WNDPROC oldWindowProc;
	HWND hWindow = nullptr;

	void m_bDidSmokeEffect( const CRecvProxyData* pData, void* pStruct, void* pOut ) {
		m_pDidSmokeEffectSwap->GetOriginalFunction( )( pData, pStruct, pOut );

		if( g_Vars.esp.remove_smoke )
			*( uintptr_t* )( ( uintptr_t )pOut ) = true;
	}

	typedef bool( __thiscall* fnGetBool )( void* );
	fnGetBool oNetShowfragmentsGetBool;
	bool __fastcall net_showfragments_get_bool( void* pConVar, void* edx ) {
		if( !g_pEngine->IsInGame( ) || !g_pEngine->IsConnected( ) )
			return oNetShowfragmentsGetBool( pConVar );

		if( !g_Vars.misc.ping_spike_key.enabled || !g_Vars.misc.ping_spike )
			return oNetShowfragmentsGetBool( pConVar );

		auto pLocal = C_CSPlayer::GetLocalPlayer( );
		if( !pLocal )
			return oNetShowfragmentsGetBool( pConVar );

		auto netchannel = Encrypted_t<INetChannel>( g_pEngine->GetNetChannelInfo( ) );
		if( !netchannel.IsValid( ) )
			return oNetShowfragmentsGetBool( pConVar );

		static auto retReadSubChannelData = reinterpret_cast< uintptr_t* >( Memory::Scan( "engine.dll", "85 C0 74 12 53 FF 75 0C 68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 83 C4 0C" ) );
		static auto retCheckReceivingList = reinterpret_cast< uintptr_t* >( Memory::Scan( "engine.dll", "8B 1D ?? ? ?? ?? 85 C0 74 16 FF B6" ) );

		static uint32_t nLastFragment = 0;

		if( _ReturnAddress( ) == retReadSubChannelData && nLastFragment > 0 ) {
			const auto data = &reinterpret_cast< uint32_t* >( netchannel.Xor( ) )[ 0x56 ];

			if( data ) {
				if( reinterpret_cast< uint32_t* >( data )[ 0x43 ] == nLastFragment ) {
					auto& buffer = reinterpret_cast< uint32_t* >( data )[ 0x42 ];
					buffer = 0;
				}
			}
		}

		if( _ReturnAddress( ) == retCheckReceivingList ) {
			const auto data = &reinterpret_cast< uint32_t* >( netchannel.Xor( ) )[ 0x56 ];

			if( data )
				nLastFragment = reinterpret_cast< uint32_t* >( data )[ 0x43 ];
		}

		return oNetShowfragmentsGetBool( pConVar );
	}

	fnGetBool oSvCheatsGetBool;
	bool __fastcall sv_cheats_get_bool( void* pConVar, void* edx ) {
		static auto ret_ard = ( uintptr_t )Memory::Scan( "client.dll", "85 C0 75 30 38 86" );
		if( reinterpret_cast< uintptr_t >( _ReturnAddress( ) ) == ret_ard )
			return true;

		return oSvCheatsGetBool( pConVar );
	}

	bool Create( void* reserved ) {
		g_pClient = ( IBaseClientDLL* )CreateInterface( XorStr( "client.dll" ), XorStr( "VClient018" ) );
		if( !g_pClient.IsValid( ) ) {
			return false;
		}

		if( !Engine::g_PropManager.Create( g_pClient.Xor( ) ) ) {
			return false;
		}

		g_pEntityList = ( IClientEntityList* )CreateInterface( XorStr( "client.dll" ), XorStr( "VClientEntityList003" ) );
		if( !g_pEntityList.IsValid( ) ) {
			return false;
		}

		g_pGameMovement = ( IGameMovement* )CreateInterface( XorStr( "client.dll" ), XorStr( "GameMovement001" ) );
		if( !g_pGameMovement.IsValid( ) ) {
			return false;
		}

		g_pPrediction = ( IPrediction* )CreateInterface( XorStr( "client.dll" ), XorStr( "VClientPrediction001" ) );
		if( !g_pPrediction.IsValid( ) ) {
			return false;
		}

		g_pInput = *reinterpret_cast< IInput** > ( ( *reinterpret_cast< uintptr_t** >( g_pClient.Xor( ) ) )[ 15 ] + 0x1 );
		if( !g_pInput.IsValid( ) ) {
			return false;
		}

		g_pGlobalVars = **reinterpret_cast< CGlobalVars*** > ( ( *reinterpret_cast< uintptr_t** > ( g_pClient.Xor( ) ) )[ 0 ] + 0x1B );
		if( !g_pGlobalVars.IsValid( ) ) {
			return false;
		}

		g_pWeaponSystem = *( IWeaponSystem** )( Memory::Scan( XorStr( "client.dll" ), XorStr( "8B 35 ? ? ? ? FF 10 0F B7 C0" ) ) + 2 );
		if( !g_pWeaponSystem.IsValid( ) ) {
			return false;
		}

		g_pEngine = ( IVEngineClient* )CreateInterface( XorStr( "engine.dll" ), XorStr( "VEngineClient014" ) );
		if( !g_pEngine.IsValid( ) ) {
			return false;
		}

		g_pPanel = ( IPanel* )CreateInterface( XorStr( "vgui2.dll" ), XorStr( "VGUI_Panel009" ) );
		if( !g_pPanel.IsValid( ) ) {
			return false;
		}

		g_pSurface = ( ISurface* )CreateInterface( XorStr( "vguimatsurface.dll" ), XorStr( "VGUI_Surface031" ) );
		if( !g_pSurface.IsValid( ) ) {
			return false;
		}

		g_pClientMode = **( IClientMode*** )( ( *( DWORD** )g_pClient.Xor( ) )[ 10 ] + 0x5 );
		if( !g_pClientMode.IsValid( ) ) {
			return false;
		}

		g_pCVar = ( ICVar* )CreateInterface( XorStr( "vstdlib.dll" ), XorStr( "VEngineCvar007" ) );
		if( !g_pCVar.IsValid( ) ) {
			return false;
		}

		g_pGameEvent = ( IGameEventManager* )CreateInterface( XorStr( "engine.dll" ), XorStr( "GAMEEVENTSMANAGER002" ) );
		if( !g_pGameEvent.IsValid( ) ) {
			return false;
		}

		g_pModelRender = ( IVModelRender* )CreateInterface( XorStr( "engine.dll" ), XorStr( "VEngineModel016" ) );
		if( !g_pModelRender.IsValid( ) ) {
			return false;
		}

		g_pMaterialSystem = ( IMaterialSystem* )CreateInterface( XorStr( "materialsystem.dll" ), XorStr( "VMaterialSystem080" ) );
		if( !g_pMaterialSystem.IsValid( ) ) {
			return false;
		}

		g_pPhysSurface = ( IPhysicsSurfaceProps* )CreateInterface( XorStr( "vphysics.dll" ), XorStr( "VPhysicsSurfaceProps001" ) );
		if( !g_pPhysSurface.IsValid( ) ) {
			return false;
		}

		g_pEngineTrace = ( IEngineTrace* )CreateInterface( XorStr( "engine.dll" ), XorStr( "EngineTraceClient004" ) );
		if( !g_pEngineTrace.IsValid( ) ) {
			return false;
		}

		if( !Engine::CreateDisplacement( reserved ) ) {
			return false;
		}

		g_pMoveHelper = ( IMoveHelper* )( Engine::Displacement.Data.m_uMoveHelper );
		if( !g_pMoveHelper.IsValid( ) ) {
			return false;
		}

		g_pGlowObjManager = ( CGlowObjectManager* )Engine::Displacement.Data.m_uGlowObjectManager;
		if( !g_pGlowObjManager.IsValid( ) ) {
			return false;
		}

		g_pModelInfo = ( IVModelInfo* )CreateInterface( XorStr( "engine.dll" ), XorStr( "VModelInfoClient004" ) );
		if( !g_pModelInfo.IsValid( ) ) {
			return false;
		}

		// A1 FC BC 58 10  mov eax, g_ClientState
		g_pClientState = Encrypted_t<CClientState>( **( CClientState*** )( ( *( std::uintptr_t** )g_pEngine.Xor( ) )[ 14 ] + 0x1 ) );
		if( !g_pClientState.IsValid( ) ) {
			return false;
		}

		g_pDebugOverlay = ( IVDebugOverlay* )CreateInterface( XorStr( "engine.dll" ), XorStr( "VDebugOverlay004" ) );
		if( !g_pDebugOverlay.IsValid( ) ) {
			return false;
		}

		g_pMemAlloc = *( IMemAlloc** )( GetProcAddress( GetModuleHandle( XorStr( "tier0.dll" ) ), XorStr( "g_pMemAlloc" ) ) );
		if( !g_pMemAlloc.IsValid( ) ) {
			return false;
		}

		g_pEngineSound = ( IEngineSound* )CreateInterface( XorStr( "engine.dll" ), XorStr( "IEngineSoundClient003" ) );
		if( !g_pEngineSound.IsValid( ) ) {
			return false;
		}

		g_pRenderBeams = *( IViewRenderBeams** )( Engine::Displacement.Data.m_uRenderBeams );
		if( !g_pRenderBeams.IsValid( ) ) {
			return false;
		}

		g_pLocalize = ( ILocalize* )CreateInterface( XorStr( "localize.dll" ), XorStr( "Localize_001" ) );
		if( !g_pLocalize.IsValid( ) ) {
			return false;
		}

		g_pStudioRender = ( IStudioRender* )CreateInterface( XorStr( "studiorender.dll" ), XorStr( "VStudioRender026" ) );
		if( !g_pStudioRender.IsValid( ) ) {
			return false;
		}

		g_pCenterPrint = *( ICenterPrint** )( Engine::Displacement.Data.m_uCenterPrint );
		if( !g_pCenterPrint.IsValid( ) ) {
			return false;
		}

		g_pRenderView = ( IVRenderView* )CreateInterface( XorStr( "engine.dll" ), XorStr( "VEngineRenderView014" ) );
		if( !g_pRenderView.IsValid( ) ) {
			return false;
		}

		g_pClientLeafSystem = ( IClientLeafSystem* )CreateInterface( XorStr( "client.dll" ), XorStr( "ClientLeafSystem002" ) );
		if( !g_pClientLeafSystem.IsValid( ) ) {
			return false;
		}

		g_pMDLCache = ( IMDLCache* )CreateInterface( XorStr( "datacache.dll" ), XorStr( "MDLCache004" ) );
		if( !g_pMDLCache.IsValid( ) ) {
			return false;
		}

		g_pEngineVGui = ( IEngineVGui* )CreateInterface( XorStr( "engine.dll" ), XorStr( "VEngineVGui001" ) );
		if( !g_pEngineVGui.IsValid( ) ) {
			return false;
		}

		g_pInputSystem = ( IInputSystem* )CreateInterface( XorStr( "inputsystem.dll" ), XorStr( "InputSystemVersion001" ) );
		if( !g_pInputSystem.IsValid( ) ) {
			return false;
		}

		g_pViewRender = **( IViewRender*** )( Memory::Scan( XorStr( "client.dll" ), XorStr( "FF 50 4C 8B 06 8D 4D F4" ) ) - 6 );
		if( !g_pViewRender.IsValid( ) ) {
			return false;
		}

		g_pFontManager = *( CFontManager** )( Memory::Scan( XorStr( "vguimatsurface.dll" ), XorStr( "74 1D 8B 0D ?? ?? ?? ?? 68" ) ) + 0x4 );
		if( !g_pFontManager.IsValid( ) ) {
			return false;
		}

		g_pHud = *( CHud** )( Memory::Scan( XorStr( "client.dll" ), XorStr( "B9 ? ? ? ? E8 ? ? ? ? 8B 5D 08" ) ) + 1 );
		if( !g_pHud.IsValid( ) )
			return false;

		g_pClientShadowMgr = *( IClientShadowMgr** )( Memory::Scan( XorStr( "client.dll" ), XorStr( "A1 ? ? ? ? FF 90 ? ? ? ? 6A 00" ) ) + 1 );
		if( !g_pClientShadowMgr.IsValid( ) )
			return false;

		g_pDeathNotices = g_pHud->FindHudElement< SFHudDeathNoticeAndBotStatus* >( XorStr( "SFHudDeathNoticeAndBotStatus" ) );
		if( !g_pDeathNotices.IsValid( ) )
			return false;

		Hooked::CL_FireEvents = reinterpret_cast< Hooked::CL_FireEventsFn >( Memory::Scan( XorStr( "engine.dll" ), XorStr( "55 8B EC 83 EC 08 53 8B 1D ? ? ? ? 56 57 83 BB ? ? 00 00 06" ) ) );
		if( !Hooked::CL_FireEvents ) {
			return false;
		}

		auto D3DDevice9 = **( IDirect3DDevice9*** )Engine::Displacement.Data.m_D3DDevice;
		if( !D3DDevice9 ) {
			return false;
		}

		if( !g_InputSystem.Initialize( D3DDevice9 ) ) {
			return false;
		}

		auto pSteamAPI = GetModuleHandleA( XorStr( "steam_api.dll" ) );
		g_pSteamClient = ( ( ISteamClient * ( __cdecl* )( void ) )GetProcAddress( pSteamAPI, XorStr( "SteamClient" ) ) )( );
		if( !g_pSteamClient.IsValid( ) ) {
			return false;
		}

		HSteamUser hSteamUser = reinterpret_cast< HSteamUser( __cdecl* ) ( void ) >( GetProcAddress( pSteamAPI, XorStr( "SteamAPI_GetHSteamUser" ) ) )( );
		HSteamPipe hSteamPipe = reinterpret_cast< HSteamPipe( __cdecl* ) ( void ) >( GetProcAddress( pSteamAPI, XorStr( "SteamAPI_GetHSteamPipe" ) ) )( );
		g_pSteamGameCoordinator = ( ISteamGameCoordinator* )g_pSteamClient->GetISteamGenericInterface( hSteamUser, hSteamPipe, XorStr( "SteamGameCoordinator001" ) );
		if( !g_pSteamGameCoordinator.IsValid( ) ) {
			return false;
		}

		g_GameEvent.Register( );

		g_KitParser.initialize_kits( );
		//Render::DirectX::init( D3DDevice9 );
		g_SkinChanger.Create( );

		for( auto clientclass = g_pClient->GetAllClasses( );
			clientclass != nullptr;
			clientclass = clientclass->m_pNext ) 
		{
			if( !strcmp( clientclass->m_pNetworkName, XorStr( "CCSPlayer" ) ) ) {
				CCSPlayerClass = clientclass;
				oCreateCCSPlayer = ( CreateClientClassFn )clientclass->m_pCreateFn;

				clientclass->m_pCreateFn = Hooked::CreateCCSPlayer;
				continue;
			}

			if( !strcmp( clientclass->m_pNetworkName, XorStr( "CPlayerResource" ) ) ) {
				RecvTable* pClassTable = clientclass->m_pRecvTable;

				for( int nIndex = 0; nIndex < pClassTable->m_nProps; nIndex++ ) {
					RecvProp* pProp = &pClassTable->m_pProps[ nIndex ];

					if( !pProp || strcmp( pProp->m_pVarName, XorStr( "m_iTeam" ) ) )
						continue;

					g_pPlayerResource = Encrypted_t<CSPlayerResource*>( *reinterpret_cast< CSPlayerResource*** >( std::uintptr_t( pProp->m_pDataTable->m_pProps->m_ProxyFn ) + 0x10 ) );
					break;
				}

				continue;
			}
		}

		if( g_pEngine->IsInGame( ) ) {
			for( int i = 1; i <= g_pGlobalVars->maxClients; ++i ) {
				auto entity = C_CSPlayer::GetPlayerByIndex( i );
				if( !entity || !entity->IsPlayer( ) )
					continue;

				auto& new_hook = Hooked::player_hooks[ i ];
				new_hook.clientHook.Create( entity );
				new_hook.renderableHook.Create( ( void* )( ( uintptr_t )entity + 0x4 ) );
				new_hook.networkableHook.Create( ( void* )( ( uintptr_t )entity + 0x8 ) );
				new_hook.SetHooks( );
			}
		}

		// init config system
		g_Vars.Create( );

		// setup variable stuff
		g_Vars.viewmodel_fov->fnChangeCallback.m_Size = 0;
		g_Vars.viewmodel_offset_x->fnChangeCallback.m_Size = 0;
		g_Vars.viewmodel_offset_y->fnChangeCallback.m_Size = 0;
		g_Vars.viewmodel_offset_z->fnChangeCallback.m_Size = 0;

		g_Vars.mat_ambient_light_r->fnChangeCallback.m_Size = 0;
		g_Vars.mat_ambient_light_g->fnChangeCallback.m_Size = 0;
		g_Vars.mat_ambient_light_b->fnChangeCallback.m_Size = 0;

		g_Vars.name->fnChangeCallback.m_Size = 0;

		// setup proxy hooks
		RecvProp* prop = nullptr;

		//Engine::g_PropManager.GetProp( XorStr( "DT_CSPlayer" ), XorStr( "m_angEyeAngles[0]" ), &prop );
		//m_angEyeAnglesSwap = std::make_shared<RecvPropHook>( prop, &Hooked::m_angEyeAngles );

		//Engine::g_PropManager.GetProp( XorStr( "DT_SmokeGrenadeProjectile" ), XorStr( "m_bDidSmokeEffect" ), &prop );
		//m_pDidSmokeEffectSwap = std::make_shared<RecvPropHook>( prop, &Hooked::m_nSmokeEffectTickBegin );
		//
		//Engine::g_PropManager.GetProp( XorStr( "DT_BaseAnimating" ), XorStr( "bClientSideAnimation" ), &prop );
		//m_bClientSideAnimationSwap = std::make_shared<RecvPropHook>( prop, &Hooked::m_bClientSideAnimation );

		//Engine::g_PropManager.GetProp( XorStr( "DT_CSPlayer" ), XorStr( "m_flVelocityModifier" ), &prop );
		//m_flVelocityModifierSwap = std::make_shared<RecvPropHook>( prop, &Hooked::m_flVelocityModifier );

		//Engine::g_PropManager.GetProp( XorStr( "DT_CSPlayer" ), XorStr( "m_bNightVisionOn" ), &prop );
		//m_bNightVisionOnSwap = std::make_shared<RecvPropHook>( prop, &Hooked::m_bNightVisionOn );

		MH_Initialize( );

		//sv_cheats_get_bool
		oNetShowfragmentsGetBool = Hooked::HooksManager.HookVirtual<decltype( oNetShowfragmentsGetBool )>( g_pCVar->FindVar( "net_showfragments" ), &net_showfragments_get_bool, 13 );
		oSvCheatsGetBool = Hooked::HooksManager.HookVirtual<decltype( oSvCheatsGetBool )>( g_pCVar->FindVar( "sv_cheats" ), &sv_cheats_get_bool, 13 );	
	//	oForceTonemapScale = Hooked::HooksManager.HookVirtual<decltype( oForceTonemapScale )>( , &ForceTonemapScaleGetFloat, 13 );

		// vmt hooks
		//Hooked::oGetScreenAspectRatio = Hooked::HooksManager.HookVirtual<decltype( Hooked::oGetScreenAspectRatio )>( g_pEngine.Xor( ), &Hooked::GetScreenAspectRatio, Index::EngineClient::GetScreenAspectRatio );
		Hooked::oIsBoxVisible = Hooked::HooksManager.HookVirtual<decltype( Hooked::oIsBoxVisible )>( g_pEngine.Xor( ), &Hooked::IsBoxVisible, 32 );
		Hooked::oIsHltv = Hooked::HooksManager.HookVirtual<decltype( Hooked::oIsHltv )>( g_pEngine.Xor( ), &Hooked::IsHltv, Index::EngineClient::IsHltv );

		//Hooked::oDispatchUserMessage = Hooked::HooksManager.HookVirtual<decltype( Hooked::oDispatchUserMessage )>( g_pClient.Xor( ), &Hooked::DispatchUserMessage, 38 );
		Hooked::oFrameStageNotify = Hooked::HooksManager.HookVirtual<decltype( Hooked::oFrameStageNotify )>( g_pClient.Xor( ), &Hooked::FrameStageNotify, Index::IBaseClientDLL::FrameStageNotify );

		Hooked::oCreateMove = Hooked::HooksManager.HookVirtual<decltype( Hooked::oCreateMove )>( g_pClientMode.Xor( ), &Hooked::CreateMove, Index::CClientModeShared::CreateMove );
		Hooked::oDoPostScreenEffects = Hooked::HooksManager.HookVirtual<decltype( Hooked::oDoPostScreenEffects )>( g_pClientMode.Xor( ), &Hooked::DoPostScreenEffects, Index::CClientModeShared::DoPostScreenSpaceEffects );
		Hooked::oOverrideView = Hooked::HooksManager.HookVirtual<decltype( Hooked::oOverrideView )>( g_pClientMode.Xor( ), &Hooked::OverrideView, Index::CClientModeShared::OverrideView );

		Hooked::oDrawSetColor = Hooked::HooksManager.HookVirtual<decltype( Hooked::oDrawSetColor )>( g_pSurface.Xor( ), &Hooked::DrawSetColor, 15 );
		Hooked::oLockCursor = Hooked::HooksManager.HookVirtual<decltype( Hooked::oLockCursor )>( g_pSurface.Xor( ), &Hooked::LockCursor, Index::VguiSurface::LockCursor );

		Hooked::oPaintTraverse = Hooked::HooksManager.HookVirtual<decltype( Hooked::oPaintTraverse )>( g_pPanel.Xor( ), &Hooked::PaintTraverse, Index::IPanel::PaintTraverse );

		Hooked::oPaint = Hooked::HooksManager.HookVirtual<decltype( Hooked::oPaint )>( g_pEngineVGui.Xor( ), &Hooked::EngineVGUI_Paint, Index::IEngineVGui::Paint );

		Hooked::oBeginFrame = Hooked::HooksManager.HookVirtual<decltype( Hooked::oBeginFrame )>( g_pMaterialSystem.Xor( ), &Hooked::BeginFrame, Index::MatSystem::BeginFrame );
		Hooked::oOverrideConfig = Hooked::HooksManager.HookVirtual<decltype( Hooked::oOverrideConfig )>( g_pMaterialSystem.Xor( ), &Hooked::OverrideConfig, 21 );

		//Hooked::oEmitSound = Hooked::HooksManager.HookVirtual<decltype( Hooked::oEmitSound )>( g_pEngineSound.Xor( ), &Hooked::EmitSound, 5 );
		Hooked::oRunCommand = Hooked::HooksManager.HookVirtual<decltype( Hooked::oRunCommand )>( g_pPrediction.Xor( ), &Hooked::RunCommand, Index::IPrediction::RunCommand );

		Hooked::oProcessMovement = Hooked::HooksManager.HookVirtual<decltype( Hooked::oProcessMovement )>( g_pGameMovement.Xor( ), &Hooked::ProcessMovement, Index::IGameMovement::ProcessMovement );

		//Hooked::oClipRayCollideable = Hooked::HooksManager.HookVirtual<decltype( Hooked::oClipRayCollideable )>( g_pEngineTrace.Xor( ), &Hooked::ClipRayCollideable, 4 );
		//Hooked::oTraceRay = Hooked::HooksManager.HookVirtual<decltype( Hooked::oTraceRay )>( g_pEngineTrace.Xor( ), &Hooked::TraceRay, 5 );

		Hooked::oDrawModelExecute = Hooked::HooksManager.HookVirtual<decltype( Hooked::oDrawModelExecute )>( g_pModelRender.Xor( ), &Hooked::DrawModelExecute, Index::ModelDraw::DrawModelExecute );

		Hooked::oAddBoxOverlay = Hooked::HooksManager.HookVirtual<decltype( Hooked::oAddBoxOverlay )>( g_pDebugOverlay.Xor( ), &Hooked::AddBoxOverlay, 1 );

		Hooked::oFindMaterial = Hooked::HooksManager.HookVirtual<decltype( Hooked::oFindMaterial )>( g_pMaterialSystem.Xor(), &Hooked::FindMaterial, Index::MatSystem::FindMaterial );

		//Hooked::oRetrieveMessage = Hooked::HooksManager.HookVirtual<decltype( Hooked::oRetrieveMessage )>( g_pSteamGameCoordinator.Xor( ), &Hooked::RetrieveMessage, 2 );

		//Hooked::oWriteUsercmdDeltaToBuffer = Hooked::HooksManager.HookVirtual<decltype( Hooked::oWriteUsercmdDeltaToBuffer )>( g_pClient.Xor( ), &Hooked::WriteUsercmdDeltaToBuffer, 24 );

		// detours
		static auto CalcViewBobAddr = Memory::Scan( XorStr( "client.dll" ), XorStr( "55 8B EC A1 ? ? ? ? 83 EC 10 56 8B F1 B9" ) );
		Hooked::oCalcViewBob = Hooked::HooksManager.CreateHook<decltype( Hooked::oCalcViewBob ) >( &Hooked::CalcViewBob, ( void* )CalcViewBobAddr );

		static auto CheckAchievementsEnabledAddr = Memory::Scan( XorStr( "client.dll" ), XorStr( "A1 ? ? ? ? 56 8B F1 B9 ? ? ? ? FF 50 34 85 C0 0F 85" ) );
		Hooked::oCheckAchievementsEnabled = Hooked::HooksManager.CreateHook<decltype( Hooked::oCheckAchievementsEnabled ) >( &Hooked::CheckAchievementsEnabled, ( void* )CheckAchievementsEnabledAddr );

		static auto CalcRenderableWorldSpaceAABB_BloatedAddr = Memory::CallableFromRelative( Memory::Scan( XorStr( "client.dll" ), XorStr( "E8 ? ? ? ? F3 0F 10 47 ? 0F 2E 45 D8" ) ) );
		Hooked::oCalcRenderableWorldSpaceAABB_Bloated = Hooked::HooksManager.CreateHook<decltype( Hooked::oCalcRenderableWorldSpaceAABB_Bloated ) >(
			&Hooked::CalcRenderableWorldSpaceAABB_Bloated, ( void* )CalcRenderableWorldSpaceAABB_BloatedAddr );

		//auto ReportHitAddr = Memory::Scan( XorStr( "client.dll" ), XorStr( "55 8B EC 8B 55 08 83 EC 1C F6 42 1C 01" ) );
		//Hooked::oReportHit = Hooked::HooksManager.CreateHook<decltype( Hooked::oReportHit ) >( &Hooked::ReportHit, ( void* )ReportHitAddr );

		auto CL_MoveAddr = Memory::Scan( XorStr( "engine.dll" ), XorStr( "55 8B EC 81 EC ?? ?? ?? ?? 53 56 57 8B 3D ?? ?? ?? ?? 8A" ) );
		Hooked::oCL_Move = Hooked::HooksManager.CreateHook<decltype( Hooked::oCL_Move ) >( &Hooked::CL_Move, ( void* )CL_MoveAddr );

		auto C_BaseAnimating__DrawModelAddr = Memory::CallableFromRelative( Memory::Scan( XorStr( "client.dll" ), XorStr( "E8 ? ? ? ? 89 44 24 20 8B 06" ) ) );
		Hooked::oC_BaseAnimating__DrawModel = Hooked::HooksManager.CreateHook<decltype( Hooked::oC_BaseAnimating__DrawModel ) >( &Hooked::C_BaseAnimating__DrawModel, ( void* )C_BaseAnimating__DrawModelAddr );

		auto SendDatagramAddr = Memory::Scan( XorStr( "engine.dll" ), XorStr( "55 8B EC 83 E4 F0 B8 ? ? ? ? E8 ? ? ? ? 56 57 8B F9 89 7C 24 18" ) );
		Hooked::oSendDatagram = Hooked::HooksManager.CreateHook<decltype( Hooked::oSendDatagram ) >( &Hooked::SendDatagram, ( void* )SendDatagramAddr );

		// auto ProcessPacketAddr = Memory::Scan( XorStr( "engine.dll" ), XorStr( "55 8B EC 83 E4 C0 81 EC ? ? ? ? 53 56 57 8B 7D 08 8B D9" ) );;
		// Hooked::oProcessPacket = Hooked::HooksManager.CreateHook<decltype( Hooked::oProcessPacket ) >( &Hooked::ProcessPacket, ( void* )ProcessPacketAddr );

		Hooked::oModifyEyePosition = Hooked::HooksManager.CreateHook<decltype( Hooked::oModifyEyePosition ) >( &Hooked::ModifyEyePosition, ( void* )Engine::Displacement.Data.m_ModifyEyePos );

		//auto PhysicsSimulateAddr = Memory::CallableFromRelative( Memory::Scan( XorStr( "client.dll" ), XorStr( "E8 ? ? ? ? 80 BE ? ? ? ? ? 0F 84 ? ? ? ? 8B 06" ) ) );
		//Hooked::oPhysicsSimulate = Hooked::HooksManager.CreateHook<decltype( Hooked::oPhysicsSimulate ) >( &Hooked::PhysicsSimulate, ( void* )PhysicsSimulateAddr );

		//auto pFileSystem = **reinterpret_cast< void*** >( Memory::Scan( XorStr( "engine.dll" ), XorStr( "8B 0D ? ? ? ? 8D 95 ? ? ? ? 6A 00 C6" ) ) + 0x2 );
		//Hooked::oLooseFileAllowed = Hooked::HooksManager.HookVirtual<decltype( Hooked::oLooseFileAllowed )>( pFileSystem, &Hooked::LooseFileAllowed, 128 );

		//auto CheckFileCRCsWithServerAddr = reinterpret_cast< void* >( Memory::Scan( XorStr( "engine.dll" ), XorStr( "55 8B EC 81 EC ? ? ? ? 53 8B D9 89 5D F8 80" ) ) );
		//Hooked::oCheckFileCRCsWithServer = Hooked::HooksManager.CreateHook<decltype( Hooked::oCheckFileCRCsWithServer ) >( &Hooked::CheckFileCRCsWithServer, ( void* )CheckFileCRCsWithServerAddr );

		auto StandardBlendingRulesAddr = Memory::Scan( XorStr( "client.dll" ), XorStr( "55 8B EC 83 E4 F0 B8 ? ? ? ? E8 ? ? ? ? 56 8B 75 08 57 8B F9 85 F6" ) );
		Hooked::oStandardBlendingRules = Hooked::HooksManager.CreateHook<decltype( Hooked::oStandardBlendingRules ) >( &Hooked::StandardBlendingRules, ( void* )StandardBlendingRulesAddr );

		//auto BuildTransformationsAddr = Memory::Scan( XorStr( "client.dll" ), XorStr( "55 8B EC 83 E4 F0 81 EC ? ? ? ? 56 57 8B F9 8B 0D ? ? ? ? 89 7C 24 1C" ) );
		//Hooked::oBuildTransformations = Hooked::HooksManager.CreateHook<decltype( Hooked::oBuildTransformations ) >( &Hooked::BuildTransformations, ( void* )BuildTransformationsAddr );

		auto DoProceduralFootPlantAddr = Memory::Scan( XorStr( "client.dll" ), XorStr( "55 8B EC 83 E4 F0 83 EC 78 56 8B F1 57 8B 56" ) );
		Hooked::oDoProceduralFootPlant = Hooked::HooksManager.CreateHook<decltype( Hooked::oDoProceduralFootPlant )>( &Hooked::DoProceduralFootPlant, ( void* )DoProceduralFootPlantAddr );

		auto ShouldSkipAnimationFrame = ( Memory::CallableFromRelative( Memory::Scan( XorStr( "client.dll" ), XorStr( "E8 ? ? ? ? 88 44 24 0B" ) ) ) );
		Hooked::oShouldSkipAnimationFrame = Hooked::HooksManager.CreateHook<decltype( Hooked::oShouldSkipAnimationFrame )>( &Hooked::ShouldSkipAnimationFrame, ( void* )ShouldSkipAnimationFrame );

		auto IsUsingStaticPropDebugModeAddr = Memory::CallableFromRelative( Memory::Scan( XorStr( "engine.dll" ), XorStr( "E8 ?? ?? ?? ?? 84 C0 8B 45 08" ) ) );
		Hooked::oIsUsingStaticPropDebugMode = Hooked::HooksManager.CreateHook<decltype( Hooked::oIsUsingStaticPropDebugMode ) >( &Hooked::IsUsingStaticPropDebugMode, ( void* )IsUsingStaticPropDebugModeAddr );

		static auto UpdateClientSideAnimationAddr = Memory::Scan( XorStr( "client.dll" ), XorStr( "55 8B EC 51 56 8B F1 80 BE ? ? ? ? ? 74 36" ) );
		Hooked::oUpdateClientSideAnimation = Hooked::HooksManager.CreateHook<decltype( Hooked::oUpdateClientSideAnimation ) >( &Hooked::UpdateClientSideAnimation, ( void* )UpdateClientSideAnimationAddr );

		static auto CalcViewAddr = Memory::Scan( XorStr( "client.dll" ), XorStr( "55 8B EC 53 8B 5D 08 56 57 FF 75 18 8B F1" ) );
		Hooked::oCalcView = Hooked::HooksManager.CreateHook<decltype( Hooked::oCalcView ) >( &Hooked::CalcView, ( void* )CalcViewAddr );

		static auto DrawLightDebuggingInfoAddr = Memory::Scan( XorStr( "engine.dll" ), XorStr( "55 8B EC 83 E4 F8 8B 0D ? ? ? ? 81 EC ? ? ? ? 53 56 57 81 F9 ? ? ? ? 75 12" ) );
		Hooked::oDrawLightDebuggingInfo = Hooked::HooksManager.CreateHook<decltype( Hooked::oDrawLightDebuggingInfo ) >( &Hooked::DrawLightDebuggingInfo, ( void* )DrawLightDebuggingInfoAddr );

		//static auto VertexBufferLockAddr = Memory::Scan( XorStr( "shaderapidx9.dll" ), XorStr( "55 8B EC 83 E4 F8 81 EC ? ? ? ? 53 56 57" ) );
		//Hooked::oVertexBufferLock = Hooked::HooksManager.CreateHook<decltype( Hooked::oVertexBufferLock ) >( &Hooked::VertexBufferLock, ( void* )VertexBufferLockAddr );

		auto ClientStateAddr = ( void* )( uintptr_t( g_pClientState.Xor( ) ) + 0x8 );
		Hooked::oPacketStart = Hooked::HooksManager.HookVirtual<decltype( Hooked::oPacketStart )>( ClientStateAddr, &Hooked::PacketStart, 5 );
		//Hooked::oPacketEnd = Hooked::HooksManager.HookVirtual<decltype( Hooked::oPacketEnd )>( ClientStateAddr, &Hooked::PacketEnd, 6 );
		Hooked::oProcessTempEntities = Hooked::HooksManager.HookVirtual<decltype( Hooked::oProcessTempEntities )>( ClientStateAddr, &Hooked::ProcessTempEntities, 36 );;
		oSVCMsg_VoiceData = Hooked::HooksManager.HookVirtual<decltype( oSVCMsg_VoiceData )>( ClientStateAddr, &SVCMsg_VoiceData, 24 );

		Hooked::oSendNetMsg = Hooked::HooksManager.CreateHook<decltype( Hooked::oSendNetMsg ) >( &Hooked::SendNetMsg, ( void* )Engine::Displacement.Data.m_SendNetMsg );

		Hooked::oInterpolateServerEntities = Hooked::HooksManager.CreateHook<decltype( Hooked::oInterpolateServerEntities ) >( &Hooked::InterpolateServerEntities, ( void* )Engine::Displacement.Data.m_InterpolateServerEntities );
		//Hooked::oProcessInterpolatedList = Hooked::HooksManager.CreateHook<decltype( Hooked::oProcessInterpolatedList ) >( &Hooked::ProcessInterpolatedList, ( void* )Engine::Displacement.Data.m_ProcessInterpolatedList );

		Hooked::oPresent = Hooked::HooksManager.HookVirtual<decltype( Hooked::oPresent )>( D3DDevice9, &Hooked::Present, Index::DirectX::Present );
		Hooked::oReset = Hooked::HooksManager.HookVirtual<decltype( Hooked::oReset )>( D3DDevice9, &Hooked::Reset, Index::DirectX::Reset );

		Hooked::oComputeShadowDepthTextures = Hooked::HooksManager.HookVirtual<decltype( Hooked::oComputeShadowDepthTextures )>( g_pClientShadowMgr.Xor( ), &Hooked::ComputeShadowDepthTextures, 43 );

		//Hooked::oIsRenderableInPvs = Hooked::HooksManager.HookVirtual<decltype( Hooked::oIsRenderableInPvs ) >( g_pClientLeafSystem.Xor( ), &Hooked::IsRenderableInPVS, 8 );

		//static auto CHudElement__ShouldDrawaddr = Memory::CallableFromRelative( Memory::Scan( XorStr( "client.dll" ), XorStr( "E8 ? ? ? ? 84 C0 74 F3" ) ) );
		//oCHudElement__ShouldDraw = Hooked::HooksManager.CreateHook<decltype( oCHudElement__ShouldDraw )>( &CHudElement__ShouldDraw, ( void* )CHudElement__ShouldDrawaddr );

		//static auto CHudScope__Paintaddr = Memory::Scan( XorStr( "client.dll" ), XorStr( "55 8B EC 83 E4 F8 83 EC 78 56 57 8B 3D" ) );
		//oCHudScope__Paint = Hooked::HooksManager.CreateHook<decltype( oCHudScope__Paint )>( &CHudScope__Paint, ( void* )CHudScope__Paintaddr );

		Hooked::HooksManager.Enable( );
		//g_Vars.globals.menuOpen = true;

		return true;
	}

	void Destroy( ) {
		Hooked::HooksManager.Restore( );

		CCSPlayerClass->m_pCreateFn = oCreateCCSPlayer;
		Hooked::player_hooks.clear( );

		g_GameEvent.Shutdown( );
		g_SkinChanger.Destroy( );
		g_InputSystem.Destroy( );

		MH_Uninitialize( );

		g_pInputSystem->EnableInput( true );
	}

	void* CreateInterface( const std::string& image_name, const std::string& name ) {
#ifndef DEV
		char buf[ 128 ] = { 0 };
		wsprintfA( buf, XorStr( "%s::%s" ), image_name.data( ), name.data( ) );
		auto iface = interfaces_get_interface( loader_hash( buf ) );

		if( !iface )
			MessageBoxA( 0, buf, buf, 0 );

		return reinterpret_cast< void* >( iface );
#else
		auto image = GetModuleHandleA( image_name.c_str( ) );
		if( !image )
			return nullptr;

		auto fn = ( CreateInterfaceFn )( GetProcAddress( image, XorStr( "CreateInterface" ) ) );
		if( !fn )
			return nullptr;

		return fn( name.c_str( ), nullptr );
#endif
	}
}