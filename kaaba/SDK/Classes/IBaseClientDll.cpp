#include "../sdk.hpp"

ClientClass* IBaseClientDLL::GetAllClasses()
{
	using Fn = ClientClass * (__thiscall*)(void*);
	return Memory::VCall<Fn>(this, Index::IBaseClientDLL::GetAllClasses)(this);
}

bool IBaseClientDLL::IsChatRaised( ) {
	// engine.dll string xref "server_browser_dialog_open"
	// 2nd xref, gets u here : https://i.imgur.com/oVGSE2G.png
	// easy from there https://i.imgur.com/hqxthyg.png

   using Fn = bool( __thiscall* )( void* );
   return Memory::VCall<Fn>( this, 89 )( this );
}
