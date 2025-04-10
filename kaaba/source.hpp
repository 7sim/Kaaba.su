#pragma once

// little include hack
#include "SDK/Valve/UtlBuffer.hpp"
#include "SDK/Valve/UtlMemory.hpp"
#include "SDK/Valve/UtlVector.hpp"

#include "SDK/sdk.hpp"
#include "SDK/Valve/recv_swap.hpp"
#include <windows.h>
#include <shlobj.h>

class IClientMode {
public:
   virtual ~IClientMode( ) { }
   virtual int ClientModeCSNormal( void* ) = 0;
   virtual void Init( ) = 0;
   virtual void InitViewport( ) = 0;
   virtual void Shutdown( ) = 0;
   virtual void Enable( ) = 0;
   virtual void Disable( ) = 0;
   virtual void Layout( ) = 0;
   virtual IPanel* GetViewport( ) = 0;
   virtual void* GetViewportAnimationController( ) = 0;
   virtual void ProcessInput( bool bActive ) = 0;
   virtual bool ShouldDrawDetailObjects( ) = 0;
   virtual bool ShouldDrawEntity( C_BaseEntity* pEnt ) = 0;
   virtual bool ShouldDrawLocalPlayer( C_BaseEntity* pPlayer ) = 0;
   virtual bool ShouldDrawParticles( ) = 0;
   virtual bool ShouldDrawFog( void ) = 0;
   virtual void OverrideView( CViewSetup* pSetup ) = 0;
   virtual int KeyInput( int down, int keynum, const char* pszCurrentBinding ) = 0;
   virtual void StartMessageMode( int iMessageModeType ) = 0;
   virtual IPanel* GetMessagePanel( ) = 0;
   virtual void OverrideMouseInput( float* x, float* y ) = 0;
   virtual bool CreateMove( float flInputSampleTime, void* usercmd ) = 0;
   virtual void LevelInit( const char* newmap ) = 0;
   virtual void LevelShutdown( void ) = 0;
};

extern Encrypted_t<IBaseClientDLL> g_pClient;
extern Encrypted_t<IClientEntityList> g_pEntityList;
extern Encrypted_t<IGameMovement> g_pGameMovement;
extern Encrypted_t<IPrediction> g_pPrediction;
extern Encrypted_t<IMoveHelper> g_pMoveHelper;
extern Encrypted_t<IInput> g_pInput;
extern Encrypted_t< CGlobalVars >  g_pGlobalVars;
extern Encrypted_t<ISurface> g_pSurface;
extern Encrypted_t<IVEngineClient> g_pEngine;
extern Encrypted_t<IClientMode> g_pClientMode;
extern Encrypted_t<ICVar> g_pCVar;
extern Encrypted_t<IPanel> g_pPanel;
extern Encrypted_t<IGameEventManager> g_pGameEvent;
extern Encrypted_t<IVModelRender> g_pModelRender;
extern Encrypted_t<IMaterialSystem> g_pMaterialSystem;
extern Encrypted_t<ISteamClient> g_pSteamClient;
extern Encrypted_t<ISteamGameCoordinator> g_pSteamGameCoordinator;
extern Encrypted_t<ISteamMatchmaking> g_pSteamMatchmaking;
extern Encrypted_t<ISteamUser> g_pSteamUser;
extern Encrypted_t<ISteamFriends> g_pSteamFriends;
extern Encrypted_t<IPhysicsSurfaceProps> g_pPhysSurface;
extern Encrypted_t<IEngineTrace> g_pEngineTrace;
extern Encrypted_t<CGlowObjectManager> g_pGlowObjManager;
extern Encrypted_t<IVModelInfo> g_pModelInfo;
extern Encrypted_t< CClientState >  g_pClientState;
extern Encrypted_t<IVDebugOverlay> g_pDebugOverlay;
extern Encrypted_t<IEngineSound> g_pEngineSound;
extern Encrypted_t<IMemAlloc> g_pMemAlloc;
extern Encrypted_t<IViewRenderBeams> g_pRenderBeams;
extern Encrypted_t<ILocalize> g_pLocalize;
extern Encrypted_t<IStudioRender> g_pStudioRender;
extern Encrypted_t<ICenterPrint> g_pCenterPrint;
extern Encrypted_t<IVRenderView> g_pRenderView;
extern Encrypted_t<IClientLeafSystem> g_pClientLeafSystem;
extern Encrypted_t<IMDLCache> g_pMDLCache;
extern Encrypted_t<IEngineVGui> g_pEngineVGui;
extern Encrypted_t<IViewRender> g_pViewRender;
extern Encrypted_t<IInputSystem> g_pInputSystem;
extern Encrypted_t<INetGraphPanel> g_pNetGraphPanel;
extern Encrypted_t<CCSGameRules> g_pGameRules;
extern Encrypted_t<CFontManager> g_pFontManager;
extern Encrypted_t<CSPlayerResource*> g_pPlayerResource;
extern Encrypted_t<IWeaponSystem> g_pWeaponSystem;
extern Encrypted_t<SFHudDeathNoticeAndBotStatus> g_pDeathNotices;
extern Encrypted_t<IClientShadowMgr> g_pClientShadowMgr;
extern Encrypted_t<CHud> g_pHud;

// netvar proxies
extern RecvPropHook::Shared m_pDidSmokeEffectSwap;
extern RecvPropHook::Shared m_pFlAbsYawSwap;
extern RecvPropHook::Shared m_pPlaybackRateSwap;
extern RecvPropHook::Shared m_bClientSideAnimationSwap;
extern RecvPropHook::Shared m_flVelocityModifierSwap;
extern RecvPropHook::Shared m_bNightVisionOnSwap;
extern RecvPropHook::Shared m_angEyeAnglesSwap;

namespace Interfaces
{
   extern WNDPROC oldWindowProc;
   extern HWND hWindow;

   bool Create( void* reserved );
   void Destroy( );

   void* CreateInterface( const std::string& image_name, const std::string& name );

}

__forceinline std::string GetDocumentsDirectory( ) {
    char my_documents[ MAX_PATH ];
    HRESULT result = SHGetFolderPath( NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, my_documents );

    return std::string( my_documents );
}