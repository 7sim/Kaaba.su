#include "GrenadeWarning.hpp"
#include "../Rage/Autowall.hpp"
#include "../Visuals/Visuals.hpp"
#include "../Visuals/GrenadePrediction.hpp"

GrenadeWarning g_GrenadeWarning;

bool GrenadeWarning::data_t::physics_simulate( ) {
	if( m_detonated )
		return true;

	static const auto sv_gravity = g_pCVar->FindVar( "sv_gravity" );

	const auto new_velocity_z = m_velocity.z - ( sv_gravity->GetFloat( ) * 0.4f ) * g_pGlobalVars->interval_per_tick;

	const auto move = Vector(
		m_velocity.x * g_pGlobalVars->interval_per_tick,
		m_velocity.y * g_pGlobalVars->interval_per_tick,
		( m_velocity.z + new_velocity_z ) / 2.f * g_pGlobalVars->interval_per_tick
	);

	m_velocity.z = new_velocity_z;

	auto trace = CGameTrace( );

	physics_push_entity( move, trace );

	if( m_detonated )
		return true;

	if( trace.fraction != 1.f ) {
		update_path< true >( );

		perform_fly_collision_resolution( trace );
	}

	return false;
}

void GrenadeWarning::data_t::physics_trace_entity( const Vector &src, const Vector &dst, std::uint32_t mask, CGameTrace &trace ) {
	TraceHull(
		src, dst, { -2.f, -2.f, -2.f }, { 2.f, 2.f, 2.f },
		mask, m_owner, m_collision_group, &trace
	);

	if( trace.startsolid && ( trace.contents & CONTENTS_CURRENT_90 ) ) {
		//trace.clear( );

		TraceHull(
			src, dst, { -2.f, -2.f, -2.f }, { 2.f, 2.f, 2.f },
			mask & ~CONTENTS_CURRENT_90, m_owner, m_collision_group, &trace
		);
	}

	if( !trace.DidHit( )
		|| !trace.hit_entity
		|| !reinterpret_cast< C_CSPlayer * >( trace.hit_entity )->IsPlayer( ) )
		return;

	//trace.clear( );

	TraceLine( src, dst, mask, m_owner, m_collision_group, &trace );
}

void GrenadeWarning::data_t::physics_push_entity( const Vector &push, CGameTrace &trace ) {
	physics_trace_entity( m_origin, m_origin + push,
						  m_collision_group == COLLISION_GROUP_DEBRIS
						  ? ( MASK_SOLID | CONTENTS_CURRENT_90 ) & ~CONTENTS_MONSTER
						  : MASK_SOLID | CONTENTS_OPAQUE | CONTENTS_IGNORE_NODRAW_OPAQUE | CONTENTS_CURRENT_90 | CONTENTS_HITBOX,
						  trace
	);

	if( trace.startsolid ) {
		m_collision_group = COLLISION_GROUP_INTERACTIVE_DEBRIS;

		TraceLine(
			m_origin - push, m_origin + push,
			( MASK_SOLID | CONTENTS_CURRENT_90 ) & ~CONTENTS_MONSTER,
			m_owner, m_collision_group, &trace
		);
	}

	if( trace.fraction ) {
		m_origin = trace.endpos;
	}

	if( !trace.hit_entity )
		return;

	if( reinterpret_cast< C_CSPlayer * >( trace.hit_entity )->IsPlayer( )
		|| m_index != WEAPON_TAGRENADE && m_index != WEAPON_MOLOTOV && m_index != WEAPON_INC )
		return;

	static const auto weapon_molotov_maxdetonateslope = g_pCVar->FindVar( "weapon_molotov_maxdetonateslope" );

	if( m_index != WEAPON_TAGRENADE
		&& trace.plane.normal.z < std::cos( DEG2RAD( weapon_molotov_maxdetonateslope->GetFloat( ) ) ) )
		return;

	detonate< true >( );
}

void GrenadeWarning::data_t::perform_fly_collision_resolution( CGameTrace &trace ) {
	auto surface_elasticity = 1.f;

	if( trace.hit_entity ) {
	#if 0 /* ayo reis wtf */
		if( const auto v8 = trace.m_surface.m_name ) {
			if( *( DWORD * )v8 != 'spam' || *( ( DWORD * )v8 + 1 ) != '_sc/' ) {
				if( *( ( DWORD * )v8 + 1 ) == '_ed/'
					&& *( ( DWORD * )v8 + 2 ) == 'ekal'
					&& *( ( DWORD * )v8 + 3 ) == 'alg/'
					&& *( ( DWORD * )v8 + 4 ) == 'g/ss'
					&& *( ( DWORD * )v8 + 5 ) == 'ssal' ) {
					goto BREAKTROUGH;
				}
			}
			else if( *( ( DWORD * )v8 + 2 ) == 'iffo'
					 && *( ( DWORD * )v8 + 3 ) == 'g/ec'
					 && *( ( DWORD * )v8 + 4 ) == 'ssal'
					 && *( ( DWORD * )v8 + 5 ) == 'bru/'
					 && *( ( DWORD * )v8 + 6 ) == 'g_na'
					 && *( ( DWORD * )v8 + 7 ) == 'ssal' ) {
				goto BREAKTROUGH;
			}
		}
	#endif
		if( Autowall::IsBreakable( reinterpret_cast< C_CSPlayer * >( trace.hit_entity ) ) ) {
		BREAKTROUGH:
			m_velocity *= 0.4f;

			return;
		}

		const auto is_player = reinterpret_cast< C_CSPlayer * >( trace.hit_entity )->IsPlayer( );
		if( is_player ) {
			surface_elasticity = 0.3f;
		}

		if( trace.hit_entity->EntIndex( ) ) {
			if( is_player
				&& m_last_hit_entity == trace.hit_entity ) {
				m_collision_group = COLLISION_GROUP_DEBRIS;

				return;
			}

			m_last_hit_entity = trace.hit_entity;
		}
	}

	auto velocity = Vector( );

	const auto back_off = m_velocity.Dot( trace.plane.normal ) * 2.f;

	for( auto i = 0u; i < 3u; i++ ) {
		const auto change = trace.plane.normal[ i ] * back_off;

		velocity[ i ] = m_velocity[ i ] - change;

		if( std::fabs( velocity[ i ] ) >= 1.f )
			continue;

		velocity[ i ] = 0.f;
	}

	velocity *= std::clamp< float >( surface_elasticity * 0.45f, 0.f, 0.9f );

	if( trace.plane.normal.z > 0.7f ) {
		const auto speed_sqr = velocity.LengthSquared( );
		if( speed_sqr > 96000.f ) {
			const auto l = velocity.Normalized( ).Dot( trace.plane.normal );
			if( l > 0.5f ) {
				velocity *= 1.f - l + 0.5f;
			}
		}

		if( speed_sqr < 400.f ) {
			m_velocity = Vector( 0, 0, 0 );
		}
		else {
			m_velocity = velocity;

			physics_push_entity( velocity * ( ( 1.f - trace.fraction ) * g_pGlobalVars->interval_per_tick ), trace );
		}
	}
	else {
		m_velocity = velocity;

		physics_push_entity( velocity * ( ( 1.f - trace.fraction ) * g_pGlobalVars->interval_per_tick ), trace );
	}

	if( m_bounces_count > 20 )
		return detonate< false >( );

	++m_bounces_count;
}

void GrenadeWarning::data_t::think( ) {
	switch( m_index ) {
		case WEAPON_SMOKEGRENADE:
			if( m_velocity.LengthSquared( ) <= 0.01f ) {
				detonate< false >( );
			}

			break;
		case WEAPON_DECOY:
			if( m_velocity.LengthSquared( ) <= 0.04f ) {
				detonate< false >( );
			}

			break;
		case WEAPON_FLASHBANG:
		case WEAPON_HEGRENADE:
		case WEAPON_MOLOTOV:
		case WEAPON_INC:
			if( TICKS_TO_TIME( m_tick ) > m_detonate_time ) {
				detonate< false >( );
			}

			break;
	}

	m_next_think_tick = m_tick + TIME_TO_TICKS( 0.2f );
}

void GrenadeWarning::data_t::predict( const Vector &origin, const Vector &velocity, float throw_time, int offset ) {
	m_origin = origin;
	m_velocity = velocity;
	m_collision_group = COLLISION_GROUP_PROJECTILE;

	const auto tick = TIME_TO_TICKS( 1.f / 30.f );

	m_last_update_tick = -tick;

	switch( m_index ) {
		case WEAPON_SMOKEGRENADE: m_next_think_tick = TIME_TO_TICKS( 1.5f ); break;
		case WEAPON_DECOY: m_next_think_tick = TIME_TO_TICKS( 2.f ); break;
		case WEAPON_FLASHBANG:
		case WEAPON_HEGRENADE:
			m_detonate_time = 1.5f;
			m_next_think_tick = TIME_TO_TICKS( 0.02f );

			break;
		case WEAPON_MOLOTOV:
		case WEAPON_INC:
			static const auto molotov_throw_detonate_time = g_pCVar->FindVar( "molotov_throw_detonate_time" );

			m_detonate_time = molotov_throw_detonate_time->GetFloat( );
			m_next_think_tick = TIME_TO_TICKS( 0.02f );

			break;
	}

	//float time = delta.Length( );
	//m_tick += TIME_TO_TICKS( time );

	for( ; m_tick < TIME_TO_TICKS( 60.f ); ++m_tick ) {
		if( m_next_think_tick <= m_tick ) {
			think( );
		}

		if( m_tick < offset )
			continue;

		if( physics_simulate( ) )
			break;

		if( m_last_update_tick + tick > m_tick )
			continue;

		update_path< false >( );
	}

	if( m_last_update_tick + tick <= m_tick ) {
		update_path< false >( );
	}

	m_expire_time = throw_time + TICKS_TO_TIME( m_tick );
}

bool GrenadeWarning::data_t::draw( ) const {
	if( m_path.size( ) <= 1u
		|| g_pGlobalVars->curtime >= m_expire_time )
		return false;

	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return false;

	float distance = pLocal->m_vecOrigin( ).Distance( m_origin ) / 12;

	if( distance > 200.f )
		return false;

	static int iScreenWidth, iScreenHeight;
	g_pEngine->GetScreenSize( iScreenWidth, iScreenHeight );

	Vector2D prev_screen = { };
	auto prev_on_screen = Render::Engine::WorldToScreen( std::get< Vector >( m_path.front( ) ), prev_screen );

	for( auto i = 1u; i < m_path.size( ); ++i ) {
		Vector2D cur_screen = { }, last_cur_screen = { };
		const auto cur_on_screen = Render::Engine::WorldToScreen( std::get< Vector >( m_path.at( i ) ), cur_screen );

		if( prev_on_screen && cur_on_screen ) {
			if( g_Vars.esp.grenade_proximity_warning ) {
				auto color = g_Vars.esp.grenade_proximity_warning_color.ToRegularColor( );

				Render::Engine::Line( ( int )prev_screen.x, ( int )prev_screen.y, ( int )cur_screen.x, ( int )cur_screen.y, g_Vars.esp.grenade_proximity_warning_color.ToRegularColor( ) );
			}
		}

		prev_screen = cur_screen;
		prev_on_screen = cur_on_screen;
	}

	const Vector center = pLocal->WorldSpaceCenter( );
	const Vector delta = center - m_origin;

	static CTraceFilter filter{ };
	CGameTrace trace;
	filter.pSkip = m_nade_entity;

	float flWarningMultiplier = 0.f;
	if( m_index == WEAPON_HEGRENADE ) {
		if( delta.Length( ) <= 475.f ) {
			g_pEngineTrace->TraceRay( Ray_t( m_origin, center ), MASK_SHOT, ( ITraceFilter * )&filter, &trace );

			if( trace.hit_entity && trace.hit_entity == pLocal ) {
				static float a = 105.0f;
				static float b = 25.0f;
				static float c = 140.0f;

				float d = ( ( ( ( pLocal->m_vecOrigin( ) ) - m_origin ).Length( ) - b ) / c );
				float flDamage = a * exp( -d * d );

				flDamage = std::max( static_cast< int >( ceilf( g_GrenadePrediction.CSGO_Armor( flDamage, pLocal->m_ArmorValue( ) ) ) ), 0 );

				// * 0.75 because we want some tolerance incase this nade will do
				// significant damage to us
				flWarningMultiplier = flDamage / ( pLocal->m_iHealth( ) * 0.75f );
				flWarningMultiplier = std::clamp<float>( flWarningMultiplier, 0.f, 1.f );
			}
		}
	}
	else if( ( m_index == WEAPON_MOLOTOV || m_index == WEAPON_INC ) ) {
		const float flMinDistance = 131.f;
		if( delta.Length( ) <= flMinDistance ) {
			flWarningMultiplier = delta.Length( ) / ( flMinDistance * 0.65f );
		}
	}

	const float flPercent = ( ( m_expire_time - g_pGlobalVars->curtime ) / TICKS_TO_TIME( m_tick ) );
	const auto strIcon = g_Visuals.GetWeaponIcon( m_index );
	const auto vecIconSize = Render::Engine::cs_large.size( g_Visuals.GetWeaponIcon( m_index ) );

	Render::Engine::CircleFilled( prev_screen.x, prev_screen.y - ( vecIconSize.m_height / 2 ), 17.f, 130, Color::Blend( Color::Black( ), Color::Red( ), flWarningMultiplier ).OverrideAlpha( 200 ) );
	Render::Engine::cs_large.string( prev_screen.x - ( vecIconSize.m_width / 2.f ), prev_screen.y - vecIconSize.m_height, Color::White( ).OverrideAlpha( 50 ), strIcon );

	g_pSurface->DrawSetColor( Color::Black( ) );
	g_pSurface->DrawOutlinedCircle( prev_screen.x, prev_screen.y - ( vecIconSize.m_height / 2 ), 17.f, 130 );

	Render::Engine::SetClip( { prev_screen.x - ( vecIconSize.m_width / 2.f ), prev_screen.y - ( vecIconSize.m_height * flPercent ) }, Vector2D( vecIconSize.m_width, vecIconSize.m_height ) );
	Render::Engine::cs_large.string( prev_screen.x - ( vecIconSize.m_width / 2.f ), prev_screen.y - vecIconSize.m_height, Color::White( ).OverrideAlpha( 210 ), strIcon );
	Render::Engine::ResetClip( );

	// no need to warn the player
	// of non-dangerous nades
	if( flWarningMultiplier <= 0.1f ) {
		return true;
	}

	Vector2D screenPos;
	Vector vLocalOrigin = pLocal->GetAbsOrigin( );

	if( pLocal->IsDead( ) )
		vLocalOrigin = g_pInput->m_vecCameraOffset;

	// give some extra room for screen position to be off screen.
	const float flExtraRoom = iScreenWidth / 18.f;

	if( !Render::Engine::WorldToScreen( m_origin, screenPos )
		|| screenPos.x < -flExtraRoom
		|| screenPos.x >( iScreenWidth + flExtraRoom )
		|| screenPos.y < -flExtraRoom
		|| screenPos.y >( iScreenHeight + flExtraRoom ) ) {
		QAngle angViewAngles;
		g_pEngine.Xor( )->GetViewAngles( angViewAngles );

		const Vector2D vecScreenCenter = Vector2D( iScreenWidth * .5f, iScreenHeight * .5f );
		const float flAngleYaw = DEG2RAD( angViewAngles.y - Math::CalcAngle( vLocalOrigin, m_origin, true ).y - 90 );

		const float flNewPointX = ( vecScreenCenter.x + ( ( ( iScreenHeight - 60.f ) / 2 ) ) * cos( flAngleYaw ) ) + 8.f;
		const float flNewPointY = ( vecScreenCenter.y + ( ( ( iScreenHeight - 60.f ) / 2 ) ) * sin( flAngleYaw ) ) + 8.f;

		Render::Engine::CircleFilled( flNewPointX, flNewPointY - ( vecIconSize.m_height / 2 ), 17.f, 130, Color::Blend( Color::Black( ), Color::Red( ), flWarningMultiplier ).OverrideAlpha( 200 ) );
		Render::Engine::cs_large.string( flNewPointX - ( vecIconSize.m_width / 2.f ), flNewPointY - vecIconSize.m_height, Color::White( ).OverrideAlpha( 50 ), strIcon );

		Render::Engine::SetClip( { flNewPointX - ( vecIconSize.m_width / 2.f ), flNewPointY - ( vecIconSize.m_height * flPercent ) }, Vector2D( vecIconSize.m_width, vecIconSize.m_height ) );
		Render::Engine::cs_large.string( flNewPointX - ( vecIconSize.m_width / 2.f ), flNewPointY - vecIconSize.m_height, Color::White( ).OverrideAlpha( 210 ), strIcon );
		Render::Engine::ResetClip( );
	}

	return true;
}

void GrenadeWarning::grenade_warning( C_BaseEntity *entity ) {
	auto &predicted_nades = get_list( );

	static auto last_server_tick = g_pGlobalVars->curtime;
	if( last_server_tick != g_pGlobalVars->curtime ) {
		predicted_nades.clear( );

		last_server_tick = g_pGlobalVars->curtime;
	}

	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( !entity || entity->IsDormant( ) || !g_Vars.esp.grenade_proximity_warning )
		return;

	const auto client_class = entity->GetClientClass( );
	if( !client_class
		|| client_class->m_ClassID != CMolotovProjectile && client_class->m_ClassID != CBaseCSGrenadeProjectile )
		return;

	if( client_class->m_ClassID == CBaseCSGrenadeProjectile ) {
		const auto model = entity->GetModel( );
		if( !model )
			return;

		const auto studio_model = g_pModelInfo->GetStudiomodel( model );
		if( !studio_model
			|| std::string_view( studio_model->szName ).find( "fraggrenade" ) == std::string::npos )
			return;
	}

	const auto handle = ( unsigned long )entity->GetRefEHandle( ).Get( );

	// i'm so sry
	static auto m_nExplodeEffectTickBeginOffset = Engine::g_PropManager.GetOffset( XorStr( "DT_BaseCSGrenadeProjectile" ), XorStr( "m_nExplodeEffectTickBegin" ) );
	static auto m_hThrowerOffset = Engine::g_PropManager.GetOffset( XorStr( "DT_BaseCSGrenadeProjectile" ), XorStr( "m_hThrower" ) );

	auto m_nExplodeEffectTickBegin = 0;// *( int* )( uintptr_t( entity ) + m_nExplodeEffectTickBeginOffset );
	auto m_hThrower = *( CBaseHandle * )( uintptr_t( entity ) + m_hThrowerOffset );

	if( !m_hThrower.IsValid( ) || !m_hThrower.Get( ) ) {
		predicted_nades.erase( handle );
		return;
	}

	auto pThrowerEntity = ( ( C_CSPlayer * )m_hThrower.Get( ) );
	if( !pThrowerEntity ) {
		predicted_nades.erase( handle );
		return;
	}

	if( m_nExplodeEffectTickBegin || ( pThrowerEntity->IsTeammate( pLocal ) && pThrowerEntity->EntIndex( ) != pLocal->EntIndex( ) ) ) {
		predicted_nades.erase( handle );
		return;
	}

	if( predicted_nades.find( handle ) == predicted_nades.end( ) ) {
		float flSpawnTime = *reinterpret_cast< float * >( uintptr_t( entity ) + 0x02D8 );

		for( auto a : g_GrenadeWarning.m_vecEventNades ) {
			if( a.first == ( ( C_BaseEntity * )m_hThrower.Get( ) )->EntIndex( ) ) {
				flSpawnTime = a.second;
			}
		}

		predicted_nades[ handle ] = { ( C_CSPlayer * )m_hThrower.Get( ),
			client_class->m_ClassID == CMolotovProjectile ? WEAPON_MOLOTOV : WEAPON_HEGRENADE,
			entity->m_vecOrigin( ), reinterpret_cast< C_CSPlayer * >( entity )->m_vecVelocity( ),
			flSpawnTime,
			TIME_TO_TICKS( reinterpret_cast< C_CSPlayer * >( entity )->m_flSimulationTime( ) - flSpawnTime ), entity };
	}

	if( predicted_nades.at( handle ).draw( ) )
		return;

	predicted_nades.erase( handle );
}