#include "../../source.hpp"
#include "../Hooked.hpp"
#include "../../SDK/Displacement.hpp"
#include "../../SDK/Classes/Player.hpp"

void __fastcall Hooked::StandardBlendingRules( C_CSPlayer* pPlayer, uint32_t, CStudioHdr* hdr, Vector* pos, Quaternion* q, float currentTime, int boneMask ) {
	g_Vars.globals.szLastHookCalled = XorStr( "1337" );

	C_CSPlayer* pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pPlayer || !pLocal || !pPlayer->IsPlayer( ) ) {
		return oStandardBlendingRules( pPlayer, hdr, pos, q, currentTime, boneMask );;
	}

	// note - maxwell; fixes arms with weird guns.
	if( !pPlayer->IsTeammate( pLocal, false ) || pPlayer->IsLocalPlayer() )
		boneMask = BONE_USED_BY_SERVER;

	if( pPlayer->IsLocalPlayer() )
		boneMask |= BONE_USED_BY_BONE_MERGE;

	oStandardBlendingRules( pPlayer, hdr, pos, q, currentTime, boneMask );
}