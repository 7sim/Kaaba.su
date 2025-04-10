#include "Miscellaneous.hpp"
#include "../../SDK/variables.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../Hooking/Hooked.hpp"
#include "../../SDK/Displacement.hpp"
#include "../Rage/LagCompensation.hpp"

Miscellaneous g_Misc;

void Miscellaneous::ThirdPerson( ) {
	C_CSPlayer* pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	// for whatever reason overrideview also gets called from the main menu.
	if( !g_pEngine->IsInGame( ) )
		return;

	// no need to do thirdperson
	if( !g_Vars.misc.third_person ) {
		return;
	}

	// reset this before doing anything
	g_Misc.m_flThirdpersonTransparency = 1.f;

	// check if we have a local player and he is alive.
	const bool bAlive = !pLocal->IsDead( );
	if( bAlive ) {
		C_WeaponCSBaseGun* pWeapon = ( C_WeaponCSBaseGun* )pLocal->m_hActiveWeapon( ).Get( );

		if( !pWeapon )
			return;

		auto pWeaponData = pWeapon->GetCSWeaponData( );
		if( !pWeaponData.IsValid( ) )
			return;

		if( pWeaponData->m_iWeaponType == WEAPONTYPE_GRENADE || pLocal->m_bIsScoped( ) ) {
			g_Misc.m_flThirdpersonTransparency = 0.45f;
		}
	}


	// camera should be in thirdperson.
	if( g_Vars.misc.third_person_bind.enabled || ( g_Vars.misc.first_person_dead && !bAlive ) ) {
		// if alive and not in thirdperson already switch to thirdperson.
		if( bAlive && !g_pInput->CAM_IsThirdPerson( ) )
			g_pInput->CAM_ToThirdPerson( );

		// if dead and spectating in firstperson switch to thirdperson.
		else if( pLocal->m_iObserverMode( ) == 4 ) {
			if( g_Vars.misc.first_person_dead ) {
				// if in thirdperson, switch to firstperson.
				// we need to disable thirdperson to spectate properly.
				if( g_pInput->CAM_IsThirdPerson( ) ) {
					g_pInput->CAM_ToFirstPerson( );
				}

				pLocal->m_iObserverMode( ) = 5;
			}
		}
	}

	// camera should be in firstperson.
	else if( g_pInput->CAM_IsThirdPerson( ) ) {
		g_pInput->CAM_ToFirstPerson( );
	}

	// if after all of this we are still in thirdperson.
	if( g_pInput->CAM_IsThirdPerson( ) ) {
		// get camera angles.
		QAngle offset;
		g_pEngine->GetViewAngles( offset );

		// get our viewangle's forward directional vector.
		Vector forward;
		Math::AngleVectors( offset, forward );

		// setup thirdperson distance
		offset.z = g_pCVar->FindVar( XorStr( "cam_idealdist" ) )->GetFloat( );

		// start pos.
		const Vector origin = pLocal->GetEyePosition( true );

		// setup trace filter and trace.
		CTraceFilterWorldAndPropsOnly filter;
		CGameTrace tr;

		g_pEngineTrace->TraceRay(
			Ray_t( origin, origin - ( forward * offset.z ), { -16.f, -16.f, -16.f }, { 16.f, 16.f, 16.f } ),
			MASK_NPCWORLDSTATIC,
			( ITraceFilter* )&filter,
			&tr
		);

		g_pInput->m_fCameraInThirdPerson = true;

		g_pInput->m_vecCameraOffset = { offset.x, offset.y, offset.z * tr.fraction };

		pLocal->UpdateVisibilityAllEntities( );
	}
}

void Miscellaneous::Modulation( ) {
	std::vector< IMaterial* > world = { }, props = { };

	const float prop_alpha = g_Vars.esp.transparent_props >= 99.f ? 1.f : g_Vars.esp.transparent_props / 100.f;
	const float wall_alpha = g_Vars.esp.transparent_walls >= 99.f ? 1.f : g_Vars.esp.transparent_walls / 100.f;

	// iterate material handles.
	for( uint16_t h{ g_pMaterialSystem->FirstMaterial( ) }; h != g_pMaterialSystem->InvalidMaterial( ); h = g_pMaterialSystem->NextMaterial( h ) ) {
		// get material from handle.
		IMaterial* mat = g_pMaterialSystem->GetMaterial( h );

		if( !mat || mat->IsErrorMaterial( ) )
			continue;

		// store world materials.
		if( hash_32_fnv1a( mat->GetTextureGroupName( ) ) == hash_32_fnv1a( "World textures" ) )
			world.push_back( mat );

		// store props.
		else if( hash_32_fnv1a( mat->GetTextureGroupName( ) ) == hash_32_fnv1a( "StaticProp textures" ) )
			props.push_back( mat );
	}

	// modulate world
	if( g_Vars.esp.modulate_world ) {
		for( const auto& w : world )
			w->ColorModulate( g_Vars.esp.wall_color.r, g_Vars.esp.wall_color.g, g_Vars.esp.wall_color.b );
	}
	else {
		for( const auto &w : world )
			w->ColorModulate( 1.f, 1.f, 1.f );
	}

	// modulate props
	if( g_Vars.esp.modulate_props ) {
		for( const auto &p : props )
			p->ColorModulate( g_Vars.esp.props_color.r, g_Vars.esp.props_color.g, g_Vars.esp.props_color.b );
	}
	else {
		for( const auto& p : props ) 
			p->ColorModulate( 1.f, 1.f, 1.f );
	}

	// transgender walls
	for( const auto& w : world ) {
		w->AlphaModulate( wall_alpha );
	}

	for( const auto& p : props ) {
		p->AlphaModulate( prop_alpha );
	}
}

void Miscellaneous::SkyBoxChanger( ) {
	static auto fnLoadNamedSkys = ( void( __fastcall* )( const char* ) )Engine::Displacement.Function.m_uLoadNamedSkys;
	static ConVar* sv_skyname = g_pCVar->FindVar( XorStr( "sv_skyname" ) );

	if( g_Vars.esp.modulate_world )
		fnLoadNamedSkys( XorStr( "sky_csgo_night02" ) );
	else 
		fnLoadNamedSkys( sv_skyname->GetString( ) );
}

void Miscellaneous::PreserveKillfeed( ) {
	auto local = C_CSPlayer::GetLocalPlayer( );

	if( !local || !g_pEngine->IsInGame( ) || !g_pEngine->IsConnected( ) ) {
		return;
	}

	static auto bStatus = false;
	static float m_flSpawnTime = local->m_flSpawnTime( );

	auto bSet = false;
	if( m_flSpawnTime != local->m_flSpawnTime( ) || bStatus != g_Vars.esp.preserve_killfeed ) {
		bSet = true;
		bStatus = g_Vars.esp.preserve_killfeed;
		m_flSpawnTime = local->m_flSpawnTime( );
	}

	for( int i = 0; i < g_pDeathNotices->m_vecDeathNotices.Count( ); i++ ) {
		auto cur = &g_pDeathNotices->m_vecDeathNotices[ i ];
		if( !cur ) {
			continue;
		}

		if( local->IsDead( ) || bSet ) {
			if( cur->set != 1.f && !bSet ) {
				continue;
			}

			cur->m_flStartTime = g_pGlobalVars->curtime;
			cur->m_flStartTime -= local->m_iHealth( ) <= 0 ? 2.f : 7.5f;
			cur->set = 2.f;

			continue;
		}

		if( cur->set == 2.f ) {
			continue;
		}

		if( !bStatus ) {
			cur->set = 1.f;
			return;
		}

		if( cur->set == 1.f ) {
			continue;
		}

		if( cur->m_flLifeTimeModifier == 1.5f ) {
			cur->m_flStartTime = FLT_MAX;
		}

		cur->set = 1.f;
	}
}

void Miscellaneous::RemoveSmoke( ) {
	if( !g_pEngine->IsInGame( ) )
		return;

	static uint32_t* pSmokeCount = nullptr;
	if( !pSmokeCount ) {
		pSmokeCount = *reinterpret_cast< uint32_t** > ( Memory::Scan( XorStr( "client.dll" ), XorStr( "A3 ? ? ? ? 57 8B CB" ) ) + 0x1 );
	}

	if( g_Vars.esp.remove_smoke ) {
		*pSmokeCount = 0;
	}

	static IMaterial* pSmokeGrenade = nullptr;
	if( !pSmokeGrenade ) {
		pSmokeGrenade = g_pMaterialSystem->FindMaterial( XorStr( "particle/vistasmokev1/vistasmokev1_smokegrenade" ), nullptr );
	}

	static IMaterial* pSmokeFire = nullptr;
	if( !pSmokeFire ) {
		pSmokeFire = g_pMaterialSystem->FindMaterial( XorStr( "particle/vistasmokev1/vistasmokev1_fire" ), nullptr );
	}

	static IMaterial* pSmokeDust = nullptr;
	if( !pSmokeDust ) {
		pSmokeDust = g_pMaterialSystem->FindMaterial( XorStr( "particle/vistasmokev1/vistasmokev1_emods_impactdust" ), nullptr );
	}

	static IMaterial* pSmokeMods = nullptr;
	if( !pSmokeMods ) {
		pSmokeMods = g_pMaterialSystem->FindMaterial( XorStr( "particle/vistasmokev1/vistasmokev1_emods" ), nullptr );
	}

	if( !pSmokeGrenade || !pSmokeFire || !pSmokeDust || !pSmokeMods ) {
		return;
	}

	pSmokeGrenade->SetMaterialVarFlag( MATERIAL_VAR_NO_DRAW, g_Vars.esp.remove_smoke );
	pSmokeGrenade->IncrementReferenceCount( );

	pSmokeFire->SetMaterialVarFlag( MATERIAL_VAR_NO_DRAW, g_Vars.esp.remove_smoke );
	pSmokeFire->IncrementReferenceCount( );

	pSmokeDust->SetMaterialVarFlag( MATERIAL_VAR_NO_DRAW, g_Vars.esp.remove_smoke );
	pSmokeDust->IncrementReferenceCount( );

	pSmokeMods->SetMaterialVarFlag( MATERIAL_VAR_NO_DRAW, g_Vars.esp.remove_smoke );
	pSmokeMods->IncrementReferenceCount( );

	if( !g_Vars.esp.remove_post_proccesing )
		return;

	enum PostProcessParameterNames_t {
		PPPN_FADE_TIME = 0,
		PPPN_LOCAL_CONTRAST_STRENGTH,
		PPPN_LOCAL_CONTRAST_EDGE_STRENGTH,
		PPPN_VIGNETTE_START,
		PPPN_VIGNETTE_END,
		PPPN_VIGNETTE_BLUR_STRENGTH,
		PPPN_FADE_TO_BLACK_STRENGTH,
		PPPN_DEPTH_BLUR_FOCAL_DISTANCE,
		PPPN_DEPTH_BLUR_STRENGTH,
		PPPN_SCREEN_BLUR_STRENGTH,
		PPPN_FILM_GRAIN_STRENGTH,

		POST_PROCESS_PARAMETER_COUNT
	};

	struct PostProcessParameters_t {
		PostProcessParameters_t( ) {
			memset( m_flParameters, 0, sizeof( m_flParameters ) );
			m_flParameters[ PPPN_VIGNETTE_START ] = 0.8f;
			m_flParameters[ PPPN_VIGNETTE_END ] = 1.1f;
		}

		float m_flParameters[ POST_PROCESS_PARAMETER_COUNT ];
	};

	static auto PostProcessParameters = *reinterpret_cast< PostProcessParameters_t** >( Memory::Scan( XorStr( "client.dll" ), XorStr( "0F 11 05 ? ? ? ? 0F 10 87" ) ) + 3 );
	PostProcessParameters->m_flParameters[ PPPN_VIGNETTE_BLUR_STRENGTH ] = 0.f;
	PostProcessParameters->m_flParameters[ PPPN_SCREEN_BLUR_STRENGTH ] = 0.f;
}

void Miscellaneous::ForceCrosshair( ) {
	C_CSPlayer* pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal ) {
		return;
	}

	static auto weapon_debug_spread_show = g_pCVar->FindVar( XorStr( "weapon_debug_spread_show" ) );
	if( !weapon_debug_spread_show ) {
		return;
	}

	static bool bReset = false;
	if( !g_Vars.esp.force_sniper_crosshair ) {
		if( bReset ) {
			weapon_debug_spread_show->SetValue( 0 );
			bReset = false;
		}

		return;
	}

	const int nCrosshairValue = pLocal->m_bIsScoped( ) ? 0 : 3;
	if( weapon_debug_spread_show->GetInt( ) != nCrosshairValue )
		weapon_debug_spread_show->SetValue( nCrosshairValue );

	bReset = true;
}

void Miscellaneous::OnSendDatagram( INetChannel* chan ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( pLocal->IsDead( ) ) {
		return;
	}

	constexpr auto NET_FRAMES_BACKUP = 64; // must be power of 2. 
	constexpr auto NET_FRAMES_MASK = ( NET_FRAMES_BACKUP - 1 );

	if( !g_Vars.misc.ping_spike || !g_Vars.misc.ping_spike_key.enabled )
		return;

	static int nLastSequenceNr = chan->m_nInSequenceNr;
	if( nLastSequenceNr == chan->m_nInSequenceNr )
		return;

	nLastSequenceNr = chan->m_nInSequenceNr;

	float flCorrect = std::max( 0.f, ( g_Vars.misc.ping_spike_amount / 1000.f ) - chan->GetLatency( FLOW_OUTGOING ) - g_LagCompensation.GetLerp( ) );

	chan->m_nInSequenceNr += 2 * NET_FRAMES_MASK - static_cast< uint32_t >( NET_FRAMES_MASK * flCorrect );
}

void Miscellaneous::OverrideFOV( CViewSetup *vsView ) {
	C_CSPlayer *pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal ) {
		return;
	}

	C_CSPlayer *pIntendedEnemy = pLocal;
	if( pLocal->EntIndex( ) == g_pEngine->GetLocalPlayer( ) ) {
		if( pLocal->IsDead( ) ) {
			if( pLocal->m_hObserverTarget( ).IsValid( ) ) {
				const auto pSpectator = ( C_CSPlayer * )pLocal->m_hObserverTarget( ).Get( );
				if( pSpectator ) {
					if( pLocal->m_iObserverMode( ) == 4 || pLocal->m_iObserverMode( ) == 5 )
						pIntendedEnemy = pSpectator;
				}
			}
		}
	}

	if( !pIntendedEnemy ) {
		return;
	}

	auto pWeapon = ( C_WeaponCSBaseGun * )( pIntendedEnemy->m_hActiveWeapon( ).Get( ) );
	if( !pWeapon ) {
		return;
	}

	// we want to set our fov to the slider value, let's see
	// what our fov is right now and see how much we have left
	// till our wish fov

	float flFovLeft = g_Vars.esp.world_fov - vsView->fov;
	float flOverrideFovScope = g_Vars.esp.override_fov_scope;

	if( pLocal->IsDead( ) )
		flOverrideFovScope = 0.f;

	if( pIntendedEnemy->m_bIsScoped( ) ) {
		flFovLeft *= ( ( 100.f - flOverrideFovScope ) / 100.f );
	}

	vsView->fov += flFovLeft;
}