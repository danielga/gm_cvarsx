#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/AutoReference.h>
#include <GarrysMod/Interfaces.hpp>
#include <lua.hpp>
#include <cstdint>
#include <hackedconvar.h>

#if defined CVARSX_SERVER

#include <eiface.h>
#include <inetchannel.h>

#elif defined CVARSX_CLIENT

#include <cdll_int.h>

#endif

namespace global
{

static SourceSDK::FactoryLoader icvar_loader( "vstdlib", true, IS_SERVERSIDE, "bin/" );
static SourceSDK::FactoryLoader engine_loader( "engine", false, IS_SERVERSIDE, "bin/" );

#if defined CVARSX_SERVER

static const char *ivengine_name = "VEngineServer021";

typedef IVEngineServer IVEngine;

#elif defined CVARSX_CLIENT

static const char *ivengine_name = "VEngineClient015";

typedef IVEngineClient IVEngine;

#endif

static ICvar *icvar = nullptr;
static IVEngine *ivengine = nullptr;

static void Initialize( GarrysMod::Lua::ILuaBase *LUA )
{
	icvar = icvar_loader.GetInterface<ICvar>( CVAR_INTERFACE_VERSION );
	if( icvar == nullptr )
		LUA->ThrowError( "ICVar not initialized. Critical error." );

	ivengine = engine_loader.GetInterface<IVEngine>( ivengine_name );
	if( ivengine == nullptr )
		LUA->ThrowError( "IVEngineServer/Client not initialized. Critical error." );
}

}

namespace convar
{

struct Container
{
	ConVar *cvar;
	const char *name_original;
	char name[64];
	const char *help_original;
	char help[256];
};

static const char metaname[] = "convar";
static uint8_t metatype = 255;
static const char invalid_error[] = "invalid convar";
static const char table_name[] = "convars_objects";

inline void CheckType( GarrysMod::Lua::ILuaBase *LUA, int32_t index )
{
	if( !LUA->IsType( index, metatype ) )
		LUA->TypeError( index, metaname );
}

inline Container *GetUserdata( GarrysMod::Lua::ILuaBase *LUA, int index )
{
	return LUA->GetUserType<Container>( index, metatype );
}

static ConVar *Get( GarrysMod::Lua::ILuaBase *LUA, int32_t index )
{
	CheckType( LUA, index );
	ConVar *convar = LUA->GetUserType<Container>( index, metatype )->cvar;
	if( convar == nullptr )
		LUA->ArgError( index, invalid_error );

	return convar;
}

inline void Push( GarrysMod::Lua::ILuaBase *LUA, ConVar *convar )
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

	Container *udata = LUA->NewUserType<Container>( metatype );
	udata->cvar = convar;
	udata->name_original = convar->m_pszName;
	udata->help_original = convar->m_pszHelpString;

	LUA->PushMetaTable( metatype );
	LUA->SetMetaTable( -2 );

	LUA->CreateTable( );
	LUA->SetFEnv( -2 );

	LUA->PushUserdata( convar );
	LUA->Push( -2 );
	LUA->SetTable( -4 );
	LUA->Remove( -2 );
}

inline ConVar *Destroy( GarrysMod::Lua::ILuaBase *LUA, int32_t index )
{
	Container *udata = GetUserdata( LUA, 1 );
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

	Destroy( LUA, 1 );
	return 0;
}

LUA_FUNCTION_STATIC( eq )
{
	LUA->PushBool( Get( LUA, 1 ) == Get( LUA, 2 ) );
	return 1;
}

LUA_FUNCTION_STATIC( tostring )
{
	LUA->PushFormattedString( "%s: %p", metaname, Get( LUA, 1 ) );
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

	LUA->GetFEnv( 1 );
	LUA->Push( 2 );
	LUA->RawGet( -2 );
	return 1;
}

LUA_FUNCTION_STATIC( newindex )
{
	LUA->GetFEnv( 1 );
	LUA->Push( 2 );
	LUA->Push( 3 );
	LUA->RawSet( -3 );
	return 0;
}

LUA_FUNCTION_STATIC( SetValue )
{
	ConVar *convar = Get( LUA, 1 );

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
	LUA->PushBool( Get( LUA, 1 )->GetBool( ) );
	return 1;
}

LUA_FUNCTION_STATIC( GetDefault )
{

	LUA->PushString( Get( LUA, 1 )->GetDefault( ) );
	return 1;
}

LUA_FUNCTION_STATIC( GetFloat )
{
	LUA->PushNumber( Get( LUA, 1 )->GetFloat( ) );
	return 1;
}

LUA_FUNCTION_STATIC( GetInt )
{
	LUA->PushNumber( Get( LUA, 1 )->GetInt( ) );
	return 1;
}

LUA_FUNCTION_STATIC( GetName )
{
	LUA->PushString( Get( LUA, 1 )->GetName( ) );
	return 1;
}

LUA_FUNCTION_STATIC( SetName )
{
	Container *udata = GetUserdata( LUA, 1 );
	ConVar *convar = udata->cvar;
	if( convar == nullptr )
		LUA->ThrowError( invalid_error );

	V_strncpy( udata->name, LUA->CheckString( 2 ), sizeof( udata->name ) );
	convar->m_pszName = udata->name;

	return 0;
}

LUA_FUNCTION_STATIC( GetString )
{
	LUA->PushString( Get( LUA, 1 )->GetString( ) );
	return 1;
}

LUA_FUNCTION_STATIC( SetFlags )
{
	Get( LUA, 1 )->m_nFlags = static_cast<int32_t>( LUA->CheckNumber( 2 ) );
	return 0;
}

LUA_FUNCTION_STATIC( GetFlags )
{
	LUA->PushNumber( Get( LUA, 1 )->m_nFlags );
	return 1;
}

LUA_FUNCTION_STATIC( HasFlag )
{
	LUA->Push( Get( LUA, 1 )->IsFlagSet( static_cast<int32_t>( LUA->CheckNumber( 2 ) ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( SetHelpText )
{
	Container *udata = GetUserdata( LUA, 1 );
	ConVar *convar = udata->cvar;
	if( convar == nullptr )
		LUA->ThrowError( invalid_error );

	V_strncpy( udata->help, LUA->CheckString( 2 ), sizeof( udata->help ) );
	convar->m_pszHelpString = udata->help;

	return 0;
}

LUA_FUNCTION_STATIC( GetHelpText )
{
	LUA->PushString( Get( LUA, 1 )->GetHelpText( ) );
	return 1;
}

LUA_FUNCTION_STATIC( Revert )
{
	Get( LUA, 1 )->Revert( );
	return 0;
}

LUA_FUNCTION_STATIC( GetMin )
{
	float min = 0.0f;
	if( !Get( LUA, 1 )->GetMin( min ) )
		return 0;

	LUA->PushNumber( min );
	return 1;
}

LUA_FUNCTION_STATIC( SetMin )
{
	Get( LUA, 1 )->m_fMinVal = static_cast<float>( LUA->CheckNumber( 2 ) );
	return 0;
}

LUA_FUNCTION_STATIC( GetMax )
{
	float max = 0.0f;
	if( !Get( LUA, 1 )->GetMax( max ) )
		return 0;

	LUA->PushNumber( max );
	return 1;
}

LUA_FUNCTION_STATIC( SetMax )
{
	Get( LUA, 1 )->m_fMaxVal = static_cast<float>( LUA->CheckNumber( 2 ) );
	return 0;
}

LUA_FUNCTION_STATIC( Remove )
{
	CheckType( LUA, 1 );
	global::icvar->UnregisterConCommand( Destroy( LUA, 1 ) );
	return 0;
}

static void Initialize( GarrysMod::Lua::ILuaBase *LUA )
{
	LUA->CreateTable( );
	LUA->SetField( GarrysMod::Lua::INDEX_REGISTRY, table_name );

	metatype = LUA->CreateMetaTable( metaname );

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

static void Deinitialize( GarrysMod::Lua::ILuaBase *LUA )
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
			convar::Push( LUA, convar );
			LUA->PushNumber( ++i );
			LUA->SetTable( -3 );
		}
	}

	return 1;
}

LUA_FUNCTION_STATIC( Get )
{
	convar::Push( LUA, global::icvar->FindVar( LUA->CheckString( 1 ) ) );
	return 1;
}

static void Initialize( GarrysMod::Lua::ILuaBase *LUA )
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

static void Deinitialize( GarrysMod::Lua::ILuaBase *LUA )
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

inline int GetEntityIndex( GarrysMod::Lua::ILuaBase *LUA, int i )
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
		GetEntityIndex( LUA, 1 ),
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
		GetEntityIndex( LUA, 1 )
	) );
	if( netchan == nullptr )
		LUA->ThrowError( invalid_error );

	char buffer[2 + 260 + 260] = { 0 };
	bf_write packet( buffer, sizeof( buffer ) );
	packet.SetAssertOnOverflow( false );

	packet.WriteUBitLong( 5, 6 );
	packet.WriteByte( 0x01 );
	packet.WriteString( LUA->GetString( 2 ) );
	packet.WriteString( LUA->GetString( 3 ) );

	LUA->PushBool( !packet.IsOverflowed( ) ? netchan->SendData( packet ) : false );
	return 1;
}

static void Initialize( GarrysMod::Lua::ILuaBase *LUA )
{
	LUA->GetField( GarrysMod::Lua::INDEX_REGISTRY, "Player" );

	LUA->PushCFunction( GetConVarValue );
	LUA->SetField( -2, "GetConVarValue" );

	LUA->PushCFunction( SetConVarValue );
	LUA->SetField( -2, "SetConVarValue" );

	LUA->Pop( 1 );
}

static void Deinitialize( GarrysMod::Lua::ILuaBase *LUA )
{

	LUA->GetField( GarrysMod::Lua::INDEX_REGISTRY, "Player" );

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
	global::Initialize( LUA );
	cvars::Initialize( LUA );
	convar::Initialize( LUA );

#if defined CVARSX_SERVER

	Player::Initialize( LUA );

#endif

	return 0;
}

GMOD_MODULE_CLOSE( )
{

#if defined CVARSX_SERVER

	Player::Deinitialize( LUA );

#endif

	convar::Deinitialize( LUA );
	cvars::Deinitialize( LUA );
	return 0;
}
