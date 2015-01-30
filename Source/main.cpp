#include <GarrysMod/Lua/Interface.h>
#include <cstdint>
#include <interface.h>
#include <hackedconvar.h>
#include <inetchannel.h>
#include <eiface.h>
#include <cdll_int.h>

namespace global
{

#if defined _WIN32

static CDllDemandLoader icvar_loader( "vstdlib.dll" );
static CDllDemandLoader engine_loader( "engine.dll" );

#elif defined __linux

#if defined CVARSX_SERVER

static CDllDemandLoader icvar_loader( "libvstdlib_srv.so" );
static CDllDemandLoader engine_loader( "engine_srv.so" );

#elif defined CVARSX_CLIENT

static CDllDemandLoader icvar_loader( "libvstdlib.so" );
static CDllDemandLoader engine_loader( "engine.so" );

#endif

#elif defined __APPLE__

static CDllDemandLoader icvar_loader( "libvstdlib.dylib" );
static CDllDemandLoader engine_loader( "engine.dylib" );

#endif

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
	CreateInterfaceFn factory = icvar_loader.GetFactory( );
	if( factory == nullptr )
		LUA->ThrowError( "Couldn't get vstdlib factory. Critical error." );

	icvar = static_cast<ICvar *>( factory( CVAR_INTERFACE_VERSION, nullptr ) );
	if( icvar == nullptr )
		LUA->ThrowError( "ICVar not initialized. Critical error." );

	factory = engine_loader.GetFactory( );
	if( factory == nullptr )
		LUA->ThrowError( "Couldn't get engine factory. Critical error." );

	ivengine = static_cast<IVEngine *>( factory( ivengine_name, nullptr ) );
	if( ivengine == nullptr )
		LUA->ThrowError( "IVEngineServer/Client not initialized. Critical error." );
}

}

namespace convar
{

struct userdata
{
	void *data;
	uint8_t type;
	char name[64];
	char help[256];
};

static const char *metaname = "convar";
static uint8_t metatype = GarrysMod::Lua::Type::COUNT + 1;

static const char *invalid_error = "convar object is not valid";

static userdata *Create( lua_State *state )
{
	userdata *udata = static_cast<userdata *>( LUA->NewUserdata( sizeof( userdata ) ) );
	udata->type = metatype;

	LUA->CreateMetaTableType( metaname, metatype );
	LUA->SetMetaTable( -2 );

	return udata;
}

inline userdata *GetUserdata( lua_State *state, int index )
{
	return static_cast<userdata *>( LUA->GetUserdata( index ) );
}

inline ConVar *GetAndValidate( lua_State *state, int index, const char *err )
{
	ConVar *convar = static_cast<ConVar *>( GetUserdata( state, index )->data );
	if( convar == nullptr )
		LUA->ThrowError( err );

	return convar;
}

LUA_FUNCTION_STATIC( gc )
{
	LUA->CheckType( 1, metatype );

	userdata *udata = static_cast<userdata *>( LUA->GetUserdata( 1 ) );
	if( udata->data == nullptr )
		return 0;

	udata->data = nullptr;

	return 0;
}

LUA_FUNCTION_STATIC( eq )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, metatype );

	ConVar *cvar1 = GetAndValidate( state, 1, invalid_error );
	ConVar *cvar2 = GetAndValidate( state, 2, invalid_error );

	LUA->PushBool( cvar1 == cvar2 );

	return 1;
}

LUA_FUNCTION_STATIC( tostring )
{
	LUA->CheckType( 1, metatype );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	char formatted[30] = { 0 };
	V_snprintf( formatted, sizeof( formatted ), "%s: 0x%p", metaname, cvar );
	LUA->PushString( formatted );

	return 1;
}

LUA_FUNCTION_STATIC( SetValue )
{
	LUA->CheckType( 1, metatype );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	switch( LUA->GetType( 2 ) )
	{
		case GarrysMod::Lua::Type::NUMBER:
			cvar->SetValue( static_cast<float>( LUA->GetNumber( 2 ) ) );
			break;

		case GarrysMod::Lua::Type::BOOL:
			cvar->SetValue( LUA->GetBool( 2 ) ? 1 : 0 );
			break;

		case GarrysMod::Lua::Type::STRING:
			cvar->SetValue( LUA->GetString( 2 ) );
			break;

		default:
			LUA->ThrowError( "argument #2 is invalid (type should be number, boolean or string)" );
	}

	return 0;
}

LUA_FUNCTION_STATIC( GetBool )
{
	LUA->CheckType( 1, metatype );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	LUA->PushBool( cvar->GetBool( ) );

	return 1;
}

LUA_FUNCTION_STATIC( GetDefault )
{
	LUA->CheckType( 1, metatype );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	LUA->PushString( cvar->GetDefault( ) );

	return 1;
}

LUA_FUNCTION_STATIC( GetFloat )
{
	LUA->CheckType( 1, metatype );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	LUA->PushNumber( cvar->GetFloat( ) );

	return 1;
}

LUA_FUNCTION_STATIC( GetInt )
{
	LUA->CheckType( 1, metatype );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	LUA->PushNumber( cvar->GetInt( ) );

	return 1;
}

LUA_FUNCTION_STATIC( GetName )
{
	LUA->CheckType( 1, metatype );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	LUA->PushString( cvar->GetName( ) );

	return 1;
}

LUA_FUNCTION_STATIC( SetName )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	userdata *udata = static_cast<userdata *>( LUA->GetUserdata( 1 ) );

	ConVar *cvar = static_cast<ConVar *>( udata->data );
	if( cvar == nullptr )
		LUA->ThrowError( invalid_error );

	V_strncpy( udata->name, LUA->GetString( 2 ), sizeof( udata->name ) );
	cvar->m_pszName = udata->name;

	return 0;
}

LUA_FUNCTION_STATIC( GetString )
{
	LUA->CheckType( 1, metatype );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	LUA->PushString( cvar->GetString( ) );

	return 1;
}

LUA_FUNCTION_STATIC( SetFlags )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	cvar->m_nFlags = static_cast<int>( LUA->GetNumber( 2 ) );

	return 0;
}

LUA_FUNCTION_STATIC( GetFlags )
{
	LUA->CheckType( 1, metatype );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	LUA->PushNumber( cvar->m_nFlags );

	return 1;
}

LUA_FUNCTION_STATIC( HasFlag )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	LUA->Push( cvar->IsFlagSet( static_cast<int>( LUA->GetNumber( 2 ) ) ) );

	return 1;
}

LUA_FUNCTION_STATIC( SetHelpText )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	userdata *udata = static_cast<userdata *>( LUA->GetUserdata( 1 ) );

	ConVar *cvar = static_cast<ConVar *>( udata->data );
	if( cvar == nullptr )
		LUA->ThrowError( invalid_error );

	V_strncpy( udata->help, LUA->GetString( 2 ), sizeof( udata->help ) );
	cvar->m_pszHelpString = udata->help;

	return 0;
}

LUA_FUNCTION_STATIC( GetHelpText )
{
	LUA->CheckType( 1, metatype );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	LUA->PushString( cvar->GetHelpText( ) );

	return 1;
}

LUA_FUNCTION_STATIC( Revert )
{
	LUA->CheckType( 1, metatype );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	cvar->Revert( );

	return 0;
}

LUA_FUNCTION_STATIC( GetMin )
{
	LUA->CheckType( 1, metatype );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	float min = 0.0f;
	if( !cvar->GetMin( min ) )
		return 0;

	LUA->PushNumber( min );
	return 1;
}

LUA_FUNCTION_STATIC( SetMin )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	cvar->m_fMinVal = static_cast<float>( LUA->GetNumber( 2 ) );

	return 0;
}

LUA_FUNCTION_STATIC( GetMax )
{
	LUA->CheckType( 1, metatype );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	float max = 0.0f;
	if( !cvar->GetMax( max ) )
		return 0;

	LUA->PushNumber( max );
	return 1;
}

LUA_FUNCTION_STATIC( SetMax )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	cvar->m_fMaxVal = static_cast<float>( LUA->GetNumber( 2 ) );

	return 0;
}

LUA_FUNCTION_STATIC( Remove )
{
	LUA->CheckType( 1, metatype );

	ConVar *cvar = GetAndValidate( state, 1, invalid_error );

	global::icvar->UnregisterConCommand( cvar );

	return 0;
}

static void RegisterMetaTable( lua_State *state )
{
	LUA->CreateMetaTableType( metaname, metatype );

	LUA->PushCFunction( gc );
	LUA->SetField( -2, "__gc" );

	LUA->PushCFunction( tostring );
	LUA->SetField( -2, "__tostring" );

	LUA->PushCFunction( eq );
	LUA->SetField( -2, "__eq" );

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

	LUA->Push( -1 );
	LUA->SetField( -2, "__index" );
}

}

namespace cvars
{

LUA_FUNCTION_STATIC( Exists )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );

	ConVar *cvar = global::icvar->FindVar( LUA->GetString( 1 ) );
	if( cvar == nullptr )
		LUA->PushBool( false );
	else
		LUA->PushBool( !cvar->IsCommand( ) );

	return 1;
}

LUA_FUNCTION_STATIC( GetAll )
{
	LUA->CreateTable( );

	size_t i = 0;
	ICvar::Iterator iter( global::icvar ); 
	for( iter.SetFirst( ); iter.IsValid( ); iter.Next( ) )
	{  
		ConCommandBase *cvar = iter.Get( );
		if( !cvar->IsCommand( ) )
		{
			convar::Create( state )->data = cvar;
			LUA->PushNumber( ++i );
			LUA->SetTable( -3 );
		}
	}

	return 1;
}

LUA_FUNCTION_STATIC( Get )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );

	ConVar *cvar = global::icvar->FindVar( LUA->GetString( 1 ) );
	if( cvar == nullptr )
		return 0;

	convar::Create( state )->data = cvar;
	return 1;
}

static void RegisterGlobalTable( lua_State *state )
{
	LUA->PushSpecial( GarrysMod::Lua::SPECIAL_GLOB );

	LUA->GetField( -1, "cvars" );

	LUA->PushCFunction( Exists );
	LUA->SetField( -2, "Exists" );

	LUA->PushCFunction( GetAll );
	LUA->SetField( -2, "GetAll" );

	LUA->PushCFunction( Get );
	LUA->SetField( -2, "Get" );
}

}

namespace Player
{

#if defined CVARSX_SERVER

static const char *invalid_error = "Player object is not valid";

inline int GetEntityIndex( lua_State *state, int i )
{
	LUA->Push( i );
	LUA->GetField( -1, "EntIndex" );
	LUA->Push( -2 );
	LUA->Call( 1, 1 );

	return static_cast<int>( LUA->GetNumber( -1 ) );
}

LUA_FUNCTION_STATIC( GetConVarValue )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::ENTITY );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	LUA->PushString( global::ivengine->GetClientConVarValue( GetEntityIndex( state, 1 ), LUA->GetString( 2 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( SetConVarValue )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::ENTITY );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 3, GarrysMod::Lua::Type::STRING );

	INetChannel *netchan = static_cast<INetChannel *>( global::ivengine->GetPlayerNetInfo( GetEntityIndex( state, 1 ) ) );
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

#endif

static void RegisterMetaTable( lua_State *state )
{

#if defined CVARSX_SERVER

	LUA->CreateMetaTableType( "Player", GarrysMod::Lua::Type::ENTITY );

	LUA->PushCFunction( GetConVarValue );
	LUA->SetField( -2, "GetConVarValue" );

	LUA->PushCFunction( SetConVarValue );
	LUA->SetField( -2, "SetConVarValue" );

#endif

}

}

GMOD_MODULE_OPEN( )
{
	global::Initialize( state );

	cvars::RegisterGlobalTable( state );
	
	convar::RegisterMetaTable( state );

	Player::RegisterMetaTable( state );

	return 0;
}

GMOD_MODULE_CLOSE( )
{
	(void)state;
	return 0;
}