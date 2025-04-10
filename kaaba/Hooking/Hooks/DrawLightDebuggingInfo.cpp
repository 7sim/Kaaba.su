#include "../Hooked.hpp"

void __fastcall Hooked::DrawLightDebuggingInfo( void* ecx, void* edx ) {
	oDrawLightDebuggingInfo( ecx );
}
