#include "Visuals.hpp"
#include "../../source.hpp"
#include "../../Renderer/Render.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../SDK/Classes/WeaponInfo.hpp"
#include "../Rage/Resolver.hpp"

#include "../Rage/Autowall.hpp"
#include "../../Utils/Threading/threading.h"
#include "../Rage/AntiAim.hpp"
#include "../Rage/ServerAnimations.hpp"
#include <ctime>
#include <iomanip>
#include "../Miscellaneous/Movement.hpp"
#include "../Miscellaneous/PlayerList.hpp"
#include "../Miscellaneous/GrenadeWarning.hpp"
#include "../Rage/TickbaseShift.hpp"

Visuals g_Visuals;
ExtendedVisuals g_ExtendedVisuals;

bool Visuals::SetupBoundingBox( C_CSPlayer *player, Box_t &box ) {
	// default mins/maxs
	static Vector vecDefaultMaxs = Vector( 16.f, 16.f, 72.f /*54.f when crouching, but whatever*/ );

	Vector vecRenderMaxs = player->GetCollideable( ) ? player->GetCollideable( )->OBBMaxs( ) : Vector( );

	if( player->IsDormant( ) ) {
		if( vecRenderMaxs.z < 54.f )
			vecRenderMaxs = vecDefaultMaxs;
	}

	if( vecRenderMaxs.IsZero( ) )
		return false;

	Vector pos = player->GetAbsOrigin( );
	Vector top = pos + Vector( 0, 0, vecRenderMaxs.z );

	Vector2D pos_screen, top_screen;

	if( !Render::Engine::WorldToScreen( pos, pos_screen ) ||
		!Render::Engine::WorldToScreen( top, top_screen ) )
		return false;

	box.x = int( top_screen.x - ( ( pos_screen.y - top_screen.y ) / 2 ) / 2 );
	box.y = int( top_screen.y );

	box.w = int( ( ( pos_screen.y - top_screen.y ) ) / 2 );
	box.h = int( ( pos_screen.y - top_screen.y ) );

	const bool out_of_fov = pos_screen.x + box.w + 20 < 0 || pos_screen.x - box.w - 20 > Render::Engine::m_width || pos_screen.y + 20 < 0 || pos_screen.y - box.h - 20 > Render::Engine::m_height;

	return !out_of_fov;
}

bool Visuals::IsValidPlayer( C_CSPlayer *entity ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return false;

	if( !entity )
		return false;

	if( !entity->IsPlayer( ) )
		return false;

	if( entity->IsDead( ) )
		return false;

	if( entity->EntIndex( ) == g_pEngine->GetLocalPlayer( ) )
		return false;

	if( entity->IsTeammate( pLocal ) )
		return g_Vars.visuals_enemy.teammates;

	if( g_PlayerList.GetSettings( entity->GetSteamID( ) ).m_bDisableVisuals )
		return false;

	return true;
}

bool Visuals::IsValidEntity( C_BaseEntity *entity ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return false;

	if( !entity )
		return false;

	if( entity->IsDormant( ) )
		return false;

	return true;
}

void Visuals::RenderBox( const Box_t &box, C_CSPlayer *entity ) {
	Render::Engine::Rect( box.x, box.y, box.w, box.h, DetermineVisualsColor( visuals_config->box_color.ToRegularColor( ).OverrideAlpha( 210, true ), entity ) );
	Render::Engine::Rect( box.x - 1, box.y - 1, box.w + 2, box.h + 2, Color( 0, 0, 0, 180 * player_fading_alpha.at( entity->EntIndex( ) ) ) );
	Render::Engine::Rect( box.x + 1, box.y + 1, box.w - 2, box.h - 2, Color( 0, 0, 0, 180 * player_fading_alpha.at( entity->EntIndex( ) ) ) );
}

void Visuals::RenderName( const Box_t &box, C_CSPlayer *entity ) {
	player_info_t info;
	if( !g_pEngine->GetPlayerInfo( entity->EntIndex( ), &info ) )
		return;

	// yeah, performance isn't the best here
	std::string name( XorStr( "" ) );

	// if( g_Resolver.m_arrResolverData.at( entity->EntIndex( ) ).m_bReceivedCheatData )
	// 	name.append( XorStr( "[k] " ) );

	name.append( info.szName );

	if( name.length( ) > 15 ) {
		name.resize( 15 );
		name.append( XorStr( "..." ) );
	}

	Render::Engine::esp_bold.string( box.x + ( box.w / 2 ), box.y - Render::Engine::esp_bold.m_size.m_height,
									 DetermineVisualsColor( visuals_config->name_color.ToRegularColor( ).OverrideAlpha( 180, true ), entity ), name.data( ), Render::Engine::ALIGN_CENTER );
}

void Visuals::RenderHealth( const Box_t &box, C_CSPlayer *entity ) {
	const int bar_size = std::clamp( int( ( entity->m_iHealth( ) * box.h ) / entity->m_iMaxHealth( ) ), 0, box.h );

	const int red = std::min( ( 510 * ( entity->m_iMaxHealth( ) - entity->m_iHealth( ) ) ) / entity->m_iMaxHealth( ), 255 );
	const int green = std::min( ( 510 * entity->m_iHealth( ) ) / entity->m_iMaxHealth( ), 255 );

	auto color = Color( red, green, 0 );

	Render::Engine::RectFilled( box.x - 6, box.y - 1, 4, box.h + 2, Color( 0, 0, 0, 180 * player_fading_alpha.at( entity->EntIndex( ) ) ) );
	Render::Engine::RectFilled( box.x - 5, box.y + ( box.h - bar_size ), 2, bar_size, DetermineVisualsColor( color.OverrideAlpha( 210, true ), entity ) );

	// draw health amount when it's lethal damage
	if( entity->m_iHealth( ) <= 92 || entity->m_iHealth( ) > entity->m_iMaxHealth( ) ) {
		// note - michal;
		// either sprintf here, or add some nice formatting library
		// std::to_string is slow, could kill some frames when multiple people are being rendered
		// this could also be apparent on a much larger scale on lower-end pcs

		Render::Engine::esp_pixel.string( box.x - 5, box.y + ( box.h - bar_size ) - 7, Color( 255, 255, 255, 180 * player_fading_alpha.at( entity->EntIndex( ) ) ), std::to_string( entity->m_iHealth( ) ), Render::Engine::ALIGN_CENTER );
	}
}

std::string Visuals::GetWeaponIcon( const int id ) {
	auto search = m_WeaponIconsMap.find( id );
	if( search != m_WeaponIconsMap.end( ) )
		return std::string( &search->second, 1 );

	return XorStr( "" );
}

void Visuals::RenderBottomInfo( const Box_t &box, C_CSPlayer *entity ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	const auto RoundToMultiple = [ & ] ( int in, int multiple ) {
		const auto ratio = static_cast< double >( in ) / multiple;
		const auto iratio = std::lround( ratio );
		return static_cast< int >( iratio * multiple );
	};

	const float flDistance = !pLocal->IsDead( ) ? pLocal->GetAbsOrigin( ).Distance( entity->m_vecOrigin( ) ) : ( pLocal->m_hObserverTarget( ).IsValid( ) && pLocal->m_hObserverTarget( ).Get( ) ) ? reinterpret_cast< C_CSPlayer * >( pLocal->m_hObserverTarget( ).Get( ) )->GetAbsOrigin( ).Distance( entity->m_vecOrigin( ) ) : 0.f;

	const float flMeters = flDistance * 0.0254f;
	const float flFeet = flMeters * 3.281f;

	std::string str = std::to_string( RoundToMultiple( flFeet, 5 ) ) + XorStr( " FT" );

	int offset = 3;
	if( visuals_config->lby_timer ) {
		if( g_Resolver.m_arrResolverData[ entity->EntIndex( ) ].m_bInPredictionStage ) {
			float flAnimationTime = entity->m_flOldSimulationTime( ) + g_pGlobalVars->interval_per_tick;
			float flTimeDifference = entity->m_flSimulationTime( ) - entity->m_flOldSimulationTime( );
			float flUpdateTime = g_Resolver.m_arrResolverData[ entity->EntIndex( ) ].m_flLowerBodyRealignTimer - flAnimationTime;
			float flBoxMultiplier = ( 1.1f - flUpdateTime ) / 1.1f;

			animated_lby.at( entity->EntIndex( ) ) = GUI::Approach( animated_lby.at( entity->EntIndex( ) ), flBoxMultiplier, g_pGlobalVars->frametime * 10.f );
			if( flBoxMultiplier < animated_lby.at( entity->EntIndex( ) ) )
				animated_lby.at( entity->EntIndex( ) ) = flBoxMultiplier;

			const int box_width = std::clamp<int>( ( box.w + 1 ) * animated_lby.at( entity->EntIndex( ) ), 0, box.w );

			if( box_width > 1 ) {
				Render::Engine::RectFilled( box.x - 1, box.y + box.h + offset - 1, box.w + 2, 4, Color( 0, 0, 0, 180 * player_fading_alpha.at( entity->EntIndex( ) ) ) );
				Render::Engine::RectFilled( box.x, box.y + box.h + offset, box_width, 2, DetermineVisualsColor( visuals_config->lby_timer_color.ToRegularColor( ).OverrideAlpha( 210, true ), entity ) );

				// 4px height
				offset += 5;
			}
		}
	}

	auto pWeapon = reinterpret_cast< C_WeaponCSBaseGun * >( entity->m_hActiveWeapon( ).Get( ) );

	const bool bDormant = entity->IsDormant( );

	CCSWeaponInfo *pWeaponData = nullptr;
	if( !bDormant || ( bDormant && g_ExtendedVisuals.m_arrWeaponInfos[ entity->EntIndex( ) ].second == nullptr ) ) {
		if( pWeapon ) {
			g_ExtendedVisuals.m_arrWeaponInfos[ entity->EntIndex( ) ].first = pWeapon->m_iItemDefinitionIndex( );
			g_ExtendedVisuals.m_arrWeaponInfos[ entity->EntIndex( ) ].second = pWeaponData = pWeapon->GetCSWeaponData( ).Xor( );
		}
	}

	bool bDontDo = false;
	if( !pWeaponData ) {
		if( bDormant ) {
			pWeaponData = g_ExtendedVisuals.m_arrWeaponInfos[ entity->EntIndex( ) ].second;
		}

		if( !pWeaponData ) {
			bDontDo = true;
		}
	}

	if( !bDontDo ) {
		if( visuals_config->ammo && ( ( !bDormant && pWeapon && pWeaponData ) || ( bDormant && pWeaponData ) ) ) {
			const bool bMaxOutAmmo = bDormant; //&& last_non_dormant_weapon.at( entity->EntIndex( ) ) != pWeapon->m_iItemDefinitionIndex( );

			// don't render on knifes, zeus, etc
			bool bIsInvalidWeapon = pWeaponData->m_iWeaponType == WEAPONTYPE_GRENADE || pWeaponData->m_iWeaponType == WEAPONTYPE_KNIFE || pWeaponData->m_iWeaponType == WEAPONTYPE_C4 || pWeaponData->m_iMaxClip <= 0;
			if( !bIsInvalidWeapon ) {
				const int nCurrentClip = bMaxOutAmmo ? pWeaponData->m_iMaxClip : pWeapon->m_iClip1( );

				// also prevent division by zero, lol
				float flBoxMultiplier = ( float )nCurrentClip / pWeaponData->m_iMaxClip;

				bool bReloading = false;
				auto pReloadLayer = entity->m_AnimOverlay( ).Element( 1 );
				if( pReloadLayer.m_pOwner ) {
					const int nActivity = entity->GetSequenceActivity( pReloadLayer.m_nSequence );

					if( nActivity == 967 && pReloadLayer.m_flWeight != 0.f ) {
						// smooth out the ammo bar for reloading players
						flBoxMultiplier = pReloadLayer.m_flCycle;
						bReloading = true;
					}
				}

				const int box_width = std::clamp<int>( ( box.w + 1 ) * flBoxMultiplier, 0, box.w );

				Render::Engine::RectFilled( box.x - 1, box.y + box.h + offset - 1, box.w + 2, 4, Color( 0, 0, 0, 180 * player_fading_alpha.at( entity->EntIndex( ) ) ) );
				Render::Engine::RectFilled( box.x, box.y + box.h + offset, box_width, 2, DetermineVisualsColor( visuals_config->ammo_color.ToRegularColor( ).OverrideAlpha( 210, true ), entity ) );

				// ammo is less than 90% of the max ammo
				if( nCurrentClip > 0 && nCurrentClip <= int( std::floor( float( pWeaponData->m_iMaxClip ) * 0.9f ) ) && !bReloading ) {
					Render::Engine::esp_pixel.string( box.x + box_width, ( box.y + box.h + offset ) - 3, DetermineVisualsColor( Color::White( ).OverrideAlpha( 210, true ), entity ),
													  std::to_string( nCurrentClip ), Render::Engine::ALIGN_CENTER );
				}

				// 4px height
				offset += 4;
			}
		}
	}

	if( visuals_config->distance ) {
		Render::Engine::esp_pixel.string( box.x + ( box.w / 2 ), box.y + box.h + offset,
										  DetermineVisualsColor( Color::White( ).OverrideAlpha( 210, true ), entity ), str.data( ), Render::Engine::ALIGN_CENTER );

		offset += Render::Engine::esp_pixel.m_size.m_height;
	}

	if( !bDontDo ) {
		if( visuals_config->weapon && pWeaponData ) {
			// note - michal;
			// not the best code optimization-wise, again...
			// i'll end up leaving notes everywhere that i'll improve performance later on
			std::wstring localized = g_pLocalize->Find( pWeaponData->m_szHudName );
			std::string name( localized.begin( ), localized.end( ) );
			std::transform( name.begin( ), name.end( ), name.begin( ), ::toupper );

			Render::Engine::esp_pixel.string( box.x + ( box.w / 2 ), box.y + box.h + offset,
											  DetermineVisualsColor( Color::White( ).OverrideAlpha( 210, true ), entity ), name.data( ), Render::Engine::ALIGN_CENTER );

			offset += Render::Engine::esp_pixel.m_size.m_height;
		}
	}

	if( visuals_config->weapon_icon && ( ( !bDormant && pWeapon ) || bDormant ) ) {
		Render::Engine::cs.string( box.x + ( box.w / 2 ), box.y + box.h + offset,
								   DetermineVisualsColor( visuals_config->weapon_color.ToRegularColor( ).OverrideAlpha( 210, true ), entity ), GetWeaponIcon( bDormant ? g_ExtendedVisuals.m_arrWeaponInfos[ entity->EntIndex( ) ].first : pWeapon->m_iItemDefinitionIndex( ) ), Render::Engine::ALIGN_CENTER );

		offset += Render::Engine::cs.m_size.m_height;
	}
}

void Visuals::RenderSideInfo( const Box_t &box, C_CSPlayer *entity ) {
	std::vector<std::pair<std::string, Color>> vec_flags{ };

#if 1
	if( g_Vars.visuals_enemy.money )
		vec_flags.emplace_back( std::string( XorStr( "$" ) ).append( std::to_string( entity->m_iAccount( ) ) ), Color( 155, 210, 100 ) );

	if( fabs( g_ExtendedVisuals.m_flLastDopiumPacketTime[ entity->EntIndex( ) ] - TICKS_TO_TIME( g_pGlobalVars->tickcount ) ) < 2.5f && g_ExtendedVisuals.m_flLastDopiumPacketTime[ entity->EntIndex( ) ] > 0.f )
		vec_flags.emplace_back( XorStr( "DU" ), Color( 255, 255, 255 ) );

	if( entity->IsReloading( ) )
		vec_flags.emplace_back( XorStr( "R" ), Color( 0, 175, 255 ) );

	if( entity->m_ArmorValue( ) > 0 ) {
		if( entity->m_bHasHelmet( ) )
			vec_flags.emplace_back( XorStr( "HK" ), Color( 255, 255, 255 ) );
		else
			vec_flags.emplace_back( XorStr( "K" ), Color( 255, 255, 255 ) );
	}

	float m_flFlashBangTime = *( float * )( ( uintptr_t )entity + 0xA2E8 );
	if( m_flFlashBangTime > 0.f )
		vec_flags.emplace_back( XorStr( "BLIND" ), Color( 0, 175, 255 ) );

	if( entity->m_bIsScoped( ) )
		vec_flags.emplace_back( XorStr( "ZOOM" ), Color( 0, 175, 255 ) );

	if( auto pLagData = g_LagCompensation.GetLagData( entity->m_entIndex ); pLagData ) {
		if( pLagData->m_Records.size( ) ) {
			if( auto pRecord = &pLagData->m_Records.front( ); pRecord ) {
				if( !pRecord->m_bIsResolved )
					vec_flags.emplace_back( XorStr( "FAKE" ), Color( 255, 255, 255 ) );
			}
		}
	}

	auto pPlayerResource = ( *g_pPlayerResource.Xor( ) );

	if( visuals_config->latency ) {
		if( pPlayerResource && pPlayerResource->GetPlayerPing( entity->EntIndex( ) ) >= 150 ) {
			const float flPing = pPlayerResource->GetPlayerPing( entity->EntIndex( ) );
			const float flPingMultiplier = std::clamp<float>( flPing / 750.f, 0.f, 1.f );
			Color color = Color::Blend( Color::White( ), Color::Red( ), flPingMultiplier );

			vec_flags.emplace_back( XorStr( "PING" ), color );
		}
	}

	// note - maxwell; good for testing...
	auto &resolverData = g_Resolver.m_arrResolverData.at( entity->EntIndex( ) );

	if( false ) {
		const float flLatestMovePlayback = std::get<0>( resolverData.m_tLoggedPlaybackRates );
		const float flPreviousMovePlayback = std::get<1>( resolverData.m_tLoggedPlaybackRates );
		const float flPenultimateMovePlayback = std::get<2>( resolverData.m_tLoggedPlaybackRates );

		std::stringstream _1, _2, _3;
		_1 << XorStr( "ideals: " ) << std::to_string( resolverData.m_nIdealFootYawSide );
		//_2 << XorStr( "prev: " ) << std::to_string( flPreviousMovePlayback );
		//_3 << XorStr( "penult: " ) << std::to_string( flPenultimateMovePlayback );

		vec_flags.emplace_back( _1.str( ).data( ), Color::White( ) );
		//vec_flags.emplace_back( _2.str( ).data( ), Color::White( ) );
		//vec_flags.emplace_back( _3.str( ).data( ), Color::White( ) );
	}

	for( size_t i = 0; i < 48; ++i ) {
		auto pWeapons = entity->m_hMyWeapons( )[ i ];
		if( !pWeapons.IsValid( ) )
			break;

		auto pItem = ( C_BaseCombatWeapon * )pWeapons.Get( );
		if( !pItem )
			continue;

		auto nItemDefinitionIndex = pItem->m_Item( ).m_iItemDefinitionIndex( );
		if( nItemDefinitionIndex == WEAPON_C4 )
			vec_flags.emplace_back( XorStr( "B" ), Color( 255, 0, 0 ) );
	}

#else
	C_AnimationLayer *layer_6 = &entity->m_AnimOverlay( )[ 6 ];
	C_AnimationLayer *layer_3 = &entity->m_AnimOverlay( )[ 3 ];
	C_AnimationLayer *layer_12 = &entity->m_AnimOverlay( )[ 12 ];
	if( layer_6 && layer_3 && layer_12 && entity->m_PlayerAnimState( ) ) {
		int layer_3_activity = entity->GetSequenceActivity( layer_3->m_nSequence );

		vec_flags.emplace_back( std::string( XorStr( "layer 3 act: " ) ).append( std::to_string( layer_3_activity ) ), Color( 255, 255, 255 ) );
		vec_flags.emplace_back( std::string( XorStr( "layer 3 weight: " ) ).append( std::to_string( layer_3->m_flWeight ) ), Color( 255, 255, 255 ) );
		vec_flags.emplace_back( std::string( XorStr( "layer 3 cycle: " ) ).append( std::to_string( layer_3->m_flCycle ) ), Color( 255, 255, 255 ) );
		vec_flags.emplace_back( std::string( XorStr( "layer 6 weight: " ) ).append( std::to_string( layer_6->m_flWeight ) ), Color( 255, 255, 255 ) );
		vec_flags.emplace_back( std::string( XorStr( "layer 6 cycle: " ) ).append( std::to_string( layer_6->m_flCycle ) ), Color( 255, 255, 255 ) );
		vec_flags.emplace_back( std::string( XorStr( "layer 6 playback: " ) ).append( std::to_string( layer_6->m_flPlaybackRate ) ), Color( 255, 255, 255 ) );
		vec_flags.emplace_back( std::string( XorStr( "layer 12 weight: " ) ).append( std::to_string( layer_12->m_flWeight ) ), Color( 255, 255, 255 ) );
		vec_flags.emplace_back( std::string( XorStr( "choke: " ) ).append( std::to_string( TIME_TO_TICKS( entity->m_flSimulationTime( ) - entity->m_flOldSimulationTime( ) ) ) ), Color( 255, 255, 255 ) );
		vec_flags.emplace_back( std::string( XorStr( "lby: " ) ).append( std::to_string( entity->m_flLowerBodyYawTarget( ) ) ), Color( 255, 255, 255 ) );
		vec_flags.emplace_back( std::string( XorStr( "velocity: " ) ).append( std::to_string( entity->m_PlayerAnimState( )->m_flVelocityLengthXY ) ), Color( 255, 255, 255 ) );
		vec_flags.emplace_back( std::string( XorStr( "abs yaw delta: " ) ).append( std::to_string( Math::AngleDiff( entity->m_PlayerAnimState( )->m_flEyeYaw, entity->m_PlayerAnimState( )->m_flFootYaw ) ) ), Color( 255, 255, 255 ) );
	}
#endif

	int offset{ 0 };
	for( auto flag : vec_flags ) {
		std::transform( flag.first.begin( ), flag.first.end( ), flag.first.begin( ), ::toupper );

		// draw the string
		Render::Engine::esp_pixel.string( box.x + box.w + 2, box.y - 1 + offset, DetermineVisualsColor( flag.second.OverrideAlpha( 180 ), entity ), flag.first );

		// extend offset
		offset += Render::Engine::esp_pixel.m_size.m_height;
	}
}

void Visuals::RenderDroppedWeapons( C_BaseEntity *entity ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	auto pWeapon = reinterpret_cast< C_WeaponCSBaseGun * >( entity );
	if( !pWeapon )
		return;

	if( !pWeapon->m_iItemDefinitionIndex( ) || pWeapon->m_hOwnerEntity( ) != -1 )
		return;

	auto pWeaponData = pWeapon->GetCSWeaponData( );
	if( !pWeaponData.IsValid( ) )
		return;

	Vector2D screen_position{ };
	if( !Render::Engine::WorldToScreen( pWeapon->GetAbsOrigin( ), screen_position ) )
		return;

	auto clientClass = entity->GetClientClass( );
	if( !clientClass )
		return;

	auto bIsC4 = clientClass->m_ClassID == CC4;

	// note - michal;
	// not the best code optimization-wise, again...
	// i'll end up leaving notes everywhere that i'll improve performance later on
	std::wstring localized = g_pLocalize->Find( pWeaponData->m_szHudName );
	std::string name( localized.begin( ), localized.end( ) );
	std::transform( name.begin( ), name.end( ), name.begin( ), ::toupper );

	if( name.empty( ) )
		return;

	// LOL
	float distance = !pLocal->IsDead( ) ? pLocal->GetAbsOrigin( ).Distance( entity->m_vecOrigin( ) ) : ( pLocal->m_hObserverTarget( ).IsValid( ) && pLocal->m_hObserverTarget( ).Get( ) ) ? reinterpret_cast< C_CSPlayer * >( pLocal->m_hObserverTarget( ).Get( ) )->GetAbsOrigin( ).Distance( entity->m_vecOrigin( ) ) : 0.f;

	const auto clamped_distance = std::clamp<float>( distance - 300.f, 0.f, 360.f );
	float alpha = bIsC4 ? 180.f : 180.f - ( clamped_distance * 0.5f );

	if( alpha < 0.f )
		return;

	if( g_Vars.esp.dropped_weapons || ( bIsC4 && g_Vars.esp.draw_bomb ) ) {
		Render::Engine::esp_pixel.string( screen_position.x, screen_position.y,
										  ( bIsC4 && g_Vars.esp.draw_bomb ) ? g_Vars.esp.draw_bomb_color.ToRegularColor( ).OverrideAlpha( 180, true ) : g_Vars.esp.dropped_weapons_color.ToRegularColor( ).OverrideAlpha( alpha, true ),
										  ( bIsC4 && g_Vars.esp.draw_bomb ) ? XorStr( "BOMB" ) : name.data( ) );
	}

	if( g_Vars.esp.dropped_weapons_ammo && !bIsC4 ) {
		const auto clip = pWeapon->m_iClip1( );
		if( clip > 0 ) {
			const auto text_size = Render::Engine::esp_pixel.size( name );
			const auto max_clip = pWeaponData->m_iMaxClip;

			auto width = text_size.m_width;
			width *= clip;

			// even tho max_clip should never be 0, better safe..
			if( max_clip )
				width /= max_clip;

			// outline
			Render::Engine::RectFilled( screen_position + Vector2D( 0, 9 ), Vector2D( text_size.m_width + 1, 4 ), Color( 0.f, 0.f, 0.f, alpha ) );

			// actual bar
			Render::Engine::RectFilled( screen_position + Vector2D( 1, 10 ), Vector2D( width - 1, 2 ), visuals_config->ammo_color.ToRegularColor( ).OverrideAlpha( alpha ) );

			// draw bullets in clip
			if( clip <= static_cast< int >( max_clip * 0.34 ) ) {
				Render::Engine::esp_pixel.string( screen_position.x + width, screen_position.y + 8, Color::White( ).OverrideAlpha( alpha ), std::to_string( clip ) );
			}
		}
	}
}

void Visuals::RenderNades( C_BaseEntity *entity ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( !entity )
		return;

	// yeah nigga I grab it here? got a problem, huh?
	static auto m_nExplodeEffectTickBeginOffsetz = Engine::g_PropManager.GetOffset( XorStr( "DT_BaseCSGrenadeProjectile" ), XorStr( "m_nExplodeEffectTickBegin" ) );

	auto client_class = entity->GetClientClass( );
	if( !client_class )
		return;

	std::string grenade{ };

	// CInferno does not have a model...
	if( client_class->m_ClassID != CInferno ) {
		auto model = entity->GetModel( );
		if( !model )
			return;

		std::string model_name = g_pModelInfo->GetModelName( model );
		if( model_name.empty( ) )
			return;

		const auto smoke = reinterpret_cast< C_SmokeGrenadeProjectile * >( entity );
		switch( client_class->m_ClassID ) {
			case CBaseCSGrenadeProjectile:
				if( *( int * )( uintptr_t( entity ) + m_nExplodeEffectTickBeginOffsetz ) ) {
					return;
				}

				// seriously, just why?
				// this game is so shit
				if( model_name.find( XorStr( "fraggrenade" ) ) != std::string::npos ) {
					grenade = XorStr( "FRAG" );
				}
				else {
					grenade = XorStr( "FLASHBANG" );
				}
				break;
			case CMolotovProjectile:
				grenade = XorStr( "FIRE" );
				break;
			case CSmokeGrenadeProjectile:
				grenade = XorStr( "SMOKE" );

				// apparently m_bDidSmokeEffect doesn't seem to work?
				if( smoke ) {
					const auto spawn_time = TICKS_TO_TIME( smoke->m_nSmokeEffectTickBegin( ) );
					const auto time = ( spawn_time + C_SmokeGrenadeProjectile::GetExpiryTime( ) ) - g_pGlobalVars->curtime;
					const auto factor = time / C_SmokeGrenadeProjectile::GetExpiryTime( );

					if( factor > 0.0f ) {
						grenade.clear( );
					}
				}

				break;
			case CDecoyProjectile:
				grenade = XorStr( "DECOY" );
				break;
		}
	}

	Vector2D screen_position{ };
	if( !Render::Engine::WorldToScreen( entity->GetAbsOrigin( ), screen_position ) )
		return;

	float distance = !pLocal->IsDead( ) ? pLocal->GetAbsOrigin( ).Distance( entity->m_vecOrigin( ) ) : ( pLocal->m_hObserverTarget( ).IsValid( ) && pLocal->m_hObserverTarget( ).Get( ) ) ? reinterpret_cast< C_CSPlayer * >( pLocal->m_hObserverTarget( ).Get( ) )->GetAbsOrigin( ).Distance( entity->m_vecOrigin( ) ) : 0.f;

	const auto clamped_distance = std::clamp<float>( distance - 300.f, 0.f, 360.f );
	float alpha = std::clamp( ( 180.f - ( clamped_distance * 0.5f ) ) / 180.f, 0.f, 1.f );

	// draw nade string..
	if( !grenade.empty( ) && client_class->m_ClassID != CInferno ) {
		Render::Engine::esp_pixel.string( screen_position.x, screen_position.y, Color::White( ), grenade, Render::Engine::ALIGN_CENTER );
	}

	if( alpha < 0.f )
		return;

	static const auto size = Vector2D( 70.f, 4.f );
	const auto scaled_position = Vector2D( screen_position.x - size.x * 0.5, screen_position.y - size.y * 0.5 );

	if( entity->GetClientClass( )->m_ClassID == CInferno ) {
		C_Inferno *pInferno = reinterpret_cast< C_Inferno * >( entity );
		C_CSPlayer *pOwner = ( C_CSPlayer * )entity->m_hOwnerEntity( ).Get( );

		if( pOwner ) {
			Color_f color = Color_f( 1.f, 0.f, 0.f, 0.8f * alpha );

			bool draw = true;
			if( auto pLocal = C_CSPlayer::GetLocalPlayer( ); pLocal ) {
				if( pOwner->m_iTeamNum( ) == pLocal->m_iTeamNum( ) && pOwner->EntIndex( ) != g_pEngine->GetLocalPlayer( ) ) {
					if( g_Vars.mp_friendlyfire->GetInt( ) != 1 ) {
						draw = false;
					}
				}
			}

			const Vector origin = pInferno->GetAbsOrigin( );
			Vector2D screen_origin = Vector2D( );

			if( Render::Engine::WorldToScreen( origin, screen_origin ) && draw ) {
				struct s {
					Vector2D a, b, c;
				};
				std::vector<int> excluded_ents;
				std::vector<s> valid_molotovs;

				const auto spawn_time = pInferno->m_flSpawnTime( );
				const auto time = ( ( spawn_time + C_Inferno::GetExpiryTime( ) ) - g_pGlobalVars->curtime );

				if( time > 0.05f ) {
					static const auto size = Vector2D( 70.f, 4.f );

					auto new_pos = Vector2D( screen_origin.x - size.x * 0.5, screen_origin.y - size.y * 0.5 );

					Vector min, max;
					if( entity->GetClientRenderable( ) )
						entity->GetClientRenderable( )->GetRenderBounds( min, max );

					auto radius = ( max - min ).Length2D( ) * 0.5f;
					Vector boundOrigin = Vector( ( min.x + max.x ) * 0.5f, ( min.y + max.y ) * 0.5f, min.z + 5 ) + origin;
					const int accuracy = 25;
					const float step = DirectX::XM_2PI / accuracy;
					for( float a = 0.0f; a < DirectX::XM_2PI; a += step ) {
						float a_c, a_s, as_c, as_s;
						DirectX::XMScalarSinCos( &a_s, &a_c, a );
						DirectX::XMScalarSinCos( &as_s, &as_c, a + step );

						Vector startPos = Vector( a_c * radius + boundOrigin.x, a_s * radius + boundOrigin.y, boundOrigin.z );
						Vector endPos = Vector( as_c * radius + boundOrigin.x, as_s * radius + boundOrigin.y, boundOrigin.z );

						Vector2D start2d, end2d, boundorigin2d;
						if( !Render::Engine::WorldToScreen( startPos, start2d ) || !Render::Engine::WorldToScreen( endPos, end2d ) || !Render::Engine::WorldToScreen( boundOrigin, boundorigin2d ) ) {
							excluded_ents.push_back( entity->EntIndex( ) );
							continue;
						}

						s n;
						n.a = start2d;
						n.b = end2d;
						n.c = boundorigin2d;
						valid_molotovs.push_back( n );
					}

					if( !excluded_ents.empty( ) ) {
						for( int v = 0; v < excluded_ents.size( ); ++v ) {
							auto bbrr = excluded_ents[ v ];
							if( bbrr == entity->EntIndex( ) )
								continue;

							if( !valid_molotovs.empty( ) )
								for( int m = 0; m < valid_molotovs.size( ); ++m ) {
									auto ba = valid_molotovs[ m ];
									//Render::Engine::FilledTriangle( ba.c, ba.a, ba.b, color.ToRegularColor( ).OverrideAlpha( 45 ) );

									if( ( ( C_CSPlayer * )pInferno )->m_vecVelocity( ).Length2D( ) <= 0.1f )
										Render::Engine::Line( ba.a, ba.b, color.ToRegularColor( ).OverrideAlpha( 220 * alpha ) );
								}
						}
					}
					else {
						if( !valid_molotovs.empty( ) )
							for( int m = 0; m < valid_molotovs.size( ); ++m ) {
								auto ba = valid_molotovs[ m ];
								//Render::Engine::FilledTriangle( ba.c, ba.a, ba.b, color.ToRegularColor( ).OverrideAlpha( 45 ) );

								if( ( ( C_CSPlayer * )pInferno )->m_vecVelocity( ).Length2D( ) <= 0.1f )
									Render::Engine::Line( ba.a, ba.b, color.ToRegularColor( ).OverrideAlpha( 220 * alpha ) );
							}
					}

					const float flMagic = std::clamp( std::clamp( ( time + 1.f ), 0.f, C_Inferno::GetExpiryTime( ) ) / ( C_Inferno::GetExpiryTime( ) ), 0.f, 1.f );

					if( ( ( C_CSPlayer * )pInferno )->m_vecVelocity( ).Length2D( ) <= 0.1f ) {
						auto size = Render::Engine::cs_large.size( GetWeaponIcon( WEAPON_MOLOTOV ) );

						Render::Engine::CircleFilled( screen_origin.x, screen_origin.y - ( size.m_height / 2 ), 17.f, 130, Color( 0, 0, 0, 100 ) );
						Render::Engine::cs_large.string( screen_origin.x - ( size.m_width / 2.f ), screen_origin.y - size.m_height, Color::White( ).OverrideAlpha( 50 ), GetWeaponIcon( WEAPON_MOLOTOV ) );

						Render::Engine::SetClip( { screen_origin.x - ( size.m_width / 2.f ), screen_origin.y - ( size.m_height * flMagic ) }, Vector2D( size.m_width, size.m_height ) );
						Render::Engine::cs_large.string( screen_origin.x - ( size.m_width / 2.f ), screen_origin.y - size.m_height, Color::White( ).OverrideAlpha( 210 ), GetWeaponIcon( WEAPON_MOLOTOV ) );
						Render::Engine::ResetClip( );
					}

				}
				else {
					if( !valid_molotovs.empty( ) )
						valid_molotovs.erase( valid_molotovs.begin( ) + entity->EntIndex( ) );

					if( !excluded_ents.empty( ) )
						excluded_ents.erase( excluded_ents.begin( ) + entity->EntIndex( ) );
				}

				excluded_ents.clear( );
				valid_molotovs.clear( );
			}
		}
	}

	C_SmokeGrenadeProjectile *pSmokeEffect = reinterpret_cast< C_SmokeGrenadeProjectile * >( entity );
	if( pSmokeEffect->GetClientClass( )->m_ClassID == CSmokeGrenadeProjectile ) {
		const Vector origin = pSmokeEffect->GetAbsOrigin( );
		Vector2D screen_origin = Vector2D( );

		if( Render::Engine::WorldToScreen( origin, screen_origin ) ) {
			struct s {
				Vector2D a, b;
			};
			std::vector<int> excluded_ents;
			std::vector<s> valid_smokes;
			const auto spawn_time = TICKS_TO_TIME( pSmokeEffect->m_nSmokeEffectTickBegin( ) );
			const auto time = ( spawn_time + C_SmokeGrenadeProjectile::GetExpiryTime( ) ) - g_pGlobalVars->curtime;

			static const auto size = Vector2D( 70.f, 4.f );

			if( time > 0.05f ) {
				auto radius = 120.f;

				const int accuracy = 25;
				const float step = DirectX::XM_2PI / accuracy;
				for( float a = 0.0f; a < DirectX::XM_2PI; a += step ) {
					float a_c, a_s, as_c, as_s;
					DirectX::XMScalarSinCos( &a_s, &a_c, a );
					DirectX::XMScalarSinCos( &as_s, &as_c, a + step );

					Vector startPos = Vector( a_c * radius + origin.x, a_s * radius + origin.y, origin.z + 5 );
					Vector endPos = Vector( as_c * radius + origin.x, as_s * radius + origin.y, origin.z + 5 );

					Vector2D start2d, end2d;
					if( !Render::Engine::WorldToScreen( startPos, start2d ) || !Render::Engine::WorldToScreen( endPos, end2d ) ) {
						excluded_ents.push_back( entity->EntIndex( ) );
						continue;
					}

					s n;
					n.a = start2d;
					n.b = end2d;
					valid_smokes.push_back( n );
				}

				if( !excluded_ents.empty( ) ) {
					for( int v = 0; v < excluded_ents.size( ); ++v ) {
						auto bbrr = excluded_ents[ v ];
						if( bbrr == entity->EntIndex( ) )
							continue;

						if( !valid_smokes.empty( ) )
							for( int m = 0; m < valid_smokes.size( ); ++m ) {
								auto ba = valid_smokes[ m ];
								//Render::Engine::FilledTriangle( screen_origin, ba.a, ba.b, Color( 220, 220, 220, 25 ) );
								if( ( ( C_CSPlayer * )pSmokeEffect )->m_vecVelocity( ).Length2D( ) <= 0.1f )
									Render::Engine::Line( ba.a, ba.b, Color(110, 102, 255, 220 * alpha ) );
							}
					}
				}
				else {
					if( !valid_smokes.empty( ) )
						for( int m = 0; m < valid_smokes.size( ); ++m ) {
							auto ba = valid_smokes[ m ];
							//Render::Engine::FilledTriangle( screen_origin, ba.a, ba.b, Color( 220, 220, 220, 25 ) );
							if( ( ( C_CSPlayer * )pSmokeEffect )->m_vecVelocity( ).Length2D( ) <= 0.1f )
								Render::Engine::Line( ba.a, ba.b, Color( 220, 220, 220, 220 * alpha ) );
						}
				}

				const float flMagic = std::clamp( std::clamp( ( time + 2.f ), 0.f, C_SmokeGrenadeProjectile::GetExpiryTime( ) ) / ( C_SmokeGrenadeProjectile::GetExpiryTime( ) ), 0.f, 1.f );

				if( ( ( C_CSPlayer * )pSmokeEffect )->m_vecVelocity( ).Length2D( ) <= 0.1f ) {
					auto size = Render::Engine::cs_large.size( GetWeaponIcon( WEAPON_SMOKEGRENADE ) );

					Render::Engine::CircleFilled( screen_origin.x, screen_origin.y - ( size.m_height / 2 ), 17.f, 130, Color( 0, 0, 0, 100 ) );
					Render::Engine::cs_large.string( screen_origin.x - ( size.m_width / 2.f ), screen_origin.y - size.m_height, Color::White( ).OverrideAlpha( 50 ), GetWeaponIcon( WEAPON_SMOKEGRENADE ) );

					Render::Engine::SetClip( { screen_origin.x - ( size.m_width / 2.f ), screen_origin.y - ( size.m_height * flMagic ) }, Vector2D( size.m_width, size.m_height ) );
					Render::Engine::cs_large.string( screen_origin.x - ( size.m_width / 2.f ), screen_origin.y - size.m_height, Color::White( ).OverrideAlpha( 210 ), GetWeaponIcon( WEAPON_SMOKEGRENADE ) );
					Render::Engine::ResetClip( );
				}

				//Render::Engine::esp_pixel.string( new_pos.x + ( Render::Engine::esp_pixel.size( std::string( XorStr( "SMOKE - " ) ).append( kek ) ).m_width * 0.5f ) - 2, new_pos.y - 15, Color( 255, 255, 255, 180 ), XorStr( "SMOKE" ), Render::Engine::ALIGN_CENTER );
			}
			else {
				if( !valid_smokes.empty( ) )
					valid_smokes.erase( valid_smokes.begin( ) + entity->EntIndex( ) );

				if( !excluded_ents.empty( ) )
					excluded_ents.erase( excluded_ents.begin( ) + entity->EntIndex( ) );
			}

			excluded_ents.clear( );
			valid_smokes.clear( );
		}
	}
}

/* this is fucking useless tibbzy is a fucking retarded paster this could of been done 20 mins ago also tibbzy was not in fact having any fun
void Visuals::DrawHitboxMatrix(C_LagRecord* record, Color col, float time) {
	const model_t* model;
	studiohdr_t* hdr;
	mstudiohitboxset_t* set;
	mstudiobbox_t* bbox;
	Vector            mins, maxs, origin;
	QAngle			   angle;

	model = record->player->GetModel();
	if (!model)
		return;

	hdr = g_pModelInfo->GetStudiomodel(model);
	if (!hdr)
		return;

	set = hdr->pHitboxSet(record->player->m_nHitboxSet());
	if (!set)
		return;

	for (int i{ }; i < set->numhitboxes; ++i) {
		bbox = set->pHitbox(i);
		if (!bbox)
			continue;

		// bbox.
		if (bbox->m_flRadius <= 0.f) {
			// https://developer.valvesoftware.com/wiki/Rotation_Tutorial

			// convert rotation angle to a matrix.
			matrix3x4_t rot_matrix;
			rot_matrix.AngleMatrix(bbox->m_angAngles);

			// apply the rotation to the entity input space (local).
			matrix3x4_t matrix;
			matrix.ConcatTransforms(record->m_pMatrix[bbox->bone], rot_matrix);

			// extract the compound rotation as an angle.
			QAngle bbox_angle;
			math::MatrixAngles(matrix, bbox_angle);

			// extract hitbox origin.
			Vector origin = matrix.GetOrigin();

			// draw box.
			g_csgo.m_debug_overlay->AddBoxOverlay(origin, bbox->bbmin, bbox->bbmax, bbox_angle, col.r(), col.g(), col.b(), 0, time);
		}

		// capsule.
		else {
			// NOTE; the angle for capsules is always 0.f, 0.f, 0.f.

			// create a rotation matrix.
			matrix3x4_t matrix;
			matrix.AngleMatrix(bbox->m_angAngles);

			// apply the rotation matrix to the entity output space (world).
			matrix.ConcatTransforms(record->m_pMatrix[bbox->bone], matrix);

			// get world positions from new matrix.
			math::VectorTransform(bbox->bbmin, matrix, mins);
			math::VectorTransform(bbox->bbmax, matrix, maxs);

			g_csgo.m_debug_overlay->AddCapsuleOverlay(mins, maxs, bbox->m_flRadius, col.r(), col.g(), col.b(), col.a(), time, 0, 0);
		}
	}
}*/


std::string Visuals::GetBombSite( C_PlantedC4 *entity ) {
	if( !g_pPlayerResource.IsValid( ) || !( *g_pPlayerResource.Xor( ) ) )
		return XorStr( "Error while finding bombsite..." );

	const auto &origin = entity->GetAbsOrigin( );

	// gosh I hate dereferencing it here!
	const auto &bomb_a = ( *g_pPlayerResource.Xor( ) )->m_bombsiteCenterA( );
	const auto &bomb_b = ( *g_pPlayerResource.Xor( ) )->m_bombsiteCenterB( );

	const auto dist_a = origin.Distance( bomb_a );
	const auto dist_b = origin.Distance( bomb_b );

	return dist_a < dist_b ? XorStr( "A" ) : XorStr( "B" );
}

void Visuals::RenderObjectives( C_BaseEntity *entity ) {
	if( !entity )
		return;

	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	auto client_class = entity->GetClientClass( );
	if( !client_class )
		return;

	if( client_class->m_ClassID == CPlantedC4 ) {
		const auto plantedc4 = reinterpret_cast< C_PlantedC4 * >( entity );

		if( !plantedc4 )
			return;

		const float c4timer = g_Vars.mp_c4timer->GetFloat( );

		// note - nico;
		// I don't know if we should clamp this to mp_c4timer->GetFloat( )
		// if the mp_c4timer changes while the c4 is planted to something lower than the remaining time
		// it will clamp it.. (this should never really happen, but yeah)
		const float time = std::clamp( plantedc4->m_flC4Blow( ) - g_pGlobalVars->curtime, 0.f, c4timer );

		if( time && !plantedc4->m_bBombDefused( ) ) {
			// SUPREMACY SUPREMACY SUPREMACY SUPREMACY SUPREMACY SUPREMACY SUPREMACY SUPREMACY SUPREMACY SUPREMACY 

			CGameTrace tr;
			CTraceFilter filter;
			auto explosion_origin = plantedc4->GetAbsOrigin( );
			auto explosion_origin_adjusted = explosion_origin;
			explosion_origin_adjusted.z += 8.f;

			// setup filter and do first trace.
			filter.pSkip = plantedc4;

			g_pEngineTrace->TraceRay(
				Ray_t( explosion_origin_adjusted, explosion_origin_adjusted + Vector( 0.f, 0.f, -40.f ) ),
				MASK_SOLID,
				&filter,
				&tr
			);

			// pull out of the wall a bit.
			if( tr.fraction != 1.f )
				explosion_origin = tr.endpos + ( tr.plane.normal * 0.6f );

			// this happens inside CCSGameRules::RadiusDamage.
			explosion_origin.z += 1.f;

			// set all other vars.
			auto m_planted_c4_explosion_origin = explosion_origin;

			auto dst = pLocal->WorldSpaceCenter( );
			auto to_target = m_planted_c4_explosion_origin - dst;
			auto dist = to_target.Length( );

			// calculate the bomb damage based on our distance to the C4's explosion.
			float range_damage = 500.f * std::exp( ( dist * dist ) / ( ( ( ( 500.f * 3.5f ) / 3.f ) * -2.f ) * ( ( 500.f * 3.5f ) / 3.f ) ) );

			static auto scale_damage = [ ] ( float damage, int armor_value ) {
				float new_damage, armor;

				if( armor_value > 0 ) {
					new_damage = damage * 0.5f;
					armor = ( damage - new_damage ) * 0.5f;

					if( armor > ( float )armor_value ) {
						armor = ( float )armor_value * 2.f;
						new_damage = damage - armor;
					}

					damage = new_damage;
				}

				return std::max( 0, ( int )std::floor( damage ) );
			};

			// now finally, scale the damage based on our armor (if we have any).
			float final_damage = scale_damage( range_damage, pLocal->m_ArmorValue( ) );

			// we can clamp this in range 0-10, it can't be higher than 10, lol!
			const float remaining_defuse_time = std::clamp( plantedc4->m_flDefuseCountDown( ) - g_pGlobalVars->curtime, 0.f, 10.f );

			const float factor_c4 = time / ( ( c4timer != 0.f ) ? c4timer : 40.f );
			const float factor_defuse = remaining_defuse_time / 10.f;

			char time_buf[ 128 ] = { };
			sprintf( time_buf, XorStr( " - %.1fs" ), time );

			char dmg_buf[ 128 ] = { };
			sprintf( dmg_buf, XorStr( "-%d HP" ), int( final_damage ) );

			char defuse_buf[ 128 ] = { };
			sprintf( defuse_buf, XorStr( "%.1fs" ), remaining_defuse_time );

			// compute bombsite string
			const auto bomb_site = GetBombSite( plantedc4 ).append( time_buf );

			if( g_Vars.esp.draw_bomb ) {
				const auto screen = Render::GetScreenSize( );
				static const auto size = Vector2D( 160.f, 3.f );

				const float width_c4 = size.x * factor_c4;

				Color site_color = Color( 150, 200, 60, 220 );
				if( time <= 10.f ) {
					site_color = Color( 255, 255, 185, 220 );

					if( time <= 5.f ) {
						site_color = Color( 255, 0, 0, 220 );
					}
				}

				// is this thing being defused?
				if( plantedc4->m_bBeingDefused( ) ) {
					const float width_defuse = size.y * factor_defuse;

					// background
					Render::Engine::RectFilled( Vector2D( 0, 0 ), Vector2D( 20, screen.y ), Color( 0, 0, 0, 100 ) );

					// defuse timer bar
					int height = ( screen.y - 2 ) * factor_defuse;
					Render::Engine::RectFilled( Vector2D( 1, 1 + ( int )abs( screen.y - height ) ), Vector2D( 18, height ), Color( 30, 170, 30 ) );
				}

				// draw bomb site string
				if( time > 0.f )
					Render::Engine::esp_indicator.string( 8, 8, site_color, bomb_site, Render::Engine::ALIGN_LEFT );

				if( final_damage > 0 )
					Render::Engine::esp_indicator.string( 8, 36,
														  final_damage >= pLocal->m_iHealth( ) ? Color( 255, 0, 0 ) : Color( 255, 255, 185 ), final_damage >= pLocal->m_iHealth( ) ? XorStr( "FATAL" ) : dmg_buf, Render::Engine::ALIGN_LEFT );
			}

			if( g_Vars.esp.draw_bomb ) {
				static const auto size = Vector2D( 80.f, 3.f );

				Vector2D screen{ };
				if( Render::Engine::WorldToScreen( entity->GetAbsOrigin( ), screen ) ) {
					const float width_c4 = size.x * factor_c4;

					// draw bomb site string
					Render::Engine::esp_pixel.string( screen.x, screen.y - Render::Engine::esp_pixel.m_size.m_height - 1, g_Vars.esp.draw_bomb_color.ToRegularColor( ).OverrideAlpha( 180, true ), XorStr( "BOMB" ), Render::Engine::ALIGN_CENTER );
				}
			}
		}
	}
}

void Visuals::RenderArrows( C_BaseEntity *entity ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	auto RotateArrow = [ ] ( std::array< Vector2D, 3 > &points, float rotation ) {
		const auto vecPointsCenter = ( points.at( 0 ) + points.at( 1 ) + points.at( 2 ) ) / 3;
		for( auto &point : points ) {
			point -= vecPointsCenter;

			const auto temp_x = point.x;
			const auto temp_y = point.y;

			const auto theta = DEG2RAD( rotation );
			const auto c = cos( theta );
			const auto s = sin( theta );

			point.x = temp_x * c - temp_y * s;
			point.y = temp_x * s + temp_y * c;

			point += vecPointsCenter;
		}
	};

	const float flWidth = Render::GetScreenSize( ).x;
	const float flHeight = Render::GetScreenSize( ).y;
	if( !entity || !entity->IsPlayer( ) || entity == pLocal || entity->m_iTeamNum( ) == pLocal->m_iTeamNum( ) )
		return;

	static float alpha[ 65 ];
	static bool plus_or_minus[ 65 ];
	if( alpha[ entity->EntIndex( ) ] <= 5 || alpha[ entity->EntIndex( ) ] >= 255 )
		plus_or_minus[ entity->EntIndex( ) ] = !plus_or_minus[ entity->EntIndex( ) ];

	alpha[ entity->EntIndex( ) ] += plus_or_minus[ entity->EntIndex( ) ] ? ( 255.f / 1.f * g_pGlobalVars->frametime ) : -( 255.f / 1.f * g_pGlobalVars->frametime );
	alpha[ entity->EntIndex( ) ] = std::clamp<float>( alpha[ entity->EntIndex( ) ], 5.f, 255.f );

	Vector2D vecScreenPos;
	const bool bWorldToScreened = Render::Engine::WorldToScreen( entity->GetAbsOrigin( ), vecScreenPos );

	// give some extra room for screen position to be off screen.
	const float flExtraRoomX = flWidth / 18.f;
	const float flExtraRoomY = flHeight / 18.f;

	if( !bWorldToScreened
		|| vecScreenPos.x < -flExtraRoomX
		|| vecScreenPos.x >( flWidth + flExtraRoomX )
		|| vecScreenPos.y < -flExtraRoomY
		|| vecScreenPos.y >( flHeight + flExtraRoomY ) ) {
		QAngle angViewAngles;
		g_pEngine.Xor( )->GetViewAngles( angViewAngles );

		const Vector2D vecScreenCenter = Vector2D( flWidth * .5f, flHeight * .5f );

		Vector vecFromOrigin = pLocal->GetAbsOrigin( );
		if( pLocal->IsDead( ) ) {
			if( pLocal->m_hObserverTarget( ).IsValid( ) && pLocal->m_hObserverTarget( ).Get( ) && pLocal->m_iObserverMode( ) != /*OBS_MODE_ROAMING*/6 ) {
				vecFromOrigin = reinterpret_cast< C_CSPlayer * >( pLocal->m_hObserverTarget( ).Get( ) )->GetAbsOrigin( );
			}
		}

		const float flAngle = ( angViewAngles.y - Math::CalcAngle( vecFromOrigin, entity->GetAbsOrigin( ), true ).y - 90 );
		const float flAngleYaw = DEG2RAD( flAngle );

		// note - michal;
		// when the day comes, i'll eventuall make this dynamic so that we can 
		// choose the distance and the size in pixels of the arrow, but this looks nice for now
		const float flNewPointX = ( vecScreenCenter.x + ( ( ( flWidth - 60.f ) / 2 ) * ( visuals_config->view_arrows_distance / 100.0f ) ) * cos( flAngleYaw ) ) + 8.f;
		const float flNewPointY = ( vecScreenCenter.y + ( ( ( flHeight - 60.f ) / 2 ) * ( visuals_config->view_arrows_distance / 100.0f ) ) * sin( flAngleYaw ) ) + 8.f;

		std::array< Vector2D, 3 >vecPoints{
			Vector2D( flNewPointX - visuals_config->view_arrows_size, flNewPointY - visuals_config->view_arrows_size ),
			Vector2D( flNewPointX + visuals_config->view_arrows_size, flNewPointY ),
			Vector2D( flNewPointX - visuals_config->view_arrows_size, flNewPointY + visuals_config->view_arrows_size ) };

		RotateArrow( vecPoints, flAngle );

		std::array< Vertex_t, 3 > uVertices{
			Vertex_t( vecPoints.at( 0 ) ),
			Vertex_t( vecPoints.at( 1 ) ),
			Vertex_t( vecPoints.at( 2 ) ) };

		static int nTextureID;
		if( !g_pSurface->IsTextureIDValid( nTextureID ) )
			nTextureID = g_pSurface.Xor( )->CreateNewTextureID( true );

		Color clr = visuals_config->view_arrows_color.ToRegularColor( ).OverrideAlpha( ( entity->IsDormant( ) ? 100 : 255 ) * player_fading_alpha.at( entity->EntIndex( ) ), true );

		// fill
		g_pSurface.Xor( )->DrawSetColor( clr.r( ), clr.g( ), clr.b( ), clr.a( ) * ( alpha[ entity->EntIndex( ) ] / 255.f ) );
		g_pSurface.Xor( )->DrawSetTexture( nTextureID );
		g_pSurface.Xor( )->DrawTexturedPolygon( 3, uVertices.data( ) );
	}
}

void Visuals::Spectators( std::vector< std::string > spectators ) {
	C_CSPlayer *pLocal = C_CSPlayer::GetLocalPlayer( );
	if( spectators.empty( ) )
		return;

	if( pLocal->m_iObserverMode( ) == /*OBS_MODE_ROAMING*/6 )
		return;

	for( size_t i{ }; i < spectators.size( ); ++i ) {
		auto msg = spectators[ i ];
		auto width = Render::Engine::esp_bold.size( msg ).m_width;
		auto height = Render::Engine::esp_bold.size( msg ).m_height + 6;

		Render::Engine::esp_bold.string( Render::GetScreenSize( ).x - 8 - width, 6 + ( height * i ), Color( 255, 255, 255, 220 ), msg );
	}
}

void Visuals::RenderSkeleton( C_CSPlayer *entity ) {
	if( !entity ) {
		return;
	}

	auto model = entity->GetModel( );
	if( !model ) {
		return;
	}

	auto *hdr = g_pModelInfo->GetStudiomodel( model );
	if( !hdr ) {
		return;
	}

	if( entity->IsDead( ) ) {
		return;
	}

	if( g_Visuals.player_fading_alpha[ entity->EntIndex( ) ] <= 0.05f || g_ExtendedVisuals.m_arrSoundPlayers.at( entity->EntIndex( ) ).m_bValidSound ) {
		return;
	}

	Vector2D bone1, bone2;

	for( int i = 0; i < hdr->numbones; ++i ) {
		auto pBone = hdr->pBone( i );
		if( !pBone )
			continue;

		if( ( pBone->flags & BONE_USED_BY_HITBOX ) == 0 || pBone->parent < 0 )
			continue;

		auto GetBonePos = [ & ] ( int n ) -> Vector {
			return Vector(
				entity->m_CachedBoneData( ).m_Memory.m_pMemory[ n ][ 0 ][ 3 ],
				entity->m_CachedBoneData( ).m_Memory.m_pMemory[ n ][ 1 ][ 3 ],
				entity->m_CachedBoneData( ).m_Memory.m_pMemory[ n ][ 2 ][ 3 ]
			);
		};

		if( !Render::Engine::WorldToScreen( GetBonePos( i ), bone1 ) || !Render::Engine::WorldToScreen( GetBonePos( pBone->parent ), bone2 ) ) {
			continue;
		}

		Render::Engine::Line( { bone1.x, bone1.y }, { bone2.x, bone2.y }, DetermineVisualsColor( g_Vars.visuals_enemy.skeleton_color.ToRegularColor( ).OverrideAlpha( 210, true ), entity ) );
	}
}

void Visuals::HandlePlayerVisuals( C_CSPlayer *entity ) {
	// do view arrows before any other visuals
	// this is so the bounding box check doesnt interfere with em
	if( visuals_config->view_arrows ) {
		RenderArrows( entity );
	}

	Box_t box;
	if( !SetupBoundingBox( entity, box ) )
		return;

	if( visuals_config->name ) {
		RenderName( box, entity );
	}

	if( visuals_config->box ) {
		RenderBox( box, entity );
	}

	if( visuals_config->health ) {
		RenderHealth( box, entity );
	}

	if( visuals_config->skeleton ) {
		RenderSkeleton( entity );
	}

	RenderBottomInfo( box, entity );

	if( visuals_config->flags )
		RenderSideInfo( box, entity );
}

void Visuals::HandleWorldVisuals( C_BaseEntity *entity ) {
	auto client_class = entity->GetClientClass( );
	if( !client_class )
		return;

	if( g_Vars.esp.dropped_weapons || g_Vars.esp.dropped_weapons_ammo ) {
		if( entity->IsWeapon( ) ) {
			RenderDroppedWeapons( entity );
		}
	}

	switch( client_class->m_ClassID ) {
		case CBaseCSGrenadeProjectile:
		case CMolotovProjectile:
		case CSmokeGrenadeProjectile:
		case CDecoyProjectile:
		case CInferno:
			if( g_Vars.esp.grenades ) {
				RenderNades( entity );
			}
			break;
		case CC4:
		case CPlantedC4:
			RenderObjectives( entity );
			break;
	}

	// already found an entity we are in range of?
	if( !g_Vars.globals.m_bInsideFireRange ) {
		// ok this maths is a huge meme and can prolly be cleaned up LMFAO
		if( client_class->m_ClassID == CInferno ) {
			const auto pLocal = C_CSPlayer::GetLocalPlayer( );
			if( pLocal ) {
				Vector mins, maxs, nmins, nmaxs;
				if( entity->GetClientRenderable( ) )
					entity->GetClientRenderable( )->GetRenderBounds( mins, maxs );

				auto vecLocalAbs = pLocal->GetAbsOrigin( ).ToVector2D( );
				const auto &vecAbsOrigin = entity->GetAbsOrigin( );

				C_CSPlayer *pOwner = ( C_CSPlayer * )entity->m_hOwnerEntity( ).Get( );
				bool bIsLethal = true;
				if( pOwner ) {
					if( pOwner->m_iTeamNum( ) == pLocal->m_iTeamNum( ) && pOwner->EntIndex( ) != g_pEngine->GetLocalPlayer( ) ) {
						if( g_Vars.mp_friendlyfire->GetInt( ) != 1 )
							bIsLethal = false;
					}
				}

				nmins = mins;
				nmaxs = maxs;
				nmins.x *= -1.f;
				nmaxs.x *= -1.f;

				maxs += vecAbsOrigin;
				mins += vecAbsOrigin;
				nmins += vecAbsOrigin;
				nmaxs += vecAbsOrigin;

				bool inBounds = bIsLethal && ( vecLocalAbs >= mins.ToVector2D( ) && vecLocalAbs <= maxs.ToVector2D( ) ) || ( vecLocalAbs >= nmins.ToVector2D( ) && vecLocalAbs <= nmaxs.ToVector2D( ) );

				g_Vars.globals.m_bInsideFireRange = inBounds;
			}
		}
	}
}

Color Visuals::DetermineVisualsColor( Color regular, C_CSPlayer *entity ) {
	if( entity->IsDormant( ) && g_Vars.visuals_enemy.dormant ) {
		return Color( 210, 210, 210, regular.a( ) * player_fading_alpha.at( entity->EntIndex( ) ) );
	}

	Color cRetColor{ regular };
	cRetColor.RGBA[ 3 ] *= player_fading_alpha.at( entity->EntIndex( ) );

	return cRetColor;
}

// lol
bool IsAimingAtPlayerThroughPenetrableWall( C_CSPlayer *local, C_WeaponCSBaseGun *pWeapon ) {
	auto weaponInfo = pWeapon->GetCSWeaponData( );
	if( !weaponInfo.IsValid( ) )
		return -1.0f;

	QAngle view_angles;
	g_pEngine->GetViewAngles( view_angles );

	Autowall::C_FireBulletData data;

	data.m_Player = local;
	data.m_TargetPlayer = nullptr;
	data.m_bPenetration = true;
	data.m_vecStart = local->GetEyePosition( );
	data.m_vecDirection = view_angles.ToVectors( );
	data.m_flMaxLength = data.m_vecDirection.Normalize( );
	data.m_WeaponData = weaponInfo.Xor( );
	data.m_flCurrentDamage = static_cast< float >( weaponInfo->m_iWeaponDamage );

	return Autowall::FireBullets( &data ) >= 1.f;
}

bool yurr( C_CSPlayer *local, C_WeaponCSBaseGun *pWeapon ) {
	auto weaponInfo = pWeapon->GetCSWeaponData( );
	if( !weaponInfo.IsValid( ) )
		return false;

	QAngle view_angles;
	g_pEngine->GetViewAngles( view_angles );

	Autowall::C_FireBulletData data;

	data.m_iPenetrationCount = 4;
	data.m_Player = local;
	data.m_TargetPlayer = nullptr;
	data.m_vecStart = local->GetEyePosition( );
	data.m_vecDirection = view_angles.ToVectors( );
	data.m_flMaxLength = data.m_vecDirection.Normalize( );
	data.m_WeaponData = weaponInfo.Xor( );
	data.m_flTraceLength = 0.0f;
	data.m_flCurrentDamage = static_cast< float >( weaponInfo->m_iWeaponDamage );

	Vector end = data.m_vecStart + data.m_vecDirection * weaponInfo->m_flWeaponRange;

	CTraceFilter filter;
	filter.pSkip = local;

	Autowall::TraceLine( data.m_vecStart, end, MASK_SHOT_HULL | CONTENTS_HITBOX, &filter, &data.m_EnterTrace );

	data.m_flTraceLength += data.m_flMaxLength * data.m_EnterTrace.fraction;
	data.m_flCurrentDamage *= powf( weaponInfo->m_flRangeModifier, data.m_flTraceLength * 0.002f );
	data.m_EnterSurfaceData = g_pPhysSurface->GetSurfaceData( data.m_EnterTrace.surface.surfaceProps );

	return !Autowall::HandleBulletPenetration( &data );
};

void Visuals::PenetrationCrosshair( ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( !g_Vars.esp.autowall_crosshair ) {
		return;
	}

	if( pLocal->IsDead( ) ) {
		return;
	}

	C_WeaponCSBaseGun *pWeapon = ( C_WeaponCSBaseGun * )pLocal->m_hActiveWeapon( ).Get( );
	if( !pWeapon )
		return;

	if( !pWeapon->GetCSWeaponData( ).IsValid( ) )
		return;

	auto type = pWeapon->GetCSWeaponData( ).Xor( )->m_iWeaponType;

	if( type == WEAPONTYPE_KNIFE || type == WEAPONTYPE_C4 || type == WEAPONTYPE_GRENADE )
		return;

	auto screen = Render::GetScreenSize( ) / 2;
	auto dmg = yurr( pLocal, pWeapon );
	bool aim = IsAimingAtPlayerThroughPenetrableWall( pLocal, pWeapon ) && dmg;
	Color color = aim ? ( Color( 0, 100, 255, 210 ) ) : ( dmg ? Color( 0, 255, 0, 210 ) : Color( 255, 0, 0, 210 ) );

	Render::Engine::RectFilled( screen - 1, { 3, 3 }, Color( 0, 0, 0, 125 ) );
	Render::Engine::RectFilled( Vector2D( screen.x, screen.y - 1 ), Vector2D( 1, 3 ), color );
	Render::Engine::RectFilled( Vector2D( screen.x - 1, screen.y ), Vector2D( 3, 1 ), color );
}

void Visuals::RenderManual( ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( !g_pEngine->IsInGame( ) || !g_pEngine->IsConnected( ) || pLocal->IsDead( ) )
		return;

	//if( g_AntiAim.GetSide( ) == SIDE_MAX )
	//	return;

	if( !g_Vars.rage.anti_aim_manual || !g_Vars.rage.anti_aim_manual_arrows )
		return;

	auto vecScreenCenter = Render::GetScreenSize( ) / 2;

	// modify these
	const int nPaddingFromCenter{ g_Vars.rage.anti_aim_manual_arrows_spacing }; // { 60 }
	const int nSizeInPixels{ g_Vars.rage.anti_aim_manual_arrows_size }; // { 22 };

	// do not fuckign touch these
	const int nSize2{ nSizeInPixels / 2 };

	std::array<Vector2D, 9> vecArrows = {
		Vector2D( vecScreenCenter.x - nPaddingFromCenter, vecScreenCenter.y - nSize2 ), // left arrow top
		Vector2D( vecScreenCenter.x - nPaddingFromCenter, vecScreenCenter.y + nSize2 ), // left arrow bottom
		Vector2D( vecScreenCenter.x - ( nPaddingFromCenter + nSizeInPixels ), vecScreenCenter.y ), // left arrow middle

		Vector2D( vecScreenCenter.x + nPaddingFromCenter, vecScreenCenter.y - nSize2 ), // right arrow top
		Vector2D( vecScreenCenter.x + nPaddingFromCenter, vecScreenCenter.y + nSize2 ), // right arrow bottom
		Vector2D( vecScreenCenter.x + ( nPaddingFromCenter + nSizeInPixels ), vecScreenCenter.y ), // right arrow middle

		Vector2D( vecScreenCenter.x - nSize2, vecScreenCenter.y + nPaddingFromCenter ), // bottom arrow left
		Vector2D( vecScreenCenter.x + nSize2, vecScreenCenter.y + nPaddingFromCenter ), // bottom arrow right
		Vector2D( vecScreenCenter.x, vecScreenCenter.y + ( nPaddingFromCenter + nSizeInPixels ) ), // bottom arrow middle
	};

	const auto eSide = g_AntiAim.GetSide( );
	Render::Engine::FilledTriangle( vecArrows.at( 0 ), vecArrows.at( 1 ), vecArrows.at( 2 ), eSide == SIDE_LEFT ? g_Vars.rage.anti_aim_manual_arrows_color.ToRegularColor( ) : Color::Black( ).OverrideAlpha( 150 ) );
	Render::Engine::FilledTriangle( vecArrows.at( 3 ), vecArrows.at( 4 ), vecArrows.at( 5 ), eSide == SIDE_RIGHT ? g_Vars.rage.anti_aim_manual_arrows_color.ToRegularColor( ) : Color::Black( ).OverrideAlpha( 150 ) );
	Render::Engine::FilledTriangle( vecArrows.at( 6 ), vecArrows.at( 7 ), vecArrows.at( 8 ), eSide == SIDE_BACK ? g_Vars.rage.anti_aim_manual_arrows_color.ToRegularColor( ) : Color::Black( ).OverrideAlpha( 150 ) );
}

void Visuals::AutopeekIndicator( ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( !g_pEngine->IsInGame( ) || !g_pEngine->IsConnected( ) || pLocal->IsDead( ) )
		return;

	if( g_Movement.m_vecAutoPeekPos.IsZero( ) )
		return;

	
	Render::Engine::WorldCircle( g_Movement.m_vecAutoPeekPos, 15.f, Color( 0, 0, 0, 100 ), g_Vars.misc.autopeek_color.ToRegularColor( ) );
	Render::Engine::WorldCircle(g_Movement.m_vecAutoPeekPos, 15.f, Color(0, 0, 0, 0), g_Vars.misc.autopeek_color.ToRegularColor());
	

}

void Visuals::Draw( ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( !g_pEngine->IsInGame( ) || !g_pEngine->IsConnected( ) )
		return;

	visuals_config = &g_Vars.visuals_enemy;

	if( !visuals_config )
		return;

	if( g_Vars.esp.remove_scope ) {
		const auto pWeapon = reinterpret_cast< C_WeaponCSBaseGun * >( pLocal->m_hActiveWeapon( ).Get( ) );
		if( pWeapon ) {
			auto pWeaponData = pWeapon->GetCSWeaponData( );
			if( pWeaponData.IsValid( ) ) {
				if( pLocal->m_bIsScoped( ) && pWeaponData->m_iWeaponType == WEAPONTYPE_SNIPER_RIFLE ) {

					const auto screen = Render::GetScreenSize( );

					int w = screen.x,
						h = screen.y,
						x = w / 2,
						y = h / 2,
						size = g_Vars.cl_crosshair_sniper_width->GetInt( );

					if( size > 1 ) {
						x -= ( size / 2 );
						y -= ( size / 2 );
					}

					Render::Engine::RectFilled( 0, y, w, size, Color::Black( ) );
					Render::Engine::RectFilled( x, 0, size, h, Color::Black( ) );
				}
			}
		}
	}

	RenderManual( );
	DrawWatermark( );
	PenetrationCrosshair( );
	AutopeekIndicator( );

	g_ExtendedVisuals.Adjust( );

	g_Vars.globals.m_bInsideFireRange = false;

	std::vector< std::string > spectators{ };
	const auto local_observer = pLocal->m_hObserverTarget( );
	C_CSPlayer *spec = nullptr;
	if( local_observer.IsValid( ) ) {
		spec = ( C_CSPlayer * )g_pEntityList->GetClientEntityFromHandle( local_observer );
	}

	// main entity loop
	for( int i = 1; i <= g_pEntityList->GetHighestEntityIndex( ); ++i ) {
		auto entity = reinterpret_cast< C_BaseEntity * >( g_pEntityList->GetClientEntity( i ) );
		if( !entity ) {

			// reset if entity got invalid
			if( i <= 64 ) {
				g_ExtendedVisuals.m_arrWeaponInfos[ i ].first = 0;
				g_ExtendedVisuals.m_arrWeaponInfos[ i ].second = nullptr;
			}

			continue;
		}

		// let's not even bother if we are out of range
		if( i <= 64 ) {
			// convert entity to csplayer
			const auto player = ToCSPlayer( entity );

			if( player->IsTeammate( pLocal ) ) {
				bool bAllowPacketSendForThisEntity = !( g_pGlobalVars->tickcount % 4 );

				// hahahha this is so fucked
				static Vector vecPreviousOrigin[ 65 ];
				if( vecPreviousOrigin[ i ] != player->GetAbsOrigin( ) ) {
					bAllowPacketSendForThisEntity = true;
					vecPreviousOrigin[ i ] = player->GetAbsOrigin( );
				}


				// hahahha this is so fucked
				if( bAllowPacketSendForThisEntity ) {
				}
			}

			// so that it doesn't require a 2nd fucking loop..
			if( g_Vars.misc.spectators ) {
				if( player ) {
					player_info_t info;
					if( g_pEngine->GetPlayerInfo( i, &info ) ) {
						if( !player->IsDormant( ) && player->IsDead( ) && player->EntIndex( ) != pLocal->EntIndex( ) ) {
							std::string playerName{ info.szName };

							if( playerName.find( XorStr( "\n" ) ) != std::string::npos ) {
								playerName = XorStr( "[BLANK]" );
							}

							if( pLocal->IsDead( ) ) {
								auto observer = player->m_hObserverTarget( );
								if( local_observer.IsValid( ) && observer.IsValid( ) ) {
									auto target = reinterpret_cast< C_CSPlayer * >( g_pEntityList->GetClientEntityFromHandle( observer ) );

									if( target == spec && spec ) {
										spectators.push_back( playerName.substr( 0, 24 ) );
									}
								}
							}
							else {
								if( player->m_hObserverTarget( ) == pLocal ) {
									spectators.push_back( playerName.substr( 0, 24 ) );
								}
							}
						}
					}
				}
			}

			// handle player visuals
			if( IsValidPlayer( player ) ) {
				std::array<std::pair<Vector, bool>, 65> m_arrManualRestore;

				if( player->IsDormant( ) ) {
					if( g_ExtendedVisuals.m_arrOverridePlayers.at( i ).m_eOverrideType != EOverrideType::ESP_NONE ) {
						m_arrManualRestore.at( i ) = std::make_pair( player->m_vecOrigin( ), true );

						player->m_vecOrigin( ) = g_ExtendedVisuals.m_arrOverridePlayers.at( i ).m_vecOrigin;
						player->SetAbsOrigin( g_ExtendedVisuals.m_arrOverridePlayers.at( i ).m_vecOrigin );
					}
				}
				else {
					m_arrManualRestore.at( i ) = std::make_pair( Vector( ), false );
					g_ExtendedVisuals.m_arrOverridePlayers.at( i ).Reset( );
				}

				if( g_Vars.misc.ingame_radar )
					player->m_bSpotted( ) = true;

				if( visuals_config->dormant ) {
					// not dormant?
					if( !player->IsDormant( ) || g_ExtendedVisuals.m_arrSoundPlayers.at( i ).m_bValidSound ) {
						if( g_ExtendedVisuals.m_arrSoundPlayers.at( i ).m_bValidSound ) {
							// set full alpha if this was a sound based update
							player_fading_alpha.at( i ) = 1.f;
						}
						else {
							// increase alpha.
							player_fading_alpha.at( i ) += ( 5.f ) * g_pGlobalVars->frametime;
						}
					}
					else {
						if( player_fading_alpha.at( i ) < 0.6f ) {
							// decrease alpha FAST.
							player_fading_alpha.at( i ) -= ( 1.f ) * g_pGlobalVars->frametime;
						}
						else {
							// decrease alpha.
							player_fading_alpha.at( i ) -= ( 0.05f ) * g_pGlobalVars->frametime;
						}
					}
				}
				else {
					if( !player->IsDormant( ) ) {
						// increase alpha.
						player_fading_alpha.at( i ) += ( 5.f ) * g_pGlobalVars->frametime;
					}
					else {
						// decrease alpha FAST.
						player_fading_alpha.at( i ) -= ( 1.f ) * g_pGlobalVars->frametime;
					}
				}

				// now clamp it
				player_fading_alpha.at( i ) = std::clamp( player_fading_alpha.at( i ), 0.f, 1.0f );

				HandlePlayerVisuals( player );

				if( m_arrManualRestore.at( i ).second ) {
					player->m_vecOrigin( ) = m_arrManualRestore.at( i ).first;
					player->SetAbsOrigin( m_arrManualRestore.at( i ).first );
				}
			}
		}

		// handle world visuals
		if( IsValidEntity( entity ) ) {
			HandleWorldVisuals( entity );
		}

		g_GrenadeWarning.grenade_warning( entity );
	}

	if( g_Vars.misc.spectators ) {
		Spectators( spectators );
	}

	g_ExtendedVisuals.Restore( );
}

void Visuals::DrawWatermark( ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( pLocal->IsDead( ) ) {
		return;
	}

	C_WeaponCSBaseGun *pWeapon = ( C_WeaponCSBaseGun * )pLocal->m_hActiveWeapon( ).Get( );
	if( !pWeapon )
		return;

	if( !pWeapon->GetCSWeaponData( ).IsValid( ) )
		return;

	struct Indicator_t { Color color; std::string text; };
	std::vector< Indicator_t > indicators{ };

	static bool bRenderIndicator = false;
	if( g_Vars.misc.ping_spike && g_Vars.misc.ping_spike_key.enabled ) {
		bRenderIndicator = true;
	}

	if( g_Vars.rage_default.minimal_damage_override && g_Vars.rage_default.minimal_damage_override_key.enabled ) {
		Indicator_t ind{ };
		ind.color = Color::White( );
		ind.text = std::to_string( g_Vars.globals.m_nCurrentMinDmg );
		indicators.push_back( ind );
	}

	if( bRenderIndicator ) {
		auto netchannel = Encrypted_t<INetChannel>( g_pEngine->GetNetChannelInfo( ) );
		if( netchannel.IsValid( ) ) {
			const float flFakePing = netchannel->GetLatency( FLOW_INCOMING ) * 1000.f;
			const float flRealPing = netchannel->GetLatency( FLOW_OUTGOING ) * 1000.f;

			int nTargetPing = g_Vars.misc.ping_spike_amount;
			float flCorrect = std::clamp<float>( ( ( nTargetPing / 1000.f ) - netchannel->GetLatency( FLOW_OUTGOING ) - g_LagCompensation.GetLerp( ) ) * 1000.f, 0.f, 1000.f );
			const float flPingMultiplier = std::clamp<float>( flFakePing / flCorrect, 0.f, 1.f );

			Indicator_t ind{ };
			ind.color = Color::Blend( Color::Red( ), Color( 150, 200, 60 ), flPingMultiplier );
			ind.text = XorStr( "PING" );
			indicators.push_back( ind );

			// we disabled ping spike etc, but our ping obviously wont instantly reach our old ping.

			// check for ping multiplier between our ping and wish ping, if it's low enough and we aren't pingspiking
			// anymore we can disable the indicator. until then, show the user the current ping "spike" amount.
			if( fabs( flFakePing - flRealPing ) <= 50.f && !( g_Vars.misc.ping_spike && g_Vars.misc.ping_spike_key.enabled ) ) {
				bRenderIndicator = false;
			}
		}
	}

	if( g_Vars.rage.fake_lag ) {
		Indicator_t ind{ };
		ind.color = g_ServerAnimations.m_uServerAnimations.m_bBreakingTeleportDst ? Color( 150, 200, 60 ) : Color( 255, 0, 0 );
		ind.text = XorStr( "LC" );

		if( g_ServerAnimations.m_uServerAnimations.m_bBreakingTeleportDst || pLocal->m_vecVelocity( ).Length2D( ) > 260.f )
			indicators.push_back( ind );
	}

	if( g_Vars.rage.anti_aim_fake_body ) {
		float change = std::abs( Math::AngleNormalize( g_ServerAnimations.m_uServerAnimations.m_flLowerBodyYawTarget - g_ServerAnimations.m_uServerAnimations.m_flEyeYaw ) );

		Indicator_t ind{ };
		ind.color = ( change > 35.f || g_TickbaseController.m_bRunningExploit ) ? Color( 150, 200, 60 ) : Color( 255, 0, 0 );
		ind.text = XorStr( "LBY" );
		indicators.push_back( ind );
	}

	if( g_Vars.misc.low_fps_warning ) {
		const float nFrameRate = float( 1.f / g_pGlobalVars->frametime );
		const float nTickRate = float( 1.f / g_pGlobalVars->interval_per_tick );
		const float flFrameRateMultiplier = std::clamp( float( nTickRate - nFrameRate ) / nTickRate, 0.f, 1.0f );

		static float flLastLowFPSTime = 0.f;

		if( nFrameRate < nTickRate ) {
			flLastLowFPSTime = g_pGlobalVars->curtime;
		}

		if( fabs( flLastLowFPSTime - g_pGlobalVars->curtime ) < 0.5f ) {
			Indicator_t ind{ };
			ind.color = Color::Blend( Color( 150, 200, 60 ), Color::Red( ), flFrameRateMultiplier );
			ind.text = XorStr( "FPS" );
			indicators.push_back( ind );
		}
	}

	// temp
	/*auto GetRotatedPos( Vector start, const float rotation, const float distance ) -> Vector {
		const auto rad = DEG2RAD( rotation );
		start.x += cosf( rad ) * distance;
		start.y += sinf( rad ) * distance;

		return start;
	};

	Vector2D screen1, screen2;
	if( Render::Engine::WorldToScreen( pLocal->GetAbsOrigin( ), screen1 ) ) {
		if( Render::Engine::WorldToScreen( GetRotatedPos( origin, g_ServerAnimations.m_uServerAnimations.m_flEyeYaw, 25.f ), screen2 ) ) {
			Render::Engine::Line( screen1.x, screen1.y, screen2.x, screen2.y, Color( 255, 255, 255 ) );
			Render::Engine::esp_bold.string( screen2.x, screen2.y, Color::White( ), "real" );
		}

		if( Render::Engine::WorldToScreen( GetRotatedPos( pLocal->GetAbsOrigin( ), g_ServerAnimations.m_uServerAnimations.m_flLowerBodyYawTarget, 25.f ), screen2 ) ) {
			Render::Engine::Line( screen1.x, screen1.y, screen2.x, screen2.y, Color( 255, 255, 255 ) );
			Render::Engine::esp_bold.string( screen2.x, screen2.y, Color::White( ), "lby" );
		}
	}*/

	if( indicators.empty( ) )
		return;

	// iterate and draw indicators.
	for( size_t i{ }; i < indicators.size( ); ++i ) {
		auto &indicator = indicators[ i ];

		Render::Engine::esp_indicator.string( 12, Render::Engine::m_height - 80 - ( 30 * i ), indicator.color, indicator.text );
	}

	if( !g_Vars.rage.debug_logs )
		return;

	if( debug_messages_sane.empty( ) ) {
		return;
	}

	Vector2D screen = Render::GetScreenSize( );
	for( int i = 0; i < debug_messages_sane.size( ); ++i ) {
		auto msg = debug_messages_sane[ i ];
		auto width = Render::Engine::esp_bold.size( msg ).m_width;
		auto height = Render::Engine::esp_bold.size( msg ).m_height;

		Render::Engine::menu_regular.string( screen.x - 9 - width - 60, 2 + ( height * i ), Color( 255, 255, 255, 220 ), msg );
	}
}

void Visuals::AddDebugMessage( std::string msg ) {
	bool dont = false;

	if( debug_messages.size( ) ) {
		for( int i = 0; i < debug_messages.size( ); ++i ) {
			auto msgs = debug_messages[ i ];

			if( msgs.find( msg ) != std::string::npos )
				dont = true;
		}
	}

	if( dont ) {
		return;
	}

	debug_messages.push_back( msg );
}

void ExtendedVisuals::NormalizeSound( C_CSPlayer *player, SndInfo_t &sound ) {
	if( !player || !&sound || !sound.m_pOrigin )
		return;

	Vector src3D, dst3D;
	CGameTrace tr;
	Ray_t ray;
	CTraceFilter filter;

	filter.pSkip = player;
	src3D = ( *sound.m_pOrigin ) + Vector( 0, 0, 1 );
	dst3D = src3D - Vector( 0, 0, 100 );
	ray.Init( src3D, dst3D );

	g_pEngineTrace->TraceRay( ray, MASK_PLAYERSOLID, &filter, &tr );

	// step = (tr.fraction < 0.20)
	// shot = (tr.fraction > 0.20)
	// stand = (tr.fraction > 0.50)
	// crouch = (tr.fraction < 0.50)

	// Player stuck, idk how this happened
	if( tr.allsolid ) {
		m_arrSoundPlayers.at( sound.m_nSoundSource ).m_iReceiveTime = -1;

		m_arrSoundPlayers.at( sound.m_nSoundSource ).m_vecOrigin = *sound.m_pOrigin;
		m_arrSoundPlayers.at( sound.m_nSoundSource ).m_nFlags = player->m_fFlags( );
	}
	else {
		m_arrSoundPlayers.at( sound.m_nSoundSource ).m_vecOrigin = ( tr.fraction < 0.97 ? tr.endpos : *sound.m_pOrigin );
		m_arrSoundPlayers.at( sound.m_nSoundSource ).m_nFlags = player->m_fFlags( );
		m_arrSoundPlayers.at( sound.m_nSoundSource ).m_nFlags |= ( tr.fraction < 0.50f ? FL_DUCKING : 0 ) | ( tr.fraction != 1 ? FL_ONGROUND : 0 );
		m_arrSoundPlayers.at( sound.m_nSoundSource ).m_nFlags &= ( tr.fraction > 0.50f ? ~FL_DUCKING : 0 ) | ( tr.fraction == 1 ? ~FL_ONGROUND : 0 );
	}
}

// todo - maxwell; fix this. https://i.imgur.com/T5q222e.png
bool ExtendedVisuals::ValidateSound( SndInfo_t &sound ) {
	if( !sound.m_bFromServer )
		return false;

	for( int i = 0; i < m_vecSoundBuffer.Count( ); i++ ) {
		const SndInfo_t &cachedSound = m_vecSoundBuffer[ i ];
		// was this sound already cached/processed?
		if( cachedSound.m_nGuid == sound.m_nGuid ) {
			return false;
		}
	}

	return true;
}

void ExtendedVisuals::Adjust( ) {
	if( !g_Vars.visuals_enemy.dormant )
		return;

	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	for( int i = 1; i <= g_pGlobalVars->maxClients; ++i ) {
		C_CSPlayer *player = C_CSPlayer::GetPlayerByIndex( i );
		if( !player ) {
			continue;
		}

		if( g_ExtendedVisuals.m_arrOverridePlayers.at( i ).m_eOverrideType == EOverrideType::ESP_SHARED )
			continue;

		// only grab info from dead players who are spectating others
		if( !player->IsDead( ) ) {
			continue;
		}

		// we can grab info from this spectator
		if( player->IsDormant( ) ) {
			continue;
		}

		// this player is spectating someone
		if( player->m_hObserverTarget( ).IsValid( ) ) {
			// grab the player who is being spectated (pSpectatee) by the spectator (player)
			const auto pSpectatee = ( C_CSPlayer * )player->m_hObserverTarget( ).Get( );
			if( pSpectatee && pSpectatee->EntIndex( ) <= 64 ) {
				// if the guy that's being spectated is dormant, set his origin
				// to the spectators origin (and ofc set dormancy state for esp to draw him)
				if( pSpectatee->IsDormant( ) ) {
					// make sure they're actually spectating them instead of flying around or smth
					if( player->m_iObserverMode( ) == 4 || player->m_iObserverMode( ) == 5 ) {
						m_arrOverridePlayers.at( pSpectatee->EntIndex( ) ).m_eOverrideType = EOverrideType::ESP_SPECTATOR;

						//std::memcpy( m_arrOverridePlayers.at( pSpectatee->EntIndex( ) ).m_flPoseParameters, player->m_flPoseParameter( ), sizeof( player->m_flPoseParameter( ) ) );
						m_arrOverridePlayers.at( pSpectatee->EntIndex( ) ).m_vecOrigin = player->m_vecOrigin( );
					}
				}
			}
		}
	}

	CUtlVector<SndInfo_t> m_vecCurSoundList;
	g_pEngineSound->GetActiveSounds( m_vecCurSoundList );

	// No active sounds.
	if( !m_vecCurSoundList.Count( ) )
		return;

	for( int i = 0; i < m_vecCurSoundList.Count( ); i++ ) {
		SndInfo_t &sound = m_vecCurSoundList[ i ];
		if( sound.m_nSoundSource == 0 || // World
			sound.m_nSoundSource > 64 )   // Most likely invalid
			continue;

		C_CSPlayer *player = C_CSPlayer::GetPlayerByIndex( sound.m_nSoundSource );

		if( !player || !sound.m_pOrigin || !player->IsPlayer( ) || player == pLocal || player->IsTeammate( pLocal ) || sound.m_pOrigin->IsZero( ) )
			continue;

		// we have a valid spectator dormant player entry
		if( m_arrOverridePlayers.at( player->EntIndex( ) ).m_eOverrideType != EOverrideType::ESP_NONE ) {
			// no need to do anything else for this player
			continue;
		}

		if( !ValidateSound( sound ) )
			continue;

		NormalizeSound( player, sound );

		m_arrSoundPlayers.at( sound.m_nSoundSource ).Override( sound );
	}

	for( int i = 1; i <= g_pGlobalVars->maxClients; ++i ) {
		const auto player = C_CSPlayer::GetPlayerByIndex( i );
		if( !player || player->IsDead( ) || !player->IsDormant( ) ) {
			// notify visuals that this target is officially not dormant..
			m_arrSoundPlayers.at( i ).m_bValidSound = false;
			m_arrOverridePlayers.at( i ).m_eOverrideType = EOverrideType::ESP_NONE;
			continue;
		}

		constexpr int EXPIRE_DURATION = 450;
		auto &soundPlayer = m_arrSoundPlayers.at( player->EntIndex( ) );

		bool bSoundExpired = GetTickCount( ) - soundPlayer.m_iReceiveTime > EXPIRE_DURATION;
		if( bSoundExpired ) {
			continue;
		}

		// first backup the player
		SoundPlayer backupPlayer;
		backupPlayer.m_iIndex = player->m_entIndex;
		backupPlayer.m_nFlags = player->m_fFlags( );
		backupPlayer.m_vecOrigin = player->m_vecOrigin( );
		backupPlayer.m_vecAbsOrigin = player->GetAbsOrigin( );

		m_vecRestorePlayers.emplace_back( backupPlayer );

		// notify visuals that this target is officially dormant but we found a sound..
		soundPlayer.m_bValidSound = true;

		// set stuff accordingly.
		player->m_fFlags( ) = soundPlayer.m_nFlags;
		player->m_vecOrigin( ) = soundPlayer.m_vecOrigin;
		player->SetAbsOrigin( soundPlayer.m_vecOrigin );
	}

	// copy sounds (cache)
	m_vecSoundBuffer = m_vecCurSoundList;
}

void ExtendedVisuals::Restore( ) {
	if( m_vecRestorePlayers.empty( ) )
		return;

	for( auto &restorePlayer : m_vecRestorePlayers ) {
		auto player = C_CSPlayer::GetPlayerByIndex( restorePlayer.m_iIndex );
		if( !player || player->IsDormant( ) )
			continue;

		player->m_fFlags( ) = restorePlayer.m_nFlags;
		player->m_vecOrigin( ) = restorePlayer.m_vecOrigin;
		player->SetAbsOrigin( restorePlayer.m_vecAbsOrigin );

		m_arrSoundPlayers.at( restorePlayer.m_iIndex ).m_bValidSound = false;
	}

	m_vecRestorePlayers.clear( );
}