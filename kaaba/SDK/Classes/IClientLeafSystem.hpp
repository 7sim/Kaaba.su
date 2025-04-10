#pragma once
#include "sdk.hpp"

class ClientRenderHandle_t;
class CClientLeafSubSystemData;
using RenderableModelType_t = int;
class SetupRenderInfo_t;
class ClientLeafShadowHandle_t;
class ClientShadowHandle_t;

class IClientLeafSystem {
public:
	//void CreateRenderableHandle( void* obj )
	//{
	//	typedef void( __thiscall* tOriginal )( void*, int, int, char, signed int, char );
	//	Memory::VCall<tOriginal>( this, 0 )( this, reinterpret_cast< uintptr_t >( obj ) + 0x4, 0, 0, 0xFFFFFFFF, 0xFFFFFFFF );
	//}
	virtual void CreateRenderableHandle( IClientRenderable* pRenderable, bool bRenderWithViewModels, RenderableTranslucencyType_t nType, RenderableModelType_t nModelType, UINT32 nSplitscreenEnabled = 0xFFFFFFFF ) = 0; // = RENDERABLE_MODEL_UNKNOWN_TYPE ) = 0;
};