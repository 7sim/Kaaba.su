#include "../Hooked.hpp"
#include "../../SDK/Classes/Player.hpp"

void __fastcall Hooked::ComputeShadowDepthTextures( void* ecx, void* edx, const CViewSetup& view, bool a4 ) {
	if( !a4 )
		return oComputeShadowDepthTextures( ecx, view, a4 );

	if( g_Vars.esp.disable_teammate_rendering ) {
		for( int i = 1; i <= g_pGlobalVars->maxClients; ++i ) {
			auto pEntity = ( C_CSPlayer* )g_pEntityList->GetClientEntity( i );

			if( !pEntity )
				continue;

			if( pEntity->EntIndex( ) == g_pEngine->GetLocalPlayer( ) )
				continue;

			if( pEntity->IsTeammate( C_CSPlayer::GetLocalPlayer( ) ) ) {
				// https://i.imgur.com/wjElvZB.png
				*( bool* )( ( uintptr_t )pEntity + 0x270 ) = false;

				auto wpn = pEntity->m_hActiveWeapon( ).Get( );
				if( wpn )
					*( bool* )( ( uintptr_t )wpn + 0x270 ) = false;
			}
		}
	}

	oComputeShadowDepthTextures( ecx, view, a4 );
}