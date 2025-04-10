#include "../../source.hpp"
#include "../Hooked.hpp"
#include "../../SDK/Displacement.hpp"
#include "../../SDK/Classes/Player.hpp"

void __fastcall Hooked::ModifyEyePosition( C_CSPlayer* ecx, void* edx, Vector* eye_position ) {
	g_Vars.globals.szLastHookCalled = XorStr( "40" );
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal ) {
		return oModifyEyePosition( ecx, eye_position );
	}

	if( ecx->EntIndex( ) == pLocal->EntIndex( ) ) {
		// note - michal;
		// the game will use our ModifyEyePosition
		// since we now cancel out the call in CalcView, 
		// we won't be getting any visual artifacts when the function
		// is called (but accuracy will remain improved, and in theory
		// it should not interfere with it whatsoever)
		return pLocal->ModifyEyePosition( eye_position );
	}

	return oModifyEyePosition( ecx, eye_position );
}