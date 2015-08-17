#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/AutoReference.h>
#include <interfaces.hpp>
#include <lua.hpp>
#include <cstdint>
#include <unordered_map>
#include <hackedconvar.h>

#if defined CVARSX_SERVER

#include <eiface.h>
#include <inetchannel.h>

#elif defined CVARSX_CLIENT

#include <cdll_int.h>

#endif

namespace global
{

static SourceSDK::FactoryLoader icvar_loader( "vstdlib", true, IS_SERVERSIDE );
static SourceSDK::FactoryLoader engine_loader( "engine", false, IS_SERVERSIDE );

#if defined CVARSX_SERVER

static const char *ivengine_name = "VEngineServer021";

typedef IVEngineServer IVEngine;

#elif defined CVARSX_CLIENT

static const char *ivengine_name = "VEngineClient015";

typedef IVEngineClient IVEngine;

#endif

static ICvar *icvar = nullptr;
static IVEngine *ivengine = nullptr;

static void Initialize( lua_State *state )
{
	icvar = engine_loader.GetInterface<ICvar>( CVAR_INTERFACE_VERSION );
	if( icvar == nullptr )
		LUA->ThrowError( "ICVar not initialized. Critical error." );

	ivengine = engine_loader.GetInterface<IVEngine>( ivengine_name );
	if( ivengine == nullptr )
		LUA->ThrowError( "IVEngineServer/Client not initialized. Critical error." );
}

}

namespace convar
{

struct UserData
{
	ConVar *cvar;
	uint8_t type;
	const char *name_original;
	char name[64];
	const char *help_original;
	char help[256];
};

static const char *metaname = "convar";
static uint8_t metatype = GarrysMod::Lua::Type::COUNT + 1;
static const char *invalid_error = "invalid convar";
static const char *table_name = "convars_objects";

inline void CheckType( lua_State *state, int32_t index )
{
	if( !LUA->IsType( index, metatype ) )
		luaL_typerror( state, index, metaname );
}

inline UserData *GetUserdata( lua_State *state, int index )
{
	return static_cast<UserData *>( LUA->GetUserdata( index ) );
}

static ConVar *Get( lua_State *state, int32_t index )
{
	CheckType( state, index );
	ConVar *convar = static_cast<UserData *>( LUA->GetUserdata( index ) )->cvar;
	if( convar == nullptr )
		LUA->ArgError( index, invalid_error );

	return convar;
}

inline void Push( lua_State *state, ConVar *convar )
{
	if( convar == nullptr )
	{
		LUA->PushNil( );
		return;
	}

	LUA->GetField( GarrysMod::Lua::INDEX_REGISTRY, table_name );
	LUA->PushUserdata( convar );
	LUA->GetTable( -2 );
	if( LUA->IsType( -1, metatype ) )
	{
		LUA->Remove( -2 );
		return;
	}

	LUA->Pop( 1 );

	UserData *udata = static_cast<UserData *>( LUA->NewUserdata( sizeof( UserData ) ) );
	udata->cvar = convar;
	udata->type = metatype;
	udata->name_original = convar->m_pszName;
	udata->help_original = convar->m_pszHelpString;

	LUA->CreateMetaTableType( metaname, metatype );
	LUA->SetMetaTable( -2 );

	LUA->CreateTable( );
	lua_setfenv( state, -2 );

	LUA->PushUserdata( convar );
	LUA->Push( -2 );
	LUA->SetTable( -4 );
	LUA->Remove( -2 );
}

inline ConVar *Destroy( lua_State *state, int32_t index )
{
	UserData *udata = GetUserdata( state, 1 );
	ConVar *convar = udata->cvar;
	if( convar == nullptr )
		return nullptr;

	LUA->GetField( GarrysMod::Lua::INDEX_REGISTRY, table_name );
	LUA->PushUserdata( convar );
	LUA->PushNil( );
	LUA->SetTable( -2 );
	LUA->Pop( 1 );

	convar->m_pszName = udata->name_original;
	convar->m_pszHelpString = udata->help_original;
	udata->cvar = nullptr;

	return convar;
}

LUA_FUNCTION_STATIC( gc )
{
	if( !LUA->IsType( 1, metatype ) )
		return 0;

	Destroy( state, 1 );
	return 0;
}

LUA_FUNCTION_STATIC( eq )
{
	LUA->PushBool( Get( state, 1 ) == Get( state, 2 ) );
	return 1;
}

LUA_FUNCTION_STATIC( tostring )
{
	lua_pushfstring( state, "%s: %p", metaname, Get( state, 1 ) );
	return 1;
}

LUA_FUNCTION_STATIC( index )
{
	LUA->GetMetaTable( 1 );
	LUA->Push( 2 );
	LUA->RawGet( -2 );
	if( !LUA->IsType( -1, GarrysMod::Lua::Type::NIL ) )
		return 1;

	LUA->Pop( 2 );

	lua_getfenv( state, 1 );
	LUA->Push( 2 );
	LUA->RawGet( -2 );
	return 1;
}

LUA_FUNCTION_STATIC( newindex )
{
	lua_getfenv( state, 1 );
	LUA->Push( 2 );
	LUA->Push( 3 );
	LUA->RawSet( -3 );
	return 0;
}

LUA_FUNCTION_STATIC( SetValue )
{
	ConVar *convar = Get( state, 1 );

	switch( LUA->GetType( 2 ) )
	{
		case GarrysMod::Lua::Type::NUMBER:
			convar->SetValue( static_cast<float>( LUA->GetNumber( 2 ) ) );
			break;

		case GarrysMod::Lua::Type::BOOL:
			convar->SetValue( LUA->GetBool( 2 ) ? 1 : 0 );
			break;

		case GarrysMod::Lua::Type::STRING:
			convar->SetValue( LUA->GetString( 2 ) );
			break;

		default:
			LUA->ThrowError( "argument #2 is invalid (type should be number, boolean or string)" );
	}

	return 0;
}

LUA_FUNCTION_STATIC( GetBool )
{
	LUA->PushBool( Get( state, 1 )->GetBool( ) );
	return 1;
}

LUA_FUNCTION_STATIC( GetDefault )
{

	LUA->PushString( Get( state, 1 )->GetDefault( ) );
	return 1;
}

LUA_FUNCTION_STATIC( GetFloat )
{
	LUA->PushNumber( Get( state, 1 )->GetFloat( ) );
	return 1;
}

LUA_FUNCTION_STATIC( GetInt )
{
	LUA->PushNumber( Get( state, 1 )->GetInt( ) );
	return 1;
}

LUA_FUNCTION_STATIC( GetName )
{
	LUA->PushString( Get( state, 1 )->GetName( ) );
	return 1;
}

LUA_FUNCTION_STATIC( SetName )
{
	UserData *udata = GetUserdata( state, 1 );
	ConVar *convar = udata->cvar;
	if( convar == nullptr )
		LUA->ThrowError( invalid_error );

	V_strncpy( udata->name, LUA->CheckString( 2 ), sizeof( udata->name ) );
	convar->m_pszName = udata->name;

	return 0;
}

LUA_FUNCTION_STATIC( GetString )
{
	LUA->PushString( Get( state, 1 )->GetString( ) );
	return 1;
}

LUA_FUNCTION_STATIC( SetFlags )
{
	Get( state, 1 )->m_nFlags = static_cast<int32_t>( LUA->CheckNumber( 2 ) );
	return 0;
}

LUA_FUNCTION_STATIC( GetFlags )
{
	LUA->PushNumber( Get( state, 1 )->m_nFlags );
	return 1;
}

LUA_FUNCTION_STATIC( HasFlag )
{
	LUA->Push( Get( state, 1 )->IsFlagSet( static_cast<int32_t>( LUA->CheckNumber( 2 ) ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( SetHelpText )
{
	UserData *udata = GetUserdata( state, 1 );
	ConVar *convar = udata->cvar;
	if( convar == nullptr )
		LUA->ThrowError( invalid_error );

	V_strncpy( udata->help, LUA->CheckString( 2 ), sizeof( udata->help ) );
	convar->m_pszHelpString = udata->help;

	return 0;
}

LUA_FUNCTION_STATIC( GetHelpText )
{
	LUA->PushString( Get( state, 1 )->GetHelpText( ) );
	return 1;
}

LUA_FUNCTION_STATIC( Revert )
{
	Get( state, 1 )->Revert( );
	return 0;
}

LUA_FUNCTION_STATIC( GetMin )
{
	float min = 0.0f;
	if( !Get( state, 1 )->GetMin( min ) )
		return 0;

	LUA->PushNumber( min );
	return 1;
}

LUA_FUNCTION_STATIC( SetMin )
{
	Get( state, 1 )->m_fMinVal = static_cast<float>( LUA->CheckNumber( 2 ) );
	return 0;
}

LUA_FUNCTION_STATIC( GetMax )
{
	float max = 0.0f;
	if( !Get( state, 1 )->GetMax( max ) )
		return 0;

	LUA->PushNumber( max );
	return 1;
}

LUA_FUNCTION_STATIC( SetMax )
{
	Get( state, 1 )->m_fMaxVal = static_cast<float>( LUA->CheckNumber( 2 ) );
	return 0;
}

LUA_FUNCTION_STATIC( Remove )
{
	CheckType( state, 1 );
	global::icvar->UnregisterConCommand( Destroy( state, 1 ) );
	return 0;
}

static void Initialize( lua_State *state )
{
	LUA->CreateTable( );
	LUA->SetField( GarrysMod::Lua::INDEX_REGISTRY, table_name );

	LUA->CreateMetaTableType( metaname, metatype );

	LUA->PushCFunction( gc );
	LUA->SetField( -2, "__gc" );

	LUA->PushCFunction( tostring );
	LUA->SetField( -2, "__tostring" );

	LUA->PushCFunction( eq );
	LUA->SetField( -2, "__eq" );

	LUA->PushCFunction( index );
	LUA->SetField( -2, "__index" );

	LUA->PushCFunction( newindex );
	LUA->SetField( -2, "__newindex" );

	LUA->PushCFunction( SetValue );
	LUA->SetField( -2, "SetValue" );

	LUA->PushCFunction( GetBool );
	LUA->SetField( -2, "GetBool" );

	LUA->PushCFunction( GetDefault );
	LUA->SetField( -2, "GetDefault" );

	LUA->PushCFunction( GetFloat );
	LUA->SetField( -2, "GetFloat" );

	LUA->PushCFunction( GetInt );
	LUA->SetField( -2, "GetInt" );

	LUA->PushCFunction( GetName );
	LUA->SetField( -2, "GetName" );

	LUA->PushCFunction( SetName );
	LUA->SetField( -2, "SetName" );

	LUA->PushCFunction( GetString );
	LUA->SetField( -2, "GetString" );

	LUA->PushCFunction( SetFlags );
	LUA->SetField( -2, "SetFlags" );

	LUA->PushCFunction( GetFlags );
	LUA->SetField( -2, "GetFlags" );

	LUA->PushCFunction( HasFlag );
	LUA->SetField( -2, "HasFlag" );

	LUA->PushCFunction( SetHelpText );
	LUA->SetField( -2, "SetHelpText" );

	LUA->PushCFunction( GetHelpText );
	LUA->SetField( -2, "GetHelpText" );

	LUA->PushCFunction( Revert );
	LUA->SetField( -2, "Revert" );

	LUA->PushCFunction( GetMin );
	LUA->SetField( -2, "GetMin" );

	LUA->PushCFunction( SetMin );
	LUA->SetField( -2, "SetMin" );

	LUA->PushCFunction( GetMax );
	LUA->SetField( -2, "GetMax" );

	LUA->PushCFunction( SetMax );
	LUA->SetField( -2, "SetMax" );

	LUA->PushCFunction( Remove );
	LUA->SetField( -2, "Remove" );

	LUA->Pop( 1 );
}

static void Deinitialize( lua_State *state )
{
	LUA->PushNil( );
	LUA->SetField( GarrysMod::Lua::INDEX_REGISTRY, metaname );

	LUA->PushNil( );
	LUA->SetField( GarrysMod::Lua::INDEX_REGISTRY, table_name );
}

}

namespace cvars
{

static const char *table_name = "cvars";

LUA_FUNCTION_STATIC( Exists )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );

	ConVar *convar = global::icvar->FindVar( LUA->GetString( 1 ) );
	if( convar == nullptr )
		LUA->PushBool( false );
	else
		LUA->PushBool( !convar->IsCommand( ) );

	return 1;
}

LUA_FUNCTION_STATIC( GetAll )
{
	LUA->CreateTable( );

	size_t i = 0;
	ICvar::Iterator iter( global::icvar ); 
	for( iter.SetFirst( ); iter.IsValid( ); iter.Next( ) )
	{  
		ConVar *convar = static_cast<ConVar *>( iter.Get( ) );
		if( !convar->IsCommand( ) )
		{
			convar::Push( state, convar );
			LUA->PushNumber( ++i );
			LUA->SetTable( -3 );
		}
	}

	return 1;
}

LUA_FUNCTION_STATIC( Get )
{
	convar::Push( state, global::icvar->FindVar( LUA->CheckString( 1 ) ) );
	return 1;
}

static void Initialize( lua_State *state )
{
	LUA->GetField( GarrysMod::Lua::INDEX_GLOBAL, table_name );

	LUA->PushCFunction( Exists );
	LUA->SetField( -2, "Exists" );

	LUA->PushCFunction( GetAll );
	LUA->SetField( -2, "GetAll" );

	LUA->PushCFunction( Get );
	LUA->SetField( -2, "Get" );

	LUA->Pop( 1 );
}

static void Deinitialize( lua_State *state )
{
	LUA->GetField( GarrysMod::Lua::INDEX_GLOBAL, table_name );

	LUA->PushNil( );
	LUA->SetField( -2, "Exists" );

	LUA->PushNil( );
	LUA->SetField( -2, "GetAll" );

	LUA->PushNil( );
	LUA->SetField( -2, "Get" );

	LUA->Pop( 1 );
}

}

#if defined CVARSX_SERVER

namespace Player
{

static const char *invalid_error = "invalid Player";

inline int GetEntityIndex( lua_State *state, int i )
{
	LUA->Push( i );
	LUA->GetField( -1, "EntIndex" );
	LUA->Push( -2 );
	LUA->Call( 1, 1 );

	return static_cast<int32_t>( LUA->GetNumber( -1 ) );
}

LUA_FUNCTION_STATIC( GetConVarValue )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::ENTITY );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	LUA->PushString( global::ivengine->GetClientConVarValue(
		GetEntityIndex( state, 1 ),
		LUA->GetString( 2 )
	) );
	return 1;
}

LUA_FUNCTION_STATIC( SetConVarValue )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::ENTITY );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 3, GarrysMod::Lua::Type::STRING );

	INetChannel *netchan = static_cast<INetChannel *>( global::ivengine->GetPlayerNetInfo(
		GetEntityIndex( state, 1 )
	) );
	if( netchan == nullptr )
		LUA->ThrowError( invalid_error );

	char buffer[256] = { 0 };
	bf_write packet( buffer, sizeof( buffer ) );

	packet.WriteUBitLong( 5, 6 );
	packet.WriteByte( 0x01 );
	packet.WriteString( LUA->GetString( 2 ) );
	packet.WriteString( LUA->GetString( 3 ) );

	LUA->PushBool( netchan->SendData( packet ) );
	return 1;
}

static void Initialize( lua_State *state )
{
	LUA->CreateMetaTableType( "Player", GarrysMod::Lua::Type::ENTITY );

	LUA->PushCFunction( GetConVarValue );
	LUA->SetField( -2, "GetConVarValue" );

	LUA->PushCFunction( SetConVarValue );
	LUA->SetField( -2, "SetConVarValue" );

	LUA->Pop( 1 );
}

static void Deinitialize( lua_State *state )
{

	LUA->CreateMetaTableType( "Player", GarrysMod::Lua::Type::ENTITY );

	LUA->PushNil( );
	LUA->SetField( -2, "GetConVarValue" );

	LUA->PushNil( );
	LUA->SetField( -2, "SetConVarValue" );

	LUA->Pop( 1 );
}

}

#endif

GMOD_MODULE_OPEN( )
{
	global::Initialize( state );
	cvars::Initialize( state );
	convar::Initialize( state );

#if defined CVARSX_SERVER

	Player::Initialize( state );

#endif

	return 0;
}

GMOD_MODULE_CLOSE( )
{

#if defined CVARSX_SERVER

	Player::Deinitialize( state );

#endif

	convar::Deinitialize( state );
	cvars::Deinitialize( state );
	return 0;
}
