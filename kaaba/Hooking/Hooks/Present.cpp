#include "../Hooked.hpp"
#include <intrin.h>
#include "../../Utils/InputSys.hpp"
#include "../../Features/Visuals/Hitmarker.hpp"
#include "../../Renderer/Render.hpp"
#include "../../SDK/Classes/entity.hpp"
#include "../../SDK/Classes/player.hpp"
#include "../../Utils/Config.hpp"
#include "../../Features/Rage/AntiAim.hpp"
#include <mutex>

std::once_flag once;
HRESULT __stdcall Hooked::Present( LPDIRECT3DDEVICE9 pDevice, const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion ) {
	std::call_once( once, [ & ] ( ) { Render::DirectX::initialize( pDevice );  } );

	g_Vars.globals.szLastHookCalled = XorStr( "27" );

	// gay idc
	InputHelper::Update( );

	IDirect3DStateBlock9 *state;
	pDevice->CreateStateBlock( D3DSBT_ALL, &state );

	Render::DirectX::begin( );
	{
		if( !g_Vars.globals.m_bChatOpen && !g_Vars.globals.m_bConsoleOpen ) {

			for( auto &keybind : g_keybinds ) {
				if( !keybind )
					continue;

				// hold
				if( keybind->cond == KeyBindType::HOLD ) {
					keybind->enabled = InputHelper::Down( keybind->key );
				}

				// toggle
				else if( keybind->cond == KeyBindType::TOGGLE ) {
					if( InputHelper::Pressed( keybind->key ) )
						keybind->enabled = !keybind->enabled;
				}

				// hold off
				else if( keybind->cond == KeyBindType::HOLD_OFF ) {
					keybind->enabled = !InputHelper::Down( keybind->key );
				}

				// always on
				else if( keybind->cond == KeyBindType::ALWAYS_ON ) {
					keybind->enabled = true;
				}

			}
		}
		else {
			for( auto &keybind : g_keybinds ) {
				if( !keybind )
					continue;

				if( keybind == &g_Vars.misc.third_person_bind )
					continue;

				// hold
				if( keybind->cond == KeyBindType::HOLD ) {
					keybind->enabled = false;
				}
			}
		}

		{
			auto pizda = g_Vars.menu.ascent.ToRegularColor( );
			size_t hash = size_t( ( uintptr_t )g_Vars.globals.menuOpen + ( uintptr_t )g_Vars.menu.size_x + ( uintptr_t )g_Vars.menu.size_y + uintptr_t( pizda.r( ) + pizda.g( ) + pizda.b( ) + pizda.a( ) ) );
			static size_t old_hash = hash;

			if( g_InputSystem.WasKeyPressed( g_Vars.menu.key.key ) ) {
				g_Vars.globals.menuOpen = !g_Vars.globals.menuOpen;
			}

			// let's save the global_data on open/close :)
			if( old_hash != hash ) {
				ConfigManager::SaveConfig( XorStr( "global_data" ), true );
				old_hash = hash;
			}

			GUI::ctx->animation = g_Vars.globals.menuOpen ? ( GUI::ctx->animation + ( 1.0f / 0.2f ) * g_pGlobalVars->frametime )
				: ( ( GUI::ctx->animation - ( 1.0f / 0.2f ) * g_pGlobalVars->frametime ) );

			GUI::ctx->animation = std::clamp<float>( GUI::ctx->animation, 0.f, 1.0f );

			// do menu fade the chad way
			// float alpha = g_pSurface->DrawGetAlphaMultiplier( );
			// g_pSurface->DrawSetAlphaMultiplier( GUI::ctx->animation );

			Menu::Draw( );

			alpha_mod = -1.f;
			g_Vars.menu.ascent.a = 1.f;

			// restore old alpha multiplier
			// g_pSurface->DrawSetAlphaMultiplier( alpha );
		}

		g_InputSystem.SetScrollMouse( 0.f );
	}
	// Render::DirectX::end( );

	state->Apply( );
	state->Release( );


	return oPresent( pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion );
}