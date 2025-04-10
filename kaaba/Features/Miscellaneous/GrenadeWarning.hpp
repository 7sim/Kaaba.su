#pragma once
#include "../../source.hpp"
#include "../../SDK/Classes/Player.hpp"

class GrenadeWarning {
public:
	__forceinline static void TraceHull( const Vector& src, const Vector& dst, const Vector& mins, const Vector& maxs, int mask, IHandleEntity* entity, int collision_group, CGameTrace* trace ) {
		static auto trace_filter_simple = Memory::Scan( "client.dll", ( "55 8B EC 83 E4 F0 83 EC 7C 56 52" ) ) + 0x3D;

		std::uintptr_t filter[ 4 ] = {
			*reinterpret_cast< std::uintptr_t* >( trace_filter_simple ),
			reinterpret_cast< std::uintptr_t >( entity ),
			collision_group,
			0
		};

		auto ray = Ray_t( );

		ray.Init( src, dst, mins, maxs );

		g_pEngineTrace->TraceRay( ray, mask, reinterpret_cast< CTraceFilter* >( &filter ), trace );
	}

	__forceinline static void TraceLine( const Vector& src, const Vector& dst, int mask, IHandleEntity* entity, int collision_group, CGameTrace* trace ) {
		static auto trace_filter_simple = Memory::Scan( "client.dll", ( "55 8B EC 83 E4 F0 83 EC 7C 56 52" ) ) + 0x3D;

		std::uintptr_t filter[ 4 ] = {
			*reinterpret_cast< std::uintptr_t* >( trace_filter_simple ),
			reinterpret_cast< std::uintptr_t >( entity ),
			collision_group,
			0
		};

		auto ray = Ray_t( );

		ray.Init( src, dst );

		g_pEngineTrace->TraceRay( ray, mask, reinterpret_cast< CTraceFilter* >( &filter ), trace );
	}

	struct data_t {
		__forceinline data_t( ) = default;

		__forceinline data_t( C_CSPlayer* owner, int index, const Vector& origin, const Vector& velocity, float throw_time, int offset, C_BaseEntity* ent ) : data_t( ) {
			m_owner = owner;
			m_index = index;
			m_nade_entity = ent;

			predict( origin, velocity, throw_time, offset );
		}

		bool physics_simulate( );

		void physics_trace_entity( const Vector& src, const Vector& dst, std::uint32_t mask, CGameTrace& trace );

		void physics_push_entity( const Vector& push, CGameTrace& trace );

		void perform_fly_collision_resolution( CGameTrace& trace );

		void think( );

		template < bool _bounced >
		__forceinline void detonate( ) {
			m_detonated = true;

			update_path< _bounced >( );
		}

		template < bool _bounced >
		__forceinline void update_path( ) {
			m_last_update_tick = m_tick;

			m_path.emplace_back( m_origin, _bounced );
		}

		void predict( const Vector& origin, const Vector& velocity, float throw_time, int offset );

		bool draw( ) const;

		bool                                            m_detonated{ };
		C_CSPlayer* m_owner{ };
		Vector                                            m_origin{ }, m_velocity{ };
		IClientEntity* m_last_hit_entity{ };
		C_BaseEntity* m_nade_entity{ };
		int                                m_collision_group{ };
		float                                            m_detonate_time{ }, m_expire_time{ };
		int                                                m_index{ }, m_tick{ }, m_next_think_tick{ },
			m_last_update_tick{ }, m_bounces_count{ };
		std::vector< std::pair< Vector, bool > >        m_path{ };
	} m_data{ };

	std::unordered_map< unsigned long, data_t > m_list{ };
public:
	const data_t& get_local_data( ) const {
		return m_data;
	}

	std::unordered_map< unsigned long, data_t >& get_list( ) {
		return m_list;
	}

	std::vector<std::pair<int, float>> m_vecEventNades;

	void grenade_warning( C_BaseEntity* entity );
};

extern GrenadeWarning g_GrenadeWarning;