#include "../Hooked.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../Utils/InputSys.hpp"
#include "../../Features/Visuals/GrenadePrediction.hpp"
#include "../../SDK/Classes/PropManager.hpp"
#include "../../Features/Miscellaneous/Miscellaneous.hpp"

void __fastcall Hooked::OverrideView( void *ECX, int EDX, CViewSetup *vsView ) {
	g_Vars.globals.szLastHookCalled = XorStr( "18" );
	C_CSPlayer *local = C_CSPlayer::GetLocalPlayer( );

	bool bOk = g_Vars.globals.RenderIsReady && vsView && local != nullptr && g_pEngine->IsInGame( );

	if( bOk ) {
		if( !local->IsDead( ) ) {
			g_GrenadePrediction.View( );

			g_Misc.OverrideFOV( vsView );
		}
	}

	if( bOk )
		g_Misc.ThirdPerson( );

	oOverrideView( ECX, vsView );

	if( g_Vars.esp.remove_post_proccesing && bOk ) {
		if( local->m_bIsScoped( ) ) {
			//	vsView->m_iEdgeBlur = 0;
		}
	}
}
