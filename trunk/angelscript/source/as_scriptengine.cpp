/*
   AngelCode Scripting Library
   Copyright (c) 2003-2009 Andreas Jonsson

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any
   damages arising from the use of this software.

   Permission is granted to anyone to use this software for any
   purpose, including commercial applications, and to alter it and
   redistribute it freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you
      must not claim that you wrote the original software. If you use
      this software in a product, an acknowledgment in the product
      documentation would be appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and
      must not be misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
      distribution.

   The original version of this library can be located at:
   http://www.angelcode.com/angelscript/

   Andreas Jonsson
   andreas@angelcode.com
*/


//
// as_scriptengine.cpp
//
// The implementation of the script engine interface
//


#include <stdlib.h>

#include "as_config.h"
#include "as_scriptengine.h"
#include "as_builder.h"
#include "as_context.h"
#include "as_string_util.h"
#include "as_tokenizer.h"
#include "as_texts.h"
#include "as_module.h"
#include "as_callfunc.h"
#include "as_arrayobject.h"
#include "as_generic.h"
#include "as_scriptstruct.h"
#include "as_compiler.h"

BEGIN_AS_NAMESPACE

extern "C"
{

AS_API const char * asGetLibraryVersion()
{
#ifdef _DEBUG
	return ANGELSCRIPT_VERSION_STRING " DEBUG";
#else
	return ANGELSCRIPT_VERSION_STRING;
#endif
}

AS_API const char * asGetLibraryOptions()
{
	const char *string = " "

	// Options
#ifdef AS_MAX_PORTABILITY
		"AS_MAX_PORTABILITY "
#endif
#ifdef AS_DEBUG
		"AS_DEBUG "
#endif
#ifdef AS_NO_CLASS_METHODS
		"AS_NO_CLASS_METHODS "
#endif
#ifdef AS_USE_DOUBLE_AS_FLOAT
		"AS_USE_DOUBLE_AS_FLOAT "
#endif
#ifdef AS_64BIT_PTR
		"AS_64BIT_PTR "
#endif
#ifdef AS_NO_THREADS
		"AS_NO_THREADS "
#endif
#ifdef AS_NO_ATOMIC
		"AS_NO_ATOMIC "
#endif

	// Target system
#ifdef AS_WIN
		"AS_WIN "
#endif
#ifdef AS_LINUX
		"AS_LINUX "
#endif
#ifdef AS_MAC
		"AS_MAC "
#endif
#ifdef AS_BSD
		"AS_BSD "
#endif
#ifdef AS_XBOX
		"AS_XBOX "
#endif
#ifdef AS_XBOX360
		"AS_XBOX360 "
#endif
#ifdef AS_PSP
		"AS_PSP "
#endif
#ifdef AS_PS2
		"AS_PS2 "
#endif
#ifdef AS_PS3
		"AS_PS3 "
#endif
#ifdef AS_DC
		"AS_DC "
#endif
#ifdef AS_GC
		"AS_GC "
#endif
#ifdef AS_WII
		"AS_WII "
#endif

	// CPU family
#ifdef AS_PPC
		"AS_PPC "
#endif
#ifdef AS_PPC_64
		"AS_PPC_64 "
#endif
#ifdef AS_X86
		"AS_X86 "
#endif
#ifdef AS_MIPS
		"AS_MIPS "
#endif
#ifdef AS_SH4
		"AS_SH4 "
#endif
#ifdef AS_XENON
		"AS_XENON "
#endif
	;

	return string;
}

AS_API asIScriptEngine *asCreateScriptEngine(asDWORD version)
{
	// Verify the version that the application expects
	if( (version/10000) != ANGELSCRIPT_VERSION_MAJOR )
		return 0;

	if( (version/100)%100 != ANGELSCRIPT_VERSION_MINOR )
		return 0;

	if( (version%100) > ANGELSCRIPT_VERSION_BUILD )
		return 0;

	// Verify the size of the types
	asASSERT( sizeof(asBYTE)  == 1 );
	asASSERT( sizeof(asWORD)  == 2 );
	asASSERT( sizeof(asDWORD) == 4 );
	asASSERT( sizeof(asQWORD) == 8 );
	asASSERT( sizeof(asPWORD) == sizeof(void*) );

	// Verify the boolean type
	asASSERT( sizeof(bool) == AS_SIZEOF_BOOL );
	asASSERT( true == VALUE_OF_BOOLEAN_TRUE );

	// Verify endianess
#ifdef AS_BIG_ENDIAN
	asASSERT( *(asDWORD*)"\x00\x01\x02\x03" == 0x00010203 );
	asASSERT( *(asQWORD*)"\x00\x01\x02\x03\x04\x05\x06\x07" == I64(0x0001020304050607) );
#else
	asASSERT( *(asDWORD*)"\x00\x01\x02\x03" == 0x03020100 );
	asASSERT( *(asQWORD*)"\x00\x01\x02\x03\x04\x05\x06\x07" == I64(0x0706050403020100) );
#endif

	return asNEW(asCScriptEngine)();
}

int asCScriptEngine::SetEngineProperty(asEEngineProp property, asPWORD value)
{
	switch( property )
	{
	case asEP_ALLOW_UNSAFE_REFERENCES:
		ep.allowUnsafeReferences = value ? true : false;
		break;

	case asEP_OPTIMIZE_BYTECODE:
		ep.optimizeByteCode = value ? true : false;
		break;

	case asEP_COPY_SCRIPT_SECTIONS:
		ep.copyScriptSections = value ? true : false;
		break;

	case asEP_MAX_STACK_SIZE:
		// The size is given in bytes, but we only store dwords
		ep.maximumContextStackSize = (int)value/4;
		if( initialContextStackSize > ep.maximumContextStackSize )
			initialContextStackSize = ep.maximumContextStackSize;
		break;

	case asEP_USE_CHARACTER_LITERALS:
		ep.useCharacterLiterals = value ? true : false;
		break;

	case asEP_ALLOW_MULTILINE_STRINGS:
		ep.allowMultilineStrings = value ? true : false;
		break;

	case asEP_ALLOW_IMPLICIT_HANDLE_TYPES:
		ep.allowImplicitHandleTypes = value ? true : false;
		break;

	case asEP_BUILD_WITHOUT_LINE_CUES:
		ep.buildWithoutLineCues = value ? true : false;
		break;

	case asEP_INIT_GLOBAL_VARS_AFTER_BUILD:
		ep.initGlobalVarsAfterBuild = value ? true : false;
		break;

	default:
		return asINVALID_ARG;
	}

	return asSUCCESS;
}

asPWORD asCScriptEngine::GetEngineProperty(asEEngineProp property)
{
	switch( property )
	{
	case asEP_ALLOW_UNSAFE_REFERENCES:
		return ep.allowUnsafeReferences;

	case asEP_OPTIMIZE_BYTECODE:
		return ep.optimizeByteCode;

	case asEP_COPY_SCRIPT_SECTIONS:
		return ep.copyScriptSections;

	case asEP_MAX_STACK_SIZE:
		return ep.maximumContextStackSize*4;

	case asEP_USE_CHARACTER_LITERALS:
		return ep.useCharacterLiterals;

	case asEP_ALLOW_MULTILINE_STRINGS:
		return ep.allowMultilineStrings;

	case asEP_ALLOW_IMPLICIT_HANDLE_TYPES:
		return ep.allowImplicitHandleTypes;

	case asEP_BUILD_WITHOUT_LINE_CUES:
		return ep.buildWithoutLineCues;

	case asEP_INIT_GLOBAL_VARS_AFTER_BUILD:
		return ep.initGlobalVarsAfterBuild;
	}

	return 0;
}

} // extern "C"





asCScriptEngine::asCScriptEngine()
{
	// Instanciate the thread manager
	if( threadManager == 0 )
		threadManager = asNEW(asCThreadManager);
	else
		threadManager->AddRef();

	// Engine properties
	ep.allowUnsafeReferences    = false;
	ep.optimizeByteCode         = true;
	ep.copyScriptSections       = true;
	ep.maximumContextStackSize  = 0;         // no limit
	ep.useCharacterLiterals     = false;
	ep.allowMultilineStrings    = false;
	ep.allowImplicitHandleTypes = false;
	ep.buildWithoutLineCues     = false;
	ep.initGlobalVarsAfterBuild = true;

	gc.engine = this;

	scriptTypeBehaviours.engine = this;
	refCount.set(1);
	stringFactory = 0;
	configFailed = false;
	isPrepared = false;
	isBuilding = false;
	lastModule = 0;


	userData = 0;

	initialContextStackSize = 1024;      // 1 KB


	typeIdSeqNbr = 0;
	currentGroup = &defaultGroup;

	msgCallback = 0;

	// Reserve function id 0 for no function
	scriptFunctions.PushLast(0);

	// Make sure typeId for the built-in primitives are defined according to asETypeIdFlags
	int id;
	id = GetTypeIdFromDataType(asCDataType::CreatePrimitive(ttVoid,   false)); asASSERT( id == asTYPEID_VOID   );
	id = GetTypeIdFromDataType(asCDataType::CreatePrimitive(ttBool,   false)); asASSERT( id == asTYPEID_BOOL   );
	id = GetTypeIdFromDataType(asCDataType::CreatePrimitive(ttInt8,   false)); asASSERT( id == asTYPEID_INT8   );
	id = GetTypeIdFromDataType(asCDataType::CreatePrimitive(ttInt16,  false)); asASSERT( id == asTYPEID_INT16  );
	id = GetTypeIdFromDataType(asCDataType::CreatePrimitive(ttInt,    false)); asASSERT( id == asTYPEID_INT32  );
	id = GetTypeIdFromDataType(asCDataType::CreatePrimitive(ttInt64,  false)); asASSERT( id == asTYPEID_INT64  );
	id = GetTypeIdFromDataType(asCDataType::CreatePrimitive(ttUInt8,  false)); asASSERT( id == asTYPEID_UINT8  );
	id = GetTypeIdFromDataType(asCDataType::CreatePrimitive(ttUInt16, false)); asASSERT( id == asTYPEID_UINT16 );
	id = GetTypeIdFromDataType(asCDataType::CreatePrimitive(ttUInt,   false)); asASSERT( id == asTYPEID_UINT32 );
	id = GetTypeIdFromDataType(asCDataType::CreatePrimitive(ttUInt64, false)); asASSERT( id == asTYPEID_UINT64 );
	id = GetTypeIdFromDataType(asCDataType::CreatePrimitive(ttFloat,  false)); asASSERT( id == asTYPEID_FLOAT  );
	id = GetTypeIdFromDataType(asCDataType::CreatePrimitive(ttDouble, false)); asASSERT( id == asTYPEID_DOUBLE );

	RegisterArrayObject(this);
	RegisterScriptObject(this);
}

asCScriptEngine::~asCScriptEngine()
{
	asASSERT(refCount.get() == 0);
	asUINT n;

	Reset();

	// Delete the factory stubs for the array types
	for( n = 0; n < arrayTypes.GetLength(); n++ )
	{
		if( arrayTypes[n] )
		{
			// Delete the factory stubs first
			for( asUINT f = 0; f < arrayTypes[n]->beh.factories.GetLength(); f++ )
			{
				DeleteScriptFunction(arrayTypes[n]->beh.factories[f]);
			}
			arrayTypes[n]->beh.factories.Allocate(0, false);
		}
	}

	// The modules must be deleted first, as they may use
	// object types from the config groups
	for( n = 0; n < scriptModules.GetLength(); n++ )
	{
		if( scriptModules[n] )
		{
			if( scriptModules[n]->CanDelete() )
			{
				asDELETE(scriptModules[n],asCModule);
			}
			else
				asASSERT(false);
		}
	}
	scriptModules.SetLength(0);

	// Do one more garbage collect to free gc objects that were global variables
	GarbageCollect(asGC_FULL_CYCLE);

	ClearUnusedTypes();

	asSMapNode<int,asCDataType*> *cursor = 0;
	while( mapTypeIdToDataType.MoveFirst(&cursor) )
	{
		asDELETE(mapTypeIdToDataType.GetValue(cursor),asCDataType);
		mapTypeIdToDataType.Erase(cursor);
	}

	while( configGroups.GetLength() )
	{
		// Delete config groups in the right order
		asCConfigGroup *grp = configGroups.PopLast();
		if( grp )
		{
			asDELETE(grp,asCConfigGroup);
		}
	}

	for( n = 0; n < registeredGlobalProps.GetLength(); n++ )
	{
		if( registeredGlobalProps[n] )
		{
			asDELETE(registeredGlobalProps[n],asCGlobalProperty);
		}
	}
	registeredGlobalProps.SetLength(0);
	globalPropAddresses.SetLength(0);


	for( n = 0; n < arrayTypes.GetLength(); n++ )
	{
		if( arrayTypes[n] )
		{
			arrayTypes[n]->subType = 0;
			asDELETE(arrayTypes[n],asCObjectType);
		}
	}
	arrayTypes.SetLength(0);

	for( n = 0; n < objectTypes.GetLength(); n++ )
	{
		if( objectTypes[n] )
		{
			objectTypes[n]->subType = 0;
			asDELETE(objectTypes[n],asCObjectType);
		}
	}
	objectTypes.SetLength(0);
	registeredTypeDefs.SetLength(0);
	registeredEnums.SetLength(0);
	registeredObjTypes.SetLength(0);

	for( n = 0; n < scriptFunctions.GetLength(); n++ )
		if( scriptFunctions[n] )
		{
			asDELETE(scriptFunctions[n],asCScriptFunction);
		}
	scriptFunctions.SetLength(0);
	registeredGlobalFuncs.SetLength(0);

	// Release the thread manager
	threadManager->Release();
}

// interface
int asCScriptEngine::AddRef()
{
	return refCount.atomicInc();
}

// interface
int asCScriptEngine::Release()
{
	int r = refCount.atomicDec();

	if( r == 0 )
	{
		asDELETE(this,asCScriptEngine);
		return 0;
	}

	return r;
}

// interface
void *asCScriptEngine::SetUserData(void *data)
{
	void *old = userData;
	userData = data;
	return old;
}

// interface
void *asCScriptEngine::GetUserData()
{
	return userData;
}

void asCScriptEngine::Reset()
{
	GarbageCollect(asGC_FULL_CYCLE);

	asUINT n;
	for( n = 0; n < scriptModules.GetLength(); ++n )
	{
		if( scriptModules[n] )
			scriptModules[n]->Discard();
	}
}

// interface
int asCScriptEngine::SetMessageCallback(const asSFuncPtr &callback, void *obj, asDWORD callConv)
{
	msgCallback = true;
	msgCallbackObj = obj;
	bool isObj = false;
	if( (unsigned)callConv == asCALL_GENERIC )
	{
		msgCallback = false;
		return asNOT_SUPPORTED;
	}
	if( (unsigned)callConv >= asCALL_THISCALL )
	{
		isObj = true;
		if( obj == 0 )
		{
			msgCallback = false;
			return asINVALID_ARG;
		}
	}
	int r = DetectCallingConvention(isObj, callback, callConv, &msgCallbackFunc);
	if( r < 0 ) msgCallback = false;
	return r;
}

// interface
int asCScriptEngine::ClearMessageCallback()
{
	msgCallback = false;
	return 0;
}

// interface
int asCScriptEngine::WriteMessage(const char *section, int row, int col, asEMsgType type, const char *message)
{
	// Validate input parameters
	if( section == 0 ||
		message == 0 )
		return asINVALID_ARG;

	// If there is no callback then there's nothing to do
	if( !msgCallback )
		return 0;

	asSMessageInfo msg;
	msg.section = section;
	msg.row     = row;
	msg.col     = col;
	msg.type    = type;
	msg.message = message;

	if( msgCallbackFunc.callConv < ICC_THISCALL )
		CallGlobalFunction(&msg, msgCallbackObj, &msgCallbackFunc, 0);
	else
		CallObjectMethod(msgCallbackObj, &msg, &msgCallbackFunc, 0);

	return 0;
}

// interface
asETokenClass asCScriptEngine::ParseToken(const char *string, size_t stringLength, int *tokenLength)
{
	if( stringLength == 0 )
		stringLength = strlen(string);

	size_t len;
	asCTokenizer t;
	asETokenClass tc;
	t.GetToken(string, stringLength, &len, &tc);

	if( tokenLength )
		*tokenLength = (int)len;

	return tc;
}

// interface
asIScriptModule *asCScriptEngine::GetModule(const char *module, asEGMFlags flag)
{
	asCModule *mod = GetModule(module, false);

	if( flag == asGM_ALWAYS_CREATE )
	{
		if( mod != 0 )
			mod->Discard();
		return GetModule(module, true);
	}

	if( mod == 0 && flag == asGM_CREATE_IF_NOT_EXISTS )
	{
		return GetModule(module, true);
	}

	return mod;
}

// interface
int asCScriptEngine::DiscardModule(const char *module)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return asNO_MODULE;

	mod->Discard();

	// TODO: multithread: Must protect this for multiple accesses
	// Verify if there are any modules that can be deleted
	bool hasDeletedModules = false;
	for( asUINT n = 0; n < scriptModules.GetLength(); n++ )
	{
		if( scriptModules[n] && scriptModules[n]->CanDelete() )
		{
			hasDeletedModules = true;
			asDELETE(scriptModules[n],asCModule);
			scriptModules[n] = 0;
		}
	}

	if( hasDeletedModules )
		ClearUnusedTypes();

	return 0;
}

void asCScriptEngine::ClearUnusedTypes()
{
	// Build a list of all types to check for
	asCArray<asCObjectType*> types;
	types = classTypes;
	types.Concatenate(scriptArrayTypes);

	// Go through all modules
	asUINT n;
	for( n = 0; n < scriptModules.GetLength() && types.GetLength(); n++ )
	{
		asCModule *mod = scriptModules[n];
		if( mod )
		{
			// Go through all globals
			asUINT m;
			for( m = 0; m < mod->scriptGlobals.GetLength() && types.GetLength(); m++ )
			{
				if( mod->scriptGlobals[m]->type.GetObjectType() )
					RemoveTypeAndRelatedFromList(types, mod->scriptGlobals[m]->type.GetObjectType());
			}

			// Go through all type declarations
			for( m = 0; m < mod->classTypes.GetLength() && types.GetLength(); m++ )
				RemoveTypeAndRelatedFromList(types, mod->classTypes[m]);
			for( m = 0; m < mod->enumTypes.GetLength() && types.GetLength(); m++ )
				RemoveTypeAndRelatedFromList(types, mod->enumTypes[m]);
			for( m = 0; m < mod->typeDefs.GetLength() && types.GetLength(); m++ )
				RemoveTypeAndRelatedFromList(types, mod->typeDefs[m]);
		}
	}

	// Go through all function parameters and remove used types
	for( n = 0; n < scriptFunctions.GetLength() && types.GetLength(); n++ )
	{
		asCScriptFunction *func = scriptFunctions[n];
		if( func )
		{
			// Ignore factory stubs
			if( func->name == "factstub" )
				continue;

			asCObjectType *ot = func->returnType.GetObjectType();
			if( ot != 0 && ot != func->objectType )
				RemoveTypeAndRelatedFromList(types, ot);

			for( asUINT p = 0; p < func->parameterTypes.GetLength(); p++ )
			{
				ot = func->parameterTypes[p].GetObjectType();
				if( ot != 0 && ot != func->objectType )
					RemoveTypeAndRelatedFromList(types, ot);
			}
		}
	}

	// Go through all global properties
	for( n = 0; n < registeredGlobalProps.GetLength() && types.GetLength(); n++ )
	{
		if( registeredGlobalProps[n] && registeredGlobalProps[n]->type.GetObjectType() )
			RemoveTypeAndRelatedFromList(types, registeredGlobalProps[n]->type.GetObjectType());
	}

	// All that remains in the list after this can be discarded, since they are no longer used
	for(;;)
	{
		bool didClearArrayType = false;

		for( n = 0; n < types.GetLength(); n++ )
		{
			// Template types will have two references for each factory stub
			int refCount = (types[n]->flags & asOBJ_TEMPLATE) ? 2*(int)types[n]->beh.factories.GetLength() : 0;

			if( types[n]->GetRefCount() == refCount )
			{
				if( types[n]->arrayType )
				{
					didClearArrayType = true;
					RemoveArrayType(types[n]);
				}
				else
				{
					RemoveFromTypeIdMap(types[n]);
					asDELETE(types[n],asCObjectType);

					int i = classTypes.IndexOf(types[n]);
					if( i == (signed)classTypes.GetLength() - 1 )
						classTypes.PopLast();
					else
						classTypes[i] = classTypes.PopLast();
				}

				// Remove the type from the array
				if( n < types.GetLength() - 1 )
					types[n] = types.PopLast();
				else
					types.PopLast();
				n--;
			}
		}

		if( didClearArrayType == false )
			break;
	}
}

void asCScriptEngine::RemoveTypeAndRelatedFromList(asCArray<asCObjectType*> &types, asCObjectType *ot)
{
	// Remove the type from the list
	int i = types.IndexOf(ot);
	if( i == -1 ) return;

	if( i == (signed)types.GetLength() - 1 )
		types.PopLast();
	else
		types[i] = types.PopLast();

	// If the type is an array, then remove all sub types as well
	if( ot->subType )
	{
		while( ot->subType )
		{
			ot = ot->subType;
			RemoveTypeAndRelatedFromList(types, ot);
		}
		return;
	}

	// If the type is a class, then remove all properties types as well
	if( ot->properties.GetLength() )
	{
		for( asUINT n = 0; n < ot->properties.GetLength(); n++ )
			RemoveTypeAndRelatedFromList(types, ot->properties[n]->type.GetObjectType());
	}
}


// internal
int asCScriptEngine::GetFactoryIdByDecl(const asCObjectType *ot, const char *decl)
{
	asCModule *mod = 0;
	
	// Is this a script class?
	if( ot->flags & asOBJ_SCRIPT_OBJECT && ot->size > 0 )
	{
		mod = scriptFunctions[ot->beh.factory]->module;
		if( mod && !mod->isBuildWithoutErrors )
			return asERROR;
	}
	
	asCBuilder bld(this, mod);

	asCScriptFunction func(this, mod);
	int r = bld.ParseFunctionDeclaration(decl, &func, false);
	if( r < 0 )
		return asINVALID_DECLARATION;

	// Search for matching factory function
	int id = -1;
	for( size_t n = 0; n < ot->beh.factories.GetLength(); n++ )
	{
		asCScriptFunction *f = scriptFunctions[ot->beh.factories[n]];
		if( f->IsSignatureEqual(&func) )
		{
			id = ot->beh.factories[n];
			break;
		}
	}
	
	if( id == -1 ) return asNO_FUNCTION;

	return id;
}


// internal
int asCScriptEngine::GetMethodIdByDecl(const asCObjectType *ot, const char *decl, asCModule *mod)
{
	if( mod && !mod->isBuildWithoutErrors )
		return asERROR;

	asCBuilder bld(this, mod);

	asCScriptFunction func(this, mod);
	int r = bld.ParseFunctionDeclaration(decl, &func, false);
	if( r < 0 )
		return asINVALID_DECLARATION;

	// Set the object type so that the signature can be properly compared
	// This cast is OK, it will only be used for comparison
	func.objectType = const_cast<asCObjectType*>(ot);

	// Search script functions for matching interface
	int id = -1;
	for( size_t n = 0; n < ot->methods.GetLength(); ++n )
	{
		if( func.IsSignatureEqual(scriptFunctions[ot->methods[n]]) )
		{
			if( id == -1 )
				id = ot->methods[n];
			else
				return asMULTIPLE_FUNCTIONS;
		}
	}

	if( id == -1 ) return asNO_FUNCTION;

	return id;
}


// Internal
asCString asCScriptEngine::GetFunctionDeclaration(int funcID)
{
	asCString str;
	asCScriptFunction *func = GetScriptFunction(funcID);
	if( func )
		str = func->GetDeclarationStr();

	return str;
}

asCScriptFunction *asCScriptEngine::GetScriptFunction(int funcID)
{
	int f = funcID & 0xFFFF;
	if( f >= (int)scriptFunctions.GetLength() )
		return 0;

	return scriptFunctions[f];
}



asIScriptContext *asCScriptEngine::CreateContext()
{
	asIScriptContext *ctx = 0;
	CreateContext(&ctx, false);
	return ctx;
}

int asCScriptEngine::CreateContext(asIScriptContext **context, bool isInternal)
{
	*context = asNEW(asCContext)(this, !isInternal);

	return 0;
}


int asCScriptEngine::RegisterObjectProperty(const char *obj, const char *declaration, int byteOffset)
{
	// Verify that the correct config group is used
	if( currentGroup->FindType(obj) == 0 )
		return asWRONG_CONFIG_GROUP;

	asCDataType dt, type;
	asCString name;

	int r;
	asCBuilder bld(this, 0);
	r = bld.ParseDataType(obj, &dt);
	if( r < 0 )
		return ConfigError(r);

	if( (r = bld.VerifyProperty(&dt, declaration, name, type)) < 0 )
		return ConfigError(r);

	// Store the property info
	if( dt.GetObjectType() == 0 )
		return ConfigError(asINVALID_OBJECT);

	asCObjectProperty *prop = asNEW(asCObjectProperty);
	prop->name       = name;
	prop->type       = type;
	prop->byteOffset = byteOffset;

	dt.GetObjectType()->properties.PushLast(prop);

	currentGroup->RefConfigGroup(FindConfigGroupForObjectType(type.GetObjectType()));

	return asSUCCESS;
}

int asCScriptEngine::RegisterSpecialObjectType(const char *name, int byteSize, asDWORD flags)
{
	// Put the data type in the list
	asCObjectType *type;
	if( strcmp(name, "array<T>") == 0 )
	{
		type = asNEW(asCObjectType)(this);
		defaultArrayObjectType = type;
		type->AddRef();
	}
	else
		return asERROR;

	type->tokenType = ttIdentifier;
	type->name      = name;
	type->arrayType = 0;
	type->size      = byteSize;
	type->flags     = flags;

	// Store it in the object types
	objectTypes.PushLast(type);

	// Add these types to the default config group
	defaultGroup.objTypes.PushLast(type);

	return asSUCCESS;
}

int asCScriptEngine::RegisterInterface(const char *name)
{
	if( name == 0 ) return ConfigError(asINVALID_NAME);

	// Verify if the name has been registered as a type already
	asUINT n;
	for( n = 0; n < objectTypes.GetLength(); n++ )
	{
		if( objectTypes[n] && objectTypes[n]->name == name )
			return asALREADY_REGISTERED;
	}

	// Use builder to parse the datatype
	asCDataType dt;
	asCBuilder bld(this, 0);
	bool oldMsgCallback = msgCallback; msgCallback = false;
	int r = bld.ParseDataType(name, &dt);
	msgCallback = oldMsgCallback;
	if( r >= 0 ) return ConfigError(asERROR);

	// Make sure the name is not a reserved keyword
	asCTokenizer t;
	size_t tokenLen;
	int token = t.GetToken(name, strlen(name), &tokenLen);
	if( token != ttIdentifier || strlen(name) != tokenLen )
		return ConfigError(asINVALID_NAME);

	r = bld.CheckNameConflict(name, 0, 0);
	if( r < 0 )
		return ConfigError(asNAME_TAKEN);

	// Don't have to check against members of object
	// types as they are allowed to use the names

	// Register the object type for the interface
	asCObjectType *st = asNEW(asCObjectType)(this);
	st->arrayType = 0;
	st->flags = asOBJ_REF | asOBJ_SCRIPT_OBJECT;
	st->size = 0; // Cannot be instanciated
	st->name = name;
	st->tokenType = ttIdentifier;

	// Use the default script class behaviours
	st->beh.factory = 0;
	st->beh.addref = scriptTypeBehaviours.beh.addref;
	st->beh.release = scriptTypeBehaviours.beh.release;
	st->beh.copy = 0;

	objectTypes.PushLast(st);
	registeredObjTypes.PushLast(st);

	currentGroup->objTypes.PushLast(st);

	return asSUCCESS;
}

int asCScriptEngine::RegisterInterfaceMethod(const char *intf, const char *declaration)
{
	// Verify that the correct config group is set.
	if( currentGroup->FindType(intf) == 0 )
		return ConfigError(asWRONG_CONFIG_GROUP);

	asCDataType dt;
	asCBuilder bld(this, 0);
	int r = bld.ParseDataType(intf, &dt);
	if( r < 0 )
		return ConfigError(r);

	asCScriptFunction *func = asNEW(asCScriptFunction)(this, 0);
	func->objectType = dt.GetObjectType();

	r = bld.ParseFunctionDeclaration(declaration, func, false);
	if( r < 0 )
	{
		asDELETE(func,asCScriptFunction);
		return ConfigError(asINVALID_DECLARATION);
	}

	// Check name conflicts
	r = bld.CheckNameConflictMember(dt, func->name.AddressOf(), 0, 0);
	if( r < 0 )
	{
		asDELETE(func,asCScriptFunction);
		return ConfigError(asNAME_TAKEN);
	}

	func->id = GetNextScriptFunctionId();
	func->funcType = asFUNC_INTERFACE;
	SetScriptFunction(func);
	func->objectType->methods.PushLast(func->id);

	func->ComputeSignatureId();

	// If parameter type from other groups are used, add references
	if( func->returnType.GetObjectType() )
	{
		asCConfigGroup *group = FindConfigGroup(func->returnType.GetObjectType());
		currentGroup->RefConfigGroup(group);
	}
	for( asUINT n = 0; n < func->parameterTypes.GetLength(); n++ )
	{
		if( func->parameterTypes[n].GetObjectType() )
		{
			asCConfigGroup *group = FindConfigGroup(func->parameterTypes[n].GetObjectType());
			currentGroup->RefConfigGroup(group);
		}
	}

	// Return function id as success
	return func->id;
}

int asCScriptEngine::RegisterObjectType(const char *name, int byteSize, asDWORD flags)
{
	isPrepared = false;

	// Verify flags
	//   Must have either asOBJ_REF or asOBJ_VALUE
	if( flags & asOBJ_REF )
	{
		// Can optionally have the asOBJ_GC, asOBJ_NOHANDLE, or asOBJ_SCOPED flag set, but nothing else
		if( flags & ~(asOBJ_REF | asOBJ_GC | asOBJ_NOHANDLE | asOBJ_SCOPED) )
			return ConfigError(asINVALID_ARG);

		// flags are exclusive
		if( (flags & asOBJ_GC) && (flags & (asOBJ_NOHANDLE|asOBJ_SCOPED)) )
			return ConfigError(asINVALID_ARG);
		if( (flags & asOBJ_NOHANDLE) && (flags & (asOBJ_GC|asOBJ_SCOPED)) )
			return ConfigError(asINVALID_ARG);
		if( (flags & asOBJ_SCOPED) && (flags & (asOBJ_GC|asOBJ_NOHANDLE)) )
			return ConfigError(asINVALID_ARG);
	}
	else if( flags & asOBJ_VALUE )
	{
		// Cannot use reference flags
		if( flags & (asOBJ_REF | asOBJ_GC | asOBJ_SCOPED) )
			return ConfigError(asINVALID_ARG);

		// Must have either asOBJ_APP_CLASS, asOBJ_APP_PRIMITIVE, or asOBJ_APP_FLOAT
		if( flags & asOBJ_APP_CLASS )
		{
			// Must not set the primitive or float flag
			if( flags & (asOBJ_APP_PRIMITIVE |
				         asOBJ_APP_FLOAT) )
				return ConfigError(asINVALID_ARG);
		}
		else if( flags & asOBJ_APP_PRIMITIVE )
		{
			// Must not set the class flags nor the float flag
			if( flags & (asOBJ_APP_CLASS             |
				         asOBJ_APP_CLASS_CONSTRUCTOR |
						 asOBJ_APP_CLASS_DESTRUCTOR  |
						 asOBJ_APP_CLASS_ASSIGNMENT  |
						 asOBJ_APP_FLOAT) )
				return ConfigError(asINVALID_ARG);
		}
		else if( flags & asOBJ_APP_FLOAT )
		{
			// Must not set the class flags nor the primitive flag
			if( flags & (asOBJ_APP_CLASS             |
				         asOBJ_APP_CLASS_CONSTRUCTOR |
						 asOBJ_APP_CLASS_DESTRUCTOR  |
						 asOBJ_APP_CLASS_ASSIGNMENT  |
						 asOBJ_APP_PRIMITIVE) )
				return ConfigError(asINVALID_ARG);
		}
		else
		{
			// The application type was not informed
			// TODO: This should be ok, as long as the application doesn't register any functions
			//       that take the type by value or return it by value using native calling convention
			return ConfigError(asINVALID_ARG);
		}
	}
	else
		return ConfigError(asINVALID_ARG);

	// Don't allow anything else than the defined flags
	if( flags - (flags & asOBJ_MASK_VALID_FLAGS) )
		return ConfigError(asINVALID_ARG);

	// Value types must have a defined size
	if( (flags & asOBJ_VALUE) && byteSize == 0 )
	{
		WriteMessage("", 0, 0, asMSGTYPE_ERROR, TXT_VALUE_TYPE_MUST_HAVE_SIZE);
		return ConfigError(asINVALID_ARG);
	}

	// Verify type name
	if( name == 0 )
		return ConfigError(asINVALID_NAME);

	// Verify if the name has been registered as a type already
	asUINT n;
	for( n = 0; n < objectTypes.GetLength(); n++ )
	{
		if( objectTypes[n] && objectTypes[n]->name == name )
			return asALREADY_REGISTERED;
	}

	for( n = 0; n < arrayTypes.GetLength(); n++ )
	{
		if( arrayTypes[n] && arrayTypes[n]->name == name )
			return asALREADY_REGISTERED;
	}

	// Verify the most recently created script array type
	asCObjectType *mostRecentScriptArrayType = 0;
	if( scriptArrayTypes.GetLength() )
		mostRecentScriptArrayType = scriptArrayTypes[scriptArrayTypes.GetLength()-1];

	// Use builder to parse the datatype
	asCDataType dt;
	asCBuilder bld(this, 0);
	bool oldMsgCallback = msgCallback; msgCallback = false;
	int r = bld.ParseDataType(name, &dt);
	msgCallback = oldMsgCallback;

	// If the builder fails, then the type name
	// is new and it should be registered
	if( r < 0 )
	{
		// Make sure the name is not a reserved keyword
		asCTokenizer t;
		size_t tokenLen;
		int token = t.GetToken(name, strlen(name), &tokenLen);
		if( token != ttIdentifier || strlen(name) != tokenLen )
			return ConfigError(asINVALID_NAME);

		int r = bld.CheckNameConflict(name, 0, 0);
		if( r < 0 )
			return ConfigError(asNAME_TAKEN);

		// Don't have to check against members of object
		// types as they are allowed to use the names

		// Put the data type in the list
		asCObjectType *type = asNEW(asCObjectType)(this);
		type->name      = name;
		type->tokenType = ttIdentifier;
		type->arrayType = 0;
		type->size      = byteSize;
		type->flags     = flags;

		objectTypes.PushLast(type);
		registeredObjTypes.PushLast(type);

		currentGroup->objTypes.PushLast(type);
	}
	else
	{
		// TODO: Template: This ought to be covered automatically by the new template code
		// int[][] must not be allowed to be registered
		// if int[] hasn't been registered first
		if( dt.GetSubType().IsTemplate() )
			return ConfigError(asLOWER_ARRAY_DIMENSION_NOT_REGISTERED);

		if( dt.IsReadOnly() ||
			dt.IsReference() )
			return ConfigError(asINVALID_TYPE);

		// Was the script array type created before?
		if( scriptArrayTypes[scriptArrayTypes.GetLength()-1] == mostRecentScriptArrayType ||
			mostRecentScriptArrayType == dt.GetObjectType() )
			return ConfigError(asNOT_SUPPORTED);

		// TODO: Add this again. The type is used by the factory stubs so we need to discount that
		// Is the script array type already being used?
//		if( dt.GetObjectType()->GetRefCount() > 1 )
//			return ConfigError(asNOT_SUPPORTED);

		// Put the data type in the list
		asCObjectType *type = asNEW(asCObjectType)(this);
		type->name      = name;
		type->subType   = dt.GetSubType().GetObjectType();
		if( type->subType ) type->subType->AddRef();
		type->tokenType = dt.GetSubType().GetTokenType();
		type->arrayType = dt.GetArrayType();
		type->size      = byteSize;
		type->flags     = flags;

		arrayTypes.PushLast(type);

		currentGroup->objTypes.PushLast(type);

		// Remove the built-in array type, which will no longer be used.
		RemoveArrayType(dt.GetObjectType());
	}

	return asSUCCESS;
}

int asCScriptEngine::RegisterSpecialObjectBehaviour(asCObjectType *objType, asDWORD behaviour, const char *decl, const asSFuncPtr &funcPointer, int callConv)
{
	asASSERT( objType );

	asSSystemFunctionInterface internal;
	if( behaviour == asBEHAVE_FACTORY )
	{
		int r = DetectCallingConvention(false, funcPointer, callConv, &internal);
		if( r < 0 )
			return ConfigError(r);
	}
	else
	{
		int r = DetectCallingConvention(true, funcPointer, callConv, &internal);
		if( r < 0 )
			return ConfigError(r);
	}

	isPrepared = false;

	asCBuilder bld(this, 0);

	asSTypeBehaviour *beh;
	asCDataType type;

	// TODO: Template: When the template feature is completed, this function should only be used for script classes
	bool isDefaultArray = (objType->flags & asOBJ_TEMPLATE) ? true : false;

	if( isDefaultArray )
		type = asCDataType::CreateDefaultArray(this);
	else if( objType->flags & asOBJ_SCRIPT_OBJECT )
	{
		type.MakeHandle(false);
		type.MakeReadOnly(false);
		type.MakeReference(false);
		type.SetObjectType(objType);
		type.SetTokenType(ttIdentifier);
	}

	beh = type.GetBehaviour();

	// The object is sent by reference to the function
	type.MakeReference(true);

	// Verify function declaration
	asCScriptFunction func(this, 0);

	// TODO: Template: When parsing methods for the template type, the builder needs to know about the
	// template subtype so that it will recognize it as a legal datatype.

	// TODO: Template: How do we make the code handle the template subtype? The engine needs to have a
	// special object type, so that asCDataType can represent the generic template subtype. Need to tell
	// the builder/parser that the type T is a template subtype.

	// The default array object is actually being registered
	// with incorrect declarations, but that's a concious decision
	bld.ParseFunctionDeclaration(decl, &func, true, &internal.paramAutoHandles, &internal.returnAutoHandle);

	if( isDefaultArray )
		func.objectType = defaultArrayObjectType;
	else
		func.objectType = objType;

	if( behaviour == asBEHAVE_CONSTRUCT )
	{
		// Verify that the return type is void
		if( func.returnType != asCDataType::CreatePrimitive(ttVoid, false) )
			return ConfigError(asINVALID_DECLARATION);

		if( objType->flags & asOBJ_SCRIPT_OBJECT )
		{
			if( func.parameterTypes.GetLength() == 1 )
			{
				beh->construct = AddBehaviourFunction(func, internal);
				beh->factory   = beh->construct;
				beh->constructors.PushLast(beh->construct);
				beh->factories.PushLast(beh->factory);
			}
			else
			{
				int id = AddBehaviourFunction(func, internal);
				beh->constructors.PushLast(id);
				beh->factories.PushLast(id);
			}
		}
	}
	else if( behaviour == asBEHAVE_FACTORY )
	{
		if( objType->flags & asOBJ_TEMPLATE )
		{
			if( func.parameterTypes.GetLength() == 1 )
			{
				beh->factory = AddBehaviourFunction(func, internal);
				beh->factories.PushLast(beh->factory);
			}
			else
			{
				beh->factories.PushLast(AddBehaviourFunction(func, internal));
			}
		}
	}
	else if( behaviour == asBEHAVE_ADDREF )
	{
		if( beh->addref )
			return ConfigError(asALREADY_REGISTERED);

		// Verify that the return type is void
		if( func.returnType != asCDataType::CreatePrimitive(ttVoid, false) )
			return ConfigError(asINVALID_DECLARATION);

		// Verify that there are no parameters
		if( func.parameterTypes.GetLength() > 0 )
			return ConfigError(asINVALID_DECLARATION);

		beh->addref = AddBehaviourFunction(func, internal);
	}
	else if( behaviour == asBEHAVE_RELEASE)
	{
		if( beh->release )
			return ConfigError(asALREADY_REGISTERED);

		// Verify that the return type is void
		if( func.returnType != asCDataType::CreatePrimitive(ttVoid, false) )
			return ConfigError(asINVALID_DECLARATION);

		// Verify that there are no parameters
		if( func.parameterTypes.GetLength() > 0 )
			return ConfigError(asINVALID_DECLARATION);

		beh->release = AddBehaviourFunction(func, internal);
	}
	else if( behaviour >= asBEHAVE_FIRST_ASSIGN && behaviour <= asBEHAVE_LAST_ASSIGN )
	{
		// Verify that there is exactly one parameter
		if( func.parameterTypes.GetLength() != 1 )
			return ConfigError(asINVALID_DECLARATION);

		if( objType->flags & (asOBJ_SCRIPT_OBJECT | asOBJ_TEMPLATE) )
		{
			if( beh->copy )
				return ConfigError(asALREADY_REGISTERED);

			beh->copy = AddBehaviourFunction(func, internal);

			beh->operators.PushLast(ttAssignment);
			beh->operators.PushLast(beh->copy);
		}
	}
	else if( behaviour == asBEHAVE_INDEX )
	{
		// Verify that there is only one parameter
		if( func.parameterTypes.GetLength() != 1 )
			return ConfigError(asINVALID_DECLARATION);

		// Verify that the return type is not void
		if( func.returnType.GetTokenType() == ttVoid )
			return ConfigError(asINVALID_DECLARATION);

		// TODO: Verify that the operator hasn't been registered already

		beh->operators.PushLast(behaviour);
		beh->operators.PushLast(AddBehaviourFunction(func, internal));
	}
	else if( behaviour >= asBEHAVE_FIRST_GC &&
		     behaviour <= asBEHAVE_LAST_GC )
	{
		// Register the gc behaviours

		// Verify parameter count
		if( (behaviour == asBEHAVE_GETREFCOUNT ||
			 behaviour == asBEHAVE_SETGCFLAG   ||
			 behaviour == asBEHAVE_GETGCFLAG) &&
			func.parameterTypes.GetLength() != 0 )
			return ConfigError(asINVALID_DECLARATION);

		if( (behaviour == asBEHAVE_ENUMREFS ||
			 behaviour == asBEHAVE_RELEASEREFS) &&
			func.parameterTypes.GetLength() != 1 )
			return ConfigError(asINVALID_DECLARATION);

		// Verify return type
		if( behaviour == asBEHAVE_GETREFCOUNT &&
			func.returnType != asCDataType::CreatePrimitive(ttInt, false) )
			return ConfigError(asINVALID_DECLARATION);

		if( behaviour == asBEHAVE_GETGCFLAG &&
			func.returnType != asCDataType::CreatePrimitive(ttBool, false) )
			return ConfigError(asINVALID_DECLARATION);

		if( (behaviour == asBEHAVE_SETGCFLAG ||
			 behaviour == asBEHAVE_ENUMREFS  ||
			 behaviour == asBEHAVE_RELEASEREFS) &&
			func.returnType != asCDataType::CreatePrimitive(ttVoid, false) )
			return ConfigError(asINVALID_DECLARATION);

		if( behaviour == asBEHAVE_GETREFCOUNT )
			beh->gcGetRefCount = AddBehaviourFunction(func, internal);
		else if( behaviour == asBEHAVE_SETGCFLAG )
			beh->gcSetFlag = AddBehaviourFunction(func, internal);
		else if( behaviour == asBEHAVE_GETGCFLAG )
			beh->gcGetFlag = AddBehaviourFunction(func, internal);
		else if( behaviour == asBEHAVE_ENUMREFS )
			beh->gcEnumReferences = AddBehaviourFunction(func, internal);
		else if( behaviour == asBEHAVE_RELEASEREFS )
			beh->gcReleaseAllReferences = AddBehaviourFunction(func, internal);
	}
	else
	{
		asASSERT(false);

		return ConfigError(asINVALID_ARG);
	}

	return asSUCCESS;
}

int asCScriptEngine::RegisterObjectBehaviour(const char *datatype, asEBehaviours behaviour, const char *decl, const asSFuncPtr &funcPointer, asDWORD callConv)
{
	// Verify that the correct config group is used
	if( currentGroup->FindType(datatype) == 0 )
		return ConfigError(asWRONG_CONFIG_GROUP);

	if( datatype == 0 ) return ConfigError(asINVALID_ARG);

	asSSystemFunctionInterface internal;
	if( behaviour == asBEHAVE_FACTORY )
	{
		int r = DetectCallingConvention(false, funcPointer, callConv, &internal);
		if( r < 0 )
			return ConfigError(r);
	}
	else
	{
#ifdef AS_MAX_PORTABILITY
		if( callConv != asCALL_GENERIC )
			return ConfigError(asNOT_SUPPORTED);
#else
		if( callConv != asCALL_THISCALL &&
			callConv != asCALL_CDECL_OBJLAST &&
			callConv != asCALL_CDECL_OBJFIRST &&
			callConv != asCALL_GENERIC )
			return ConfigError(asNOT_SUPPORTED);
#endif

		int r = DetectCallingConvention(datatype != 0, funcPointer, callConv, &internal);
		if( r < 0 )
			return ConfigError(r);
	}

	isPrepared = false;

	asCBuilder bld(this, 0);

	asSTypeBehaviour *beh;
	asCDataType type;

	int r = bld.ParseDataType(datatype, &type);
	if( r < 0 )
		return ConfigError(r);

	if( type.IsReadOnly() || type.IsReference() )
		return ConfigError(asINVALID_TYPE);

	// Verify that the type is allowed
	if( type.GetObjectType() == 0 )
		return ConfigError(asINVALID_TYPE);

	beh = type.GetBehaviour();

	// The object is sent by reference to the function
	type.MakeReference(true);

	// Verify function declaration
	asCScriptFunction func(this, 0);

	r = bld.ParseFunctionDeclaration(decl, &func, true, &internal.paramAutoHandles, &internal.returnAutoHandle);
	if( r < 0 )
		return ConfigError(asINVALID_DECLARATION);

	if( behaviour != asBEHAVE_FACTORY )
		func.objectType = type.GetObjectType();

	if( behaviour == asBEHAVE_CONSTRUCT )
	{
		// TODO: Add asBEHAVE_IMPLICIT_CONSTRUCT

		// Verify that it is a value type
		if( !(func.objectType->flags & asOBJ_VALUE) )
		{
			WriteMessage("", 0, 0, asMSGTYPE_ERROR, TXT_ILLEGAL_BEHAVIOUR_FOR_TYPE);
			return ConfigError(asILLEGAL_BEHAVIOUR_FOR_TYPE);
		}

		// Verify that the return type is void
		if( func.returnType != asCDataType::CreatePrimitive(ttVoid, false) )
			return ConfigError(asINVALID_DECLARATION);

		// Implicit constructors must take one and only one parameter
/*		if( behaviour == asBEHAVE_IMPLICIT_CONSTRUCT &&
			func.parameterTypes.GetLength() != 1 )
			return ConfigError(asINVALID_DECLARATION);
*/
		// TODO: Verify that the same constructor hasn't been registered already

		// Store all constructors in a list
		if( func.parameterTypes.GetLength() == 0 )
		{
			func.id = beh->construct = AddBehaviourFunction(func, internal);
			beh->constructors.PushLast(beh->construct);
		}
		else
		{
			func.id = AddBehaviourFunction(func, internal);
			beh->constructors.PushLast(func.id);
/*
			if( behaviour == asBEHAVE_IMPLICIT_CONSTRUCT )
			{
				beh->operators.PushLast(behaviour);
				beh->operators.PushLast(func.id);
			}
*/		}
	}
	else if( behaviour == asBEHAVE_DESTRUCT )
	{
		// Must be a value type
		if( !(func.objectType->flags & asOBJ_VALUE) )
		{
			WriteMessage("", 0, 0, asMSGTYPE_ERROR, TXT_ILLEGAL_BEHAVIOUR_FOR_TYPE);
			return ConfigError(asILLEGAL_BEHAVIOUR_FOR_TYPE);
		}

		if( beh->destruct )
			return ConfigError(asALREADY_REGISTERED);

		// Verify that the return type is void
		if( func.returnType != asCDataType::CreatePrimitive(ttVoid, false) )
			return ConfigError(asINVALID_DECLARATION);

		// Verify that there are no parameters
		if( func.parameterTypes.GetLength() > 0 )
			return ConfigError(asINVALID_DECLARATION);

		func.id = beh->destruct = AddBehaviourFunction(func, internal);
	}
	else if( behaviour == asBEHAVE_FACTORY )
	{
		// TODO: Add asBEHAVE_IMPLICIT_FACTORY

		// Must be a ref type and must not have asOBJ_NOHANDLE
		if( !(type.GetObjectType()->flags & asOBJ_REF) || (type.GetObjectType()->flags & asOBJ_NOHANDLE) )
		{
			WriteMessage("", 0, 0, asMSGTYPE_ERROR, TXT_ILLEGAL_BEHAVIOUR_FOR_TYPE);
			return ConfigError(asILLEGAL_BEHAVIOUR_FOR_TYPE);
		}

		// Verify that the return type is a handle to the type
		if( func.returnType != asCDataType::CreateObjectHandle(type.GetObjectType(), false) )
			return ConfigError(asINVALID_DECLARATION);

		// Implicit factories must take one and only one parameter
/*		if( behaviour == asBEHAVE_IMPLICIT_FACTORY &&
			func.parameterTypes.GetLength() != 1 )
			return ConfigError(asINVALID_DECLARATION);
*/
		// TODO: Verify that the same factory function hasn't been registered already

		// Store all factory functions in a list
		if( func.parameterTypes.GetLength() == 0 )
		{
			func.id = beh->factory = AddBehaviourFunction(func, internal);
			beh->factories.PushLast(beh->factory);
		}
		else
		{
			func.id = AddBehaviourFunction(func, internal);
			beh->factories.PushLast(func.id);
/*
			if( behaviour == asBEHAVE_IMPLICIT_FACTORY )
			{
				beh->operators.PushLast(behaviour);
				beh->operators.PushLast(func.id);
			}
*/		}
	}
	else if( behaviour == asBEHAVE_ADDREF )
	{
		// Must be a ref type and must not have asOBJ_NOHANDLE, nor asOBJ_SCOPED
		if( !(func.objectType->flags & asOBJ_REF) ||
			(func.objectType->flags & asOBJ_NOHANDLE) ||
			(func.objectType->flags & asOBJ_SCOPED) )
		{
			WriteMessage("", 0, 0, asMSGTYPE_ERROR, TXT_ILLEGAL_BEHAVIOUR_FOR_TYPE);
			return ConfigError(asILLEGAL_BEHAVIOUR_FOR_TYPE);
		}

		if( beh->addref )
			return ConfigError(asALREADY_REGISTERED);

		// Verify that the return type is void
		if( func.returnType != asCDataType::CreatePrimitive(ttVoid, false) )
			return ConfigError(asINVALID_DECLARATION);

		// Verify that there are no parameters
		if( func.parameterTypes.GetLength() > 0 )
			return ConfigError(asINVALID_DECLARATION);

		func.id = beh->addref = AddBehaviourFunction(func, internal);
	}
	else if( behaviour == asBEHAVE_RELEASE)
	{
		// Must be a ref type and must not have asOBJ_NOHANDLE
		if( !(func.objectType->flags & asOBJ_REF) || (func.objectType->flags & asOBJ_NOHANDLE) )
		{
			WriteMessage("", 0, 0, asMSGTYPE_ERROR, TXT_ILLEGAL_BEHAVIOUR_FOR_TYPE);
			return ConfigError(asILLEGAL_BEHAVIOUR_FOR_TYPE);
		}

		if( beh->release )
			return ConfigError(asALREADY_REGISTERED);

		// Verify that the return type is void
		if( func.returnType != asCDataType::CreatePrimitive(ttVoid, false) )
			return ConfigError(asINVALID_DECLARATION);

		// Verify that there are no parameters
		if( func.parameterTypes.GetLength() > 0 )
			return ConfigError(asINVALID_DECLARATION);

		func.id = beh->release = AddBehaviourFunction(func, internal);
	}
	else if( behaviour >= asBEHAVE_FIRST_ASSIGN && behaviour <= asBEHAVE_LAST_ASSIGN )
	{
		// Verify that the var type is not used
		if( VerifyVarTypeNotInFunction(&func) < 0 )
			return ConfigError(asINVALID_DECLARATION);

		// Verify that there is exactly one parameter
		if( func.parameterTypes.GetLength() != 1 )
			return ConfigError(asINVALID_DECLARATION);

		// Verify that the return type is a reference to the object type
		if( func.returnType != type )
			return ConfigError(asINVALID_DECLARATION);

		if( !ep.allowUnsafeReferences )
		{
			// Verify that the rvalue is marked as in if a reference
			if( func.parameterTypes[0].IsReference() && func.inOutFlags[0] != 1 )
				return ConfigError(asINVALID_DECLARATION);
		}

		if( behaviour == asBEHAVE_ASSIGNMENT && func.parameterTypes[0].IsEqualExceptConst(type) )
		{
			if( beh->copy )
				return ConfigError(asALREADY_REGISTERED);

			func.id = beh->copy = AddBehaviourFunction(func, internal);

			beh->operators.PushLast(behaviour);
			beh->operators.PushLast(beh->copy);
		}
		else
		{
			// TODO: Verify that the operator hasn't been registered with the same parameter already

			beh->operators.PushLast(behaviour);
			func.id = AddBehaviourFunction(func, internal);
			beh->operators.PushLast(func.id);
		}
	}
	else if( behaviour == asBEHAVE_INDEX )
	{
		// Verify that the var type is not used
		if( VerifyVarTypeNotInFunction(&func) < 0 )
			return ConfigError(asINVALID_DECLARATION);

		// Verify that there is only one parameter
		if( func.parameterTypes.GetLength() != 1 )
			return ConfigError(asINVALID_DECLARATION);

		// Verify that the return type is not void
		if( func.returnType.GetTokenType() == ttVoid )
			return ConfigError(asINVALID_DECLARATION);

		// TODO: Verify that the operator hasn't been registered already

		beh->operators.PushLast(behaviour);
		func.id = AddBehaviourFunction(func, internal);
		beh->operators.PushLast(func.id);
	}
	// TODO: We also need asBEHAVE_BIT_NOT
	else if( behaviour == asBEHAVE_NEGATE )
	{
		// Verify that there are no parameters
		if( func.parameterTypes.GetLength() != 0 )
			return ConfigError(asINVALID_DECLARATION);

		// Verify that the return type is a the same as the type
		type.MakeReference(false);
		if( type.GetObjectType()->flags & asOBJ_SCOPED )
		{
			// Scoped types must be returned by handle
			asCDataType t = type;
			t.MakeHandle(true, true);
			if( func.returnType != t )
				return ConfigError(asINVALID_DECLARATION);
		}
		else if( func.returnType != type )
			return ConfigError(asINVALID_DECLARATION);

		// TODO: Verify that the operator hasn't been registered already

		beh->operators.PushLast(behaviour);
		func.id = AddBehaviourFunction(func, internal);
		beh->operators.PushLast(func.id);
	}
	else if( behaviour >= asBEHAVE_FIRST_GC &&
		     behaviour <= asBEHAVE_LAST_GC )
	{
		// Only allow GC behaviours for types registered to be garbage collected
		if( !(func.objectType->flags & asOBJ_GC) )
		{
			WriteMessage("", 0, 0, asMSGTYPE_ERROR, TXT_ILLEGAL_BEHAVIOUR_FOR_TYPE);
			return ConfigError(asILLEGAL_BEHAVIOUR_FOR_TYPE);
		}

		// Verify parameter count
		if( (behaviour == asBEHAVE_GETREFCOUNT ||
			 behaviour == asBEHAVE_SETGCFLAG   ||
			 behaviour == asBEHAVE_GETGCFLAG) &&
			func.parameterTypes.GetLength() != 0 )
			return ConfigError(asINVALID_DECLARATION);

		if( (behaviour == asBEHAVE_ENUMREFS ||
			 behaviour == asBEHAVE_RELEASEREFS) &&
			func.parameterTypes.GetLength() != 1 )
			return ConfigError(asINVALID_DECLARATION);

		// Verify return type
		if( behaviour == asBEHAVE_GETREFCOUNT &&
			func.returnType != asCDataType::CreatePrimitive(ttInt, false) )
			return ConfigError(asINVALID_DECLARATION);

		if( behaviour == asBEHAVE_GETGCFLAG &&
			func.returnType != asCDataType::CreatePrimitive(ttBool, false) )
			return ConfigError(asINVALID_DECLARATION);

		if( (behaviour == asBEHAVE_SETGCFLAG ||
			 behaviour == asBEHAVE_ENUMREFS  ||
			 behaviour == asBEHAVE_RELEASEREFS) &&
			func.returnType != asCDataType::CreatePrimitive(ttVoid, false) )
			return ConfigError(asINVALID_DECLARATION);

		if( behaviour == asBEHAVE_GETREFCOUNT )
			func.id = beh->gcGetRefCount = AddBehaviourFunction(func, internal);
		else if( behaviour == asBEHAVE_SETGCFLAG )
			func.id = beh->gcSetFlag = AddBehaviourFunction(func, internal);
		else if( behaviour == asBEHAVE_GETGCFLAG )
			func.id = beh->gcGetFlag = AddBehaviourFunction(func, internal);
		else if( behaviour == asBEHAVE_ENUMREFS )
			func.id = beh->gcEnumReferences = AddBehaviourFunction(func, internal);
		else if( behaviour == asBEHAVE_RELEASEREFS )
			func.id = beh->gcReleaseAllReferences = AddBehaviourFunction(func, internal);
	}
	else if( behaviour == asBEHAVE_IMPLICIT_VALUE_CAST ||
		     behaviour == asBEHAVE_VALUE_CAST )
	{
		// Verify parameter count
		if( func.parameterTypes.GetLength() != 0 )
			return ConfigError(asINVALID_DECLARATION);

		// Verify return type
		if( func.returnType.IsEqualExceptRefAndConst(asCDataType::CreatePrimitive(ttBool, false)) )
			return ConfigError(asNOT_SUPPORTED);

		if( func.returnType.IsEqualExceptRefAndConst(asCDataType::CreatePrimitive(ttVoid, false)) )
			return ConfigError(asINVALID_DECLARATION);

		// TODO: verify that the same cast is not registered already (const or non-const is treated the same for the return type)

		beh->operators.PushLast(behaviour);
		func.id = AddBehaviourFunction(func, internal);
		beh->operators.PushLast(func.id);
	}
	else
	{
		if( (behaviour >= asBEHAVE_FIRST_DUAL &&
			 behaviour <= asBEHAVE_LAST_DUAL) ||
			behaviour == asBEHAVE_REF_CAST )
		{
			WriteMessage("", 0, 0, asMSGTYPE_ERROR, TXT_MUST_BE_GLOBAL_BEHAVIOUR);
		}
		else
			asASSERT(false);

		return ConfigError(asINVALID_ARG);
	}

	// Return function id as success
	return func.id;
}


// TODO: RegisterGlobalBehaviour should be removed once all behaviours are registered to objects
// interface
int asCScriptEngine::RegisterGlobalBehaviour(asEBehaviours behaviour, const char *decl, const asSFuncPtr &funcPointer, asDWORD callConv)
{
	asSSystemFunctionInterface internal;
	int r = DetectCallingConvention(false, funcPointer, callConv, &internal);
	if( r < 0 )
		return ConfigError(r);

	isPrepared = false;

	asCBuilder bld(this, 0);

#ifdef AS_MAX_PORTABILITY
	if( callConv != asCALL_GENERIC )
		return ConfigError(asNOT_SUPPORTED);
#else
	if( callConv != asCALL_CDECL &&
		callConv != asCALL_STDCALL &&
		callConv != asCALL_GENERIC )
		return ConfigError(asNOT_SUPPORTED);
#endif

	// We need a global behaviour structure
	asSTypeBehaviour *beh = &globalBehaviours;

	// Verify function declaration
	asCScriptFunction func(this, 0);

	r = bld.ParseFunctionDeclaration(decl, &func, true, &internal.paramAutoHandles, &internal.returnAutoHandle);
	if( r < 0 )
		return ConfigError(asINVALID_DECLARATION);

	if( behaviour >= asBEHAVE_FIRST_DUAL && behaviour <= asBEHAVE_LAST_DUAL )
	{
		// Verify that the var type is not used
		if( VerifyVarTypeNotInFunction(&func) < 0 )
			return ConfigError(asINVALID_DECLARATION);

		// Verify that there are exactly two parameters
		if( func.parameterTypes.GetLength() != 2 )
			return ConfigError(asINVALID_DECLARATION);

		// Verify that the return type is not void
		if( func.returnType.GetTokenType() == ttVoid )
			return ConfigError(asINVALID_DECLARATION);

		// Verify that at least one of the parameters is a registered type
		if( !(func.parameterTypes[0].GetTokenType() == ttIdentifier) &&
			!(func.parameterTypes[1].GetTokenType() == ttIdentifier) )
			return ConfigError(asINVALID_DECLARATION);

		// TODO: Verify that the operator hasn't been registered with the same parameters already

		beh->operators.PushLast(behaviour);
		func.id = AddBehaviourFunction(func, internal);
		beh->operators.PushLast(func.id);
		currentGroup->globalBehaviours.PushLast((int)beh->operators.GetLength()-2);
	}
	else if( behaviour == asBEHAVE_REF_CAST ||
		     behaviour == asBEHAVE_IMPLICIT_REF_CAST )
	{
		// Verify that the var type not used
		if( VerifyVarTypeNotInFunction(&func) < 0 )
			return ConfigError(asINVALID_DECLARATION);

		// Verify that the only parameter is a handle
		if( func.parameterTypes.GetLength() != 1 ||
			!func.parameterTypes[0].IsObjectHandle() )
			return ConfigError(asINVALID_DECLARATION);

		// Verify that the return type is a handle
		if( !func.returnType.IsObjectHandle() )
			return ConfigError(asINVALID_DECLARATION);

		beh->operators.PushLast(behaviour);
		func.id = AddBehaviourFunction(func, internal);
		beh->operators.PushLast(func.id);
		currentGroup->globalBehaviours.PushLast((int)beh->operators.GetLength()-2);
	}
	else
	{
		return ConfigError(asINVALID_ARG);
	}

	// Return the function id as success
	return func.id;
}

// interface
int asCScriptEngine::GetGlobalBehaviourCount()
{
	return (int)globalBehaviours.operators.GetLength()/2;
}

// interface
int asCScriptEngine::GetGlobalBehaviourByIndex(asUINT index, asEBehaviours *outBehaviour)
{
	if( index*2 >= globalBehaviours.operators.GetLength() )
		return asINVALID_ARG;

	if( outBehaviour )
		*outBehaviour = static_cast<asEBehaviours>(globalBehaviours.operators[index*2]);

	return globalBehaviours.operators[index*2+1];
}

int asCScriptEngine::VerifyVarTypeNotInFunction(asCScriptFunction *func)
{
	// Don't allow var type in this function
	if( func->returnType.GetTokenType() == ttQuestion )
		return asINVALID_DECLARATION;

	for( unsigned int n = 0; n < func->parameterTypes.GetLength(); n++ )
		if( func->parameterTypes[n].GetTokenType() == ttQuestion )
			return asINVALID_DECLARATION;

	return 0;
}

int asCScriptEngine::AddBehaviourFunction(asCScriptFunction &func, asSSystemFunctionInterface &internal)
{
	asUINT n;

	int id = GetNextScriptFunctionId();

	asSSystemFunctionInterface *newInterface = asNEW(asSSystemFunctionInterface);
	newInterface->func               = internal.func;
	newInterface->baseOffset         = internal.baseOffset;
	newInterface->callConv           = internal.callConv;
	newInterface->scriptReturnSize   = internal.scriptReturnSize;
	newInterface->hostReturnInMemory = internal.hostReturnInMemory;
	newInterface->hostReturnFloat    = internal.hostReturnFloat;
	newInterface->hostReturnSize     = internal.hostReturnSize;
	newInterface->paramSize          = internal.paramSize;
	newInterface->takesObjByVal      = internal.takesObjByVal;
	newInterface->paramAutoHandles   = internal.paramAutoHandles;
	newInterface->returnAutoHandle   = internal.returnAutoHandle;
	newInterface->hasAutoHandles     = internal.hasAutoHandles;

	asCScriptFunction *f = asNEW(asCScriptFunction)(this, 0);
	f->funcType    = asFUNC_SYSTEM;
	f->sysFuncIntf = newInterface;
	f->returnType  = func.returnType;
	f->objectType  = func.objectType;
	f->id          = id;
	f->isReadOnly  = func.isReadOnly;
	for( n = 0; n < func.parameterTypes.GetLength(); n++ )
	{
		f->parameterTypes.PushLast(func.parameterTypes[n]);
		f->inOutFlags.PushLast(func.inOutFlags[n]);
	}

	SetScriptFunction(f);

	// If parameter type from other groups are used, add references
	if( f->returnType.GetObjectType() )
	{
		asCConfigGroup *group = FindConfigGroup(f->returnType.GetObjectType());
		currentGroup->RefConfigGroup(group);
	}
	for( n = 0; n < f->parameterTypes.GetLength(); n++ )
	{
		if( f->parameterTypes[n].GetObjectType() )
		{
			asCConfigGroup *group = FindConfigGroup(f->parameterTypes[n].GetObjectType());
			currentGroup->RefConfigGroup(group);
		}
	}

	return id;
}

// interface
int asCScriptEngine::RegisterGlobalProperty(const char *declaration, void *pointer)
{
	asCDataType type;
	asCString name;

	int r;
	asCBuilder bld(this, 0);
	if( (r = bld.VerifyProperty(0, declaration, name, type)) < 0 )
		return ConfigError(r);

	// Don't allow registering references as global properties
	if( type.IsReference() )
		return ConfigError(asINVALID_TYPE);

	// Store the property info
	asCGlobalProperty *prop = asNEW(asCGlobalProperty);
	prop->name       = name;
	prop->type       = type;
	prop->index      = -1 - (int)globalPropAddresses.GetLength();

	// TODO: Reuse slots emptied when removing configuration groups
	registeredGlobalProps.PushLast(prop);
	globalPropAddresses.PushLast(pointer);

	currentGroup->globalProps.PushLast(prop);

	// If from another group add reference
	if( type.GetObjectType() )
	{
		asCConfigGroup *group = FindConfigGroup(type.GetObjectType());
		currentGroup->RefConfigGroup(group);
	}

	if( type.IsObject() && !type.IsReference() && !type.IsObjectHandle() )
	{
		// Create a pointer to a pointer, by storing the object pointer in the global property structure
		prop->memory = pointer;
		globalPropAddresses[-prop->index -1] = &prop->memory;
	}

	return asSUCCESS;
}

// interface
int asCScriptEngine::GetGlobalPropertyCount()
{
	return (int)registeredGlobalProps.GetLength();
}

// interface
// TODO: If the typeId ever encodes the const flag, then the isConst parameter should be removed
int asCScriptEngine::GetGlobalPropertyByIndex(asUINT index, const char **name, int *typeId, bool *isConst, const char **configGroup, void **pointer)
{
	if( index >= registeredGlobalProps.GetLength() )
		return asINVALID_ARG;

	if( name )
		*name = registeredGlobalProps[index]->name.AddressOf();

	if( configGroup )
	{
		asCConfigGroup *group = FindConfigGroupForGlobalVar(index);
		if( group )
			*configGroup = group->groupName.AddressOf();
		else
			*configGroup = 0;
	}

	if( typeId )
		*typeId = GetTypeIdFromDataType(registeredGlobalProps[index]->type);

	if( isConst )
		*isConst = registeredGlobalProps[index]->type.IsReadOnly();

	if( pointer )
		*pointer = globalPropAddresses[-1 - registeredGlobalProps[index]->index];

	return asSUCCESS;
}

int asCScriptEngine::RegisterSpecialObjectMethod(const char *obj, const char *declaration, const asSFuncPtr &funcPointer, int callConv)
{
	asSSystemFunctionInterface internal;
	int r = DetectCallingConvention(obj != 0, funcPointer, callConv, &internal);
	if( r < 0 )
		return ConfigError(r);

	// We only support these calling conventions for object methods
	if( (unsigned)callConv != asCALL_THISCALL &&
		(unsigned)callConv != asCALL_CDECL_OBJLAST &&
		(unsigned)callConv != asCALL_CDECL_OBJFIRST &&
		(unsigned)callConv != asCALL_GENERIC )
		return ConfigError(asNOT_SUPPORTED);

	asCObjectType *objType = GetObjectType(obj);
	if( objType == 0 )
		return ConfigError(asINVALID_OBJECT);

	isPrepared = false;

	// Put the system function in the list of system functions
	asSSystemFunctionInterface *newInterface = asNEW(asSSystemFunctionInterface)(internal);

	asCScriptFunction *func = asNEW(asCScriptFunction)(this, 0);
	func->funcType    = asFUNC_SYSTEM;
	func->sysFuncIntf = newInterface;
	func->objectType  = objType;

	objType->methods.PushLast((int)scriptFunctions.GetLength());

	asCBuilder bld(this, 0);
	r = bld.ParseFunctionDeclaration(declaration, func, true, &newInterface->paramAutoHandles, &newInterface->returnAutoHandle);
	if( r < 0 )
	{
		asDELETE(func,asCScriptFunction);
		return ConfigError(asINVALID_DECLARATION);
	}

	// Check name conflicts
	asCDataType dt;
	dt = asCDataType::CreateDefaultArray(this);
	r = bld.CheckNameConflictMember(dt, func->name.AddressOf(), 0, 0);
	if( r < 0 )
	{
		asDELETE(func,asCScriptFunction);
		return ConfigError(asNAME_TAKEN);
	}

	func->id = GetNextScriptFunctionId();
	SetScriptFunction(func);

	return 0;
}


int asCScriptEngine::RegisterObjectMethod(const char *obj, const char *declaration, const asSFuncPtr &funcPointer, asDWORD callConv)
{
	// Verify that the correct config group is set.
	if( currentGroup->FindType(obj) == 0 )
		return ConfigError(asWRONG_CONFIG_GROUP);

	asSSystemFunctionInterface internal;
	int r = DetectCallingConvention(obj != 0, funcPointer, callConv, &internal);
	if( r < 0 )
		return ConfigError(r);

	// We only support these calling conventions for object methods
#ifdef AS_MAX_PORTABILITY
	if( callConv != asCALL_GENERIC )
		return ConfigError(asNOT_SUPPORTED);
#else
	if( callConv != asCALL_THISCALL &&
		callConv != asCALL_CDECL_OBJLAST &&
		callConv != asCALL_CDECL_OBJFIRST &&
		callConv != asCALL_GENERIC )
		return ConfigError(asNOT_SUPPORTED);
#endif

	asCDataType dt;
	asCBuilder bld(this, 0);
	r = bld.ParseDataType(obj, &dt);
	if( r < 0 )
		return ConfigError(r);

	isPrepared = false;

	// Put the system function in the list of system functions
	asSSystemFunctionInterface *newInterface = asNEW(asSSystemFunctionInterface)(internal);

	asCScriptFunction *func = asNEW(asCScriptFunction)(this, 0);
	func->funcType    = asFUNC_SYSTEM;
	func->sysFuncIntf = newInterface;
	func->objectType  = dt.GetObjectType();

	r = bld.ParseFunctionDeclaration(declaration, func, true, &newInterface->paramAutoHandles, &newInterface->returnAutoHandle);
	if( r < 0 )
	{
		asDELETE(func,asCScriptFunction);
		return ConfigError(asINVALID_DECLARATION);
	}

	// Check name conflicts
	r = bld.CheckNameConflictMember(dt, func->name.AddressOf(), 0, 0);
	if( r < 0 )
	{
		asDELETE(func,asCScriptFunction);
		return ConfigError(asNAME_TAKEN);
	}

	func->id = GetNextScriptFunctionId();
	func->objectType->methods.PushLast(func->id);
	SetScriptFunction(func);

	// If parameter type from other groups are used, add references
	if( func->returnType.GetObjectType() )
	{
		asCConfigGroup *group = FindConfigGroup(func->returnType.GetObjectType());
		currentGroup->RefConfigGroup(group);
	}
	for( asUINT n = 0; n < func->parameterTypes.GetLength(); n++ )
	{
		if( func->parameterTypes[n].GetObjectType() )
		{
			asCConfigGroup *group = FindConfigGroup(func->parameterTypes[n].GetObjectType());
			currentGroup->RefConfigGroup(group);
		}
	}

	// Return the function id as success
	return func->id;
}

// interface
int asCScriptEngine::RegisterGlobalFunction(const char *declaration, const asSFuncPtr &funcPointer, asDWORD callConv)
{
	asSSystemFunctionInterface internal;
	int r = DetectCallingConvention(false, funcPointer, callConv, &internal);
	if( r < 0 )
		return ConfigError(r);

#ifdef AS_MAX_PORTABILITY
	if( callConv != asCALL_GENERIC )
		return ConfigError(asNOT_SUPPORTED);
#else
	if( callConv != asCALL_CDECL &&
		callConv != asCALL_STDCALL &&
		callConv != asCALL_GENERIC )
		return ConfigError(asNOT_SUPPORTED);
#endif

	isPrepared = false;

	// Put the system function in the list of system functions
	asSSystemFunctionInterface *newInterface = asNEW(asSSystemFunctionInterface)(internal);

	asCScriptFunction *func = asNEW(asCScriptFunction)(this, 0);
	func->funcType    = asFUNC_SYSTEM;
	func->sysFuncIntf = newInterface;

	asCBuilder bld(this, 0);
	r = bld.ParseFunctionDeclaration(declaration, func, true, &newInterface->paramAutoHandles, &newInterface->returnAutoHandle);
	if( r < 0 )
	{
		asDELETE(func,asCScriptFunction);
		return ConfigError(asINVALID_DECLARATION);
	}

	// Check name conflicts
	r = bld.CheckNameConflict(func->name.AddressOf(), 0, 0);
	if( r < 0 )
	{
		asDELETE(func,asCScriptFunction);
		return ConfigError(asNAME_TAKEN);
	}

	func->id = GetNextScriptFunctionId();
	SetScriptFunction(func);

	currentGroup->scriptFunctions.PushLast(func);
	registeredGlobalFuncs.PushLast(func);

	// If parameter type from other groups are used, add references
	if( func->returnType.GetObjectType() )
	{
		asCConfigGroup *group = FindConfigGroup(func->returnType.GetObjectType());
		currentGroup->RefConfigGroup(group);
	}
	for( asUINT n = 0; n < func->parameterTypes.GetLength(); n++ )
	{
		if( func->parameterTypes[n].GetObjectType() )
		{
			asCConfigGroup *group = FindConfigGroup(func->parameterTypes[n].GetObjectType());
			currentGroup->RefConfigGroup(group);
		}
	}

	// Return the function id as success
	return func->id;
}

// interface
int asCScriptEngine::GetGlobalFunctionCount()
{
	return (int)registeredGlobalFuncs.GetLength();
}

// interface
int asCScriptEngine::GetGlobalFunctionIdByIndex(asUINT index)
{
	if( index >= registeredGlobalFuncs.GetLength() )
		return asINVALID_ARG;

	return registeredGlobalFuncs[index]->id;
}





asCObjectType *asCScriptEngine::GetObjectType(const char *type)
{
	// TODO: optimize: Improve linear search
	for( asUINT n = 0; n < objectTypes.GetLength(); n++ )
		if( objectTypes[n] &&
			objectTypes[n]->name == type &&
			objectTypes[n]->arrayType == 0 )
			return objectTypes[n];

	return 0;
}


// This method will only return registered array types, since only they have the name stored
asCObjectType *asCScriptEngine::GetArrayType(const char *type)
{
	if( type[0] == 0 )
		return 0;

	// TODO: optimize: Improve linear search
	for( asUINT n = 0; n < arrayTypes.GetLength(); n++ )
		if( arrayTypes[n] &&
			arrayTypes[n]->name == type )
			return arrayTypes[n];

	return 0;
}



void asCScriptEngine::PrepareEngine()
{
	if( isPrepared ) return;
	if( configFailed ) return;

	asUINT n;
	for( n = 0; n < scriptFunctions.GetLength(); n++ )
	{
		// Determine the host application interface
		if( scriptFunctions[n] && scriptFunctions[n]->funcType == asFUNC_SYSTEM )
		{
			if( scriptFunctions[n]->sysFuncIntf->callConv == ICC_GENERIC_FUNC ||
				scriptFunctions[n]->sysFuncIntf->callConv == ICC_GENERIC_METHOD )
				PrepareSystemFunctionGeneric(scriptFunctions[n], scriptFunctions[n]->sysFuncIntf, this);
			else
				PrepareSystemFunction(scriptFunctions[n], scriptFunctions[n]->sysFuncIntf, this);
		}
	}

	// Validate object type registrations
	for( n = 0; n < objectTypes.GetLength(); n++ )
	{
		if( objectTypes[n] && !(objectTypes[n]->flags & (asOBJ_SCRIPT_OBJECT | asOBJ_TEMPLATE)) )
		{
			bool missingBehaviour = false;

			// Verify that GC types have all behaviours
			if( objectTypes[n]->flags & asOBJ_GC )
			{
				if( objectTypes[n]->beh.addref                 == 0 ||
					objectTypes[n]->beh.release                == 0 ||
					objectTypes[n]->beh.gcGetRefCount          == 0 ||
					objectTypes[n]->beh.gcSetFlag              == 0 ||
					objectTypes[n]->beh.gcGetFlag              == 0 ||
					objectTypes[n]->beh.gcEnumReferences       == 0 ||
					objectTypes[n]->beh.gcReleaseAllReferences == 0 )
				{
					missingBehaviour = true;
				}
			}
			// Verify that scoped ref types have the release behaviour
			else if( objectTypes[n]->flags & asOBJ_SCOPED )
			{
				if( objectTypes[n]->beh.release == 0 )
				{
					missingBehaviour = true;
				}
			}
			// Verify that ref types have add ref and release behaviours
			else if( (objectTypes[n]->flags & asOBJ_REF) &&
				     !(objectTypes[n]->flags & asOBJ_NOHANDLE) )
			{
				if( objectTypes[n]->beh.addref  == 0 ||
					objectTypes[n]->beh.release == 0 )
				{
					missingBehaviour = true;
				}
			}
			// Verify that non-pod value types have the constructor and destructor registered
			else if( (objectTypes[n]->flags & asOBJ_VALUE) &&
				     !(objectTypes[n]->flags & asOBJ_POD) )
			{
				if( objectTypes[n]->beh.construct == 0 ||
					objectTypes[n]->beh.destruct  == 0 )
				{
					missingBehaviour = true;
				}
			}

			if( missingBehaviour )
			{
				asCString str;
				str.Format(TXT_TYPE_s_IS_MISSING_BEHAVIOURS, objectTypes[n]->name.AddressOf());
				WriteMessage("", 0, 0, asMSGTYPE_ERROR, str.AddressOf());
				ConfigError(asINVALID_CONFIGURATION);
			}
		}
	}

	isPrepared = true;
}

int asCScriptEngine::ConfigError(int err)
{
	configFailed = true;
	return err;
}


// interface
int asCScriptEngine::RegisterStringFactory(const char *datatype, const asSFuncPtr &funcPointer, asDWORD callConv)
{
	asSSystemFunctionInterface internal;
	int r = DetectCallingConvention(false, funcPointer, callConv, &internal);
	if( r < 0 )
		return ConfigError(r);

#ifdef AS_MAX_PORTABILITY
	if( callConv != asCALL_GENERIC )
		return ConfigError(asNOT_SUPPORTED);
#else
	if( callConv != asCALL_CDECL &&
		callConv != asCALL_STDCALL &&
		callConv != asCALL_GENERIC )
		return ConfigError(asNOT_SUPPORTED);
#endif

	// Put the system function in the list of system functions
	asSSystemFunctionInterface *newInterface = asNEW(asSSystemFunctionInterface)(internal);

	asCScriptFunction *func = asNEW(asCScriptFunction)(this, 0);
	func->funcType    = asFUNC_SYSTEM;
	func->sysFuncIntf = newInterface;

	asCBuilder bld(this, 0);

	asCDataType dt;
	r = bld.ParseDataType(datatype, &dt);
	if( r < 0 )
	{
		asDELETE(func,asCScriptFunction);
		return ConfigError(asINVALID_TYPE);
	}

	func->returnType = dt;
	func->parameterTypes.PushLast(asCDataType::CreatePrimitive(ttInt, true));
	asCDataType parm1 = asCDataType::CreatePrimitive(ttUInt8, true);
	parm1.MakeReference(true);
	func->parameterTypes.PushLast(parm1);
	func->id = GetNextScriptFunctionId();
	SetScriptFunction(func);

	stringFactory = func;

	if( func->returnType.GetObjectType() )
	{
		asCConfigGroup *group = FindConfigGroup(func->returnType.GetObjectType());
		if( group == 0 ) group = &defaultGroup;
		group->scriptFunctions.PushLast(func);
	}

	// Register function id as success
	return func->id;
}

// interface
int asCScriptEngine::GetStringFactoryReturnTypeId()
{
	if( stringFactory == 0 )
		return asNO_FUNCTION;

	return GetTypeIdFromDataType(stringFactory->returnType);
}

// interface
asCModule *asCScriptEngine::GetModule(const char *_name, bool create)
{
	// Accept null as well as zero-length string
	const char *name = "";
	if( _name != 0 ) name = _name;

	if( lastModule && lastModule->name == name )
	{
		if( !lastModule->isDiscarded )
			return lastModule;

		lastModule = 0;
	}

	// TODO: optimize: Improve linear search
	for( asUINT n = 0; n < scriptModules.GetLength(); ++n )
		if( scriptModules[n] && scriptModules[n]->name == name )
		{
			if( !scriptModules[n]->isDiscarded )
			{
				lastModule = scriptModules[n];
				return lastModule;
			}
		}

	if( create )
	{
		// TODO: Store a list of free indices
		// Should find a free spot, not just the last one
		asUINT idx;
		for( idx = 0; idx < scriptModules.GetLength(); ++idx )
			if( scriptModules[idx] == 0 )
				break;

		int moduleID = idx << 16;
		asASSERT(moduleID <= 0x3FF0000);

		asCModule *module = asNEW(asCModule)(name, moduleID, this);

		if( idx == scriptModules.GetLength() )
			scriptModules.PushLast(module);
		else
			scriptModules[idx] = module;

		lastModule = module;

		return lastModule;
	}

	return 0;
}

asCModule *asCScriptEngine::GetModule(int id)
{
	// TODO: This may not work any longer
	id = (id >> 16) & 0x3FF;
	if( id >= (int)scriptModules.GetLength() ) return 0;
	return scriptModules[id];
}

asCModule *asCScriptEngine::GetModuleFromFuncId(int id)
{
	if( id < 0 ) return 0;
	id &= 0xFFFF;
	if( id >= (int)scriptFunctions.GetLength() ) return 0;
	asCScriptFunction *func = scriptFunctions[id];
	if( func == 0 ) return 0;
	return func->module;
}

// internal
int asCScriptEngine::RequestBuild()
{
	ENTERCRITICALSECTION(engineCritical);
	if( isBuilding )
	{
		LEAVECRITICALSECTION(engineCritical);
		return asBUILD_IN_PROGRESS;
	}
	isBuilding = true;
	LEAVECRITICALSECTION(engineCritical);

	return 0;
}

// internal
void asCScriptEngine::BuildCompleted()
{
	// Always free up pooled memory after a completed build
	memoryMgr.FreeUnusedMemory();

	isBuilding = false;
}

// interface
int asCScriptEngine::ExecuteString(const char *module, const char *script, asIScriptContext **ctx, asDWORD flags)
{
	ENTERCRITICALSECTION(engineCritical);
	if( isBuilding )
	{
		LEAVECRITICALSECTION(engineCritical);
		return asBUILD_IN_PROGRESS;
	}
	isBuilding = true;
	LEAVECRITICALSECTION(engineCritical);

	PrepareEngine();

	// Make sure the config worked
	if( configFailed )
	{
		if( ctx && !(flags & asEXECSTRING_USE_MY_CONTEXT) )
			*ctx = 0;

		WriteMessage("",0,0,asMSGTYPE_ERROR,TXT_INVALID_CONFIGURATION);
		isBuilding = false;
		return asINVALID_CONFIGURATION;
	}

	asIScriptContext *exec = 0;
	if( !(flags & asEXECSTRING_USE_MY_CONTEXT) )
	{
		int r = CreateContext(&exec, false);
		if( r < 0 )
		{
			if( ctx && !(flags & asEXECSTRING_USE_MY_CONTEXT) )
				*ctx = 0;
			isBuilding = false;
			return r;
		}
		if( ctx )
		{
			*ctx = exec;
			exec->AddRef();
		}
	}
	else
	{
		if( *ctx == 0 )
		{
			isBuilding = false;
			return asINVALID_ARG;
		}
		exec = *ctx;
		exec->AddRef();
	}

	// Get the module to compile the string in
	asCModule *mod = GetModule(module, true);

	// Compile string function
	asCBuilder builder(this, mod);
	asCString str = script;
	str = "void ExecuteString(){\n" + str + "\n;}";

	int r = builder.BuildString(str.AddressOf(), (asCContext*)exec);
	memoryMgr.FreeUnusedMemory();
	if( r < 0 )
	{
		if( ctx && !(flags & asEXECSTRING_USE_MY_CONTEXT) )
		{
			(*ctx)->Release();
			*ctx = 0;
		}
		exec->Release();
		isBuilding = false;
		return asERROR;
	}

	// The build has ended
	isBuilding = false;

	// Prepare and execute the context
	r = ((asCContext*)exec)->PrepareSpecial(asFUNC_STRING, mod);
	if( r < 0 )
	{
		if( ctx && !(flags & asEXECSTRING_USE_MY_CONTEXT) )
		{
			(*ctx)->Release();
			*ctx = 0;
		}
		exec->Release();
		return r;
	}

	if( flags & asEXECSTRING_ONLY_PREPARE )
		r = asEXECUTION_PREPARED;
	else
		r = exec->Execute();

	exec->Release();
	return r;
}

void asCScriptEngine::RemoveArrayType(asCObjectType *t)
{
	int n;

	// Destroy the factory stubs
	for( n = 0; n < (int)t->beh.factories.GetLength(); n++ )
	{
		DeleteScriptFunction(t->beh.factories[n]);
	}

	// Start searching from the end of the list, as most of
	// the time it will be the last two types
	for( n = (int)arrayTypes.GetLength()-1; n >= 0; n-- )
	{
		if( arrayTypes[n] == t )
		{
			if( n == (signed)arrayTypes.GetLength()-1 )
				arrayTypes.PopLast();
			else
				arrayTypes[n] = arrayTypes.PopLast();
		}
	}

	for( n = (int)scriptArrayTypes.GetLength()-1; n >= 0; n-- )
	{
		if( scriptArrayTypes[n] == t )
		{
			if( n == (signed)scriptArrayTypes.GetLength()-1 )
				scriptArrayTypes.PopLast();
			else
				scriptArrayTypes[n] = scriptArrayTypes.PopLast();
		}
	}

	asDELETE(t,asCObjectType);
}

asCObjectType *asCScriptEngine::GetArrayTypeFromSubType(asCDataType &type)
{
	int arrayType = type.GetArrayType();
	if( type.IsObjectHandle() )
		arrayType = (arrayType<<2) | 3;
	else
		arrayType = (arrayType<<2) | 2;

	// Is there any array type already with this subtype?
	if( type.IsObject() )
	{
		// TODO: optimize: Improve linear search
		for( asUINT n = 0; n < arrayTypes.GetLength(); n++ )
		{
			if( arrayTypes[n] &&
				arrayTypes[n]->tokenType == ttIdentifier &&
				arrayTypes[n]->subType == type.GetObjectType() &&
				arrayTypes[n]->arrayType == arrayType )
				return arrayTypes[n];
		}
	}
	else
	{
		// TODO: optimize: Improve linear search
		for( asUINT n = 0; n < arrayTypes.GetLength(); n++ )
		{
			if( arrayTypes[n] &&
				arrayTypes[n]->tokenType == type.GetTokenType() &&
				arrayTypes[n]->arrayType == arrayType )
				return arrayTypes[n];
		}
	}

	// No previous array type has been registered

	// Create a new array type based on the defaultArrayObjectType
	asCObjectType *ot = asNEW(asCObjectType)(this);
	ot->arrayType = arrayType;
	ot->flags = asOBJ_REF | asOBJ_TEMPLATE;
	ot->size = sizeof(asCArrayObject);
	ot->name = ""; // Built-in script arrays are registered without name
	ot->tokenType = type.GetTokenType();
	ot->methods = defaultArrayObjectType->methods;
	// Store the real factory in the constructor
	ot->beh.construct = defaultArrayObjectType->beh.factory;
	ot->beh.constructors = defaultArrayObjectType->beh.factories;
	// Generate factory stubs for each of the factories
	for( asUINT n = 0; n < defaultArrayObjectType->beh.factories.GetLength(); n++ )
	{
		int factoryId = defaultArrayObjectType->beh.factories[n];
		asCScriptFunction *factory = scriptFunctions[factoryId];

		asCScriptFunction *func = asNEW(asCScriptFunction)(this, 0);
		func->funcType         = asFUNC_SCRIPT;
		func->name             = "factstub";
		func->id               = GetNextScriptFunctionId();
		func->returnType       = asCDataType::CreateObjectHandle(ot, false);

		// Skip the first parameter as this is the object type pointer that the stub will add
		for( asUINT p = 1; p < factory->parameterTypes.GetLength(); p++ )
		{
			func->parameterTypes.PushLast(factory->parameterTypes[p]);
			func->inOutFlags.PushLast(factory->inOutFlags[p]);
		}

		SetScriptFunction(func);

		asCBuilder builder(this, 0);
		asCCompiler compiler(this);
		compiler.CompileTemplateFactoryStub(&builder, factoryId, ot, func);

		ot->beh.factories.PushLast(func->id);
	}
	if( ot->beh.factories.GetLength() )
		ot->beh.factory = ot->beh.factories[0];
	else
		ot->beh.factory = defaultArrayObjectType->beh.factory;



	ot->beh.addref                 = defaultArrayObjectType->beh.addref;
	ot->beh.release                = defaultArrayObjectType->beh.release;
	ot->beh.copy                   = defaultArrayObjectType->beh.copy;
	ot->beh.operators              = defaultArrayObjectType->beh.operators;
	ot->beh.gcGetRefCount          = defaultArrayObjectType->beh.gcGetRefCount;
	ot->beh.gcSetFlag              = defaultArrayObjectType->beh.gcSetFlag;
	ot->beh.gcGetFlag              = defaultArrayObjectType->beh.gcGetFlag;
	ot->beh.gcEnumReferences       = defaultArrayObjectType->beh.gcEnumReferences;
	ot->beh.gcReleaseAllReferences = defaultArrayObjectType->beh.gcReleaseAllReferences;

	// TODO: Template: As the new array type is instanciated, the engine should
	//       generate new functions to substitute the ones with the template subtype.

	// The object type needs to store the sub type as well
	ot->subType = type.GetObjectType();
	if( ot->subType ) ot->subType->AddRef();

	// TODO: The indexing behaviour and assignment
	//       behaviour should use the correct datatype

	// Verify if the subtype contains an any object, in which case this array is a potential circular reference
	// TODO: We may be a bit smarter here. If we can guarantee that the array type cannot be part of the
	//       potential circular reference then we don't need to set the flag
	if( ot->subType && (ot->subType->flags & asOBJ_GC) )
		ot->flags |= asOBJ_GC;

	arrayTypes.PushLast(ot);

	// We need to store the object type somewhere for clean-up later
	scriptArrayTypes.PushLast(ot);

	return ot;
}

void asCScriptEngine::CallObjectMethod(void *obj, int func)
{
	asCScriptFunction *s = scriptFunctions[func];
	CallObjectMethod(obj, s->sysFuncIntf, s);
}

void asCScriptEngine::CallObjectMethod(void *obj, asSSystemFunctionInterface *i, asCScriptFunction *s)
{
#ifdef __GNUC__
	if( i->callConv == ICC_GENERIC_METHOD )
	{
		asCGeneric gen(this, s, obj, 0);
		void (*f)(asIScriptGeneric *) = (void (*)(asIScriptGeneric *))(i->func);
		f(&gen);
	}
	else if( i->callConv == ICC_VIRTUAL_THISCALL )
	{
		// For virtual thiscalls we must call the method as a true class method
		// so that the compiler will lookup the function address in the vftable
		union
		{
			asSIMPLEMETHOD_t mthd;
			struct
			{
				asFUNCTION_t func;
				asPWORD baseOffset;  // Same size as the pointer
			} f;
		} p;
		p.f.func = (void (*)())(i->func);
		p.f.baseOffset = asPWORD(i->baseOffset);
		void (asCSimpleDummy::*f)() = p.mthd;
		(((asCSimpleDummy*)obj)->*f)();
	}
	else /*if( i->callConv == ICC_THISCALL || i->callConv == ICC_CDECL_OBJLAST || i->callConv == ICC_CDECL_OBJFIRST )*/
	{
		// Non-virtual thiscall can be called just like any global function, passing the object as the first parameter
		void (*f)(void *) = (void (*)(void *))(i->func);
		f(obj);
	}
#else
#ifndef AS_NO_CLASS_METHODS
	if( i->callConv == ICC_THISCALL )
	{
		union
		{
			asSIMPLEMETHOD_t mthd;
			asFUNCTION_t func;
		} p;
		p.func = (void (*)())(i->func);
		void (asCSimpleDummy::*f)() = p.mthd;
		(((asCSimpleDummy*)obj)->*f)();
	}
	else
#endif
	if( i->callConv == ICC_GENERIC_METHOD )
	{
		asCGeneric gen(this, s, obj, 0);
		void (*f)(asIScriptGeneric *) = (void (*)(asIScriptGeneric *))(i->func);
		f(&gen);
	}
	else /*if( i->callConv == ICC_CDECL_OBJLAST || i->callConv == ICC_CDECL_OBJFIRST )*/
	{
		void (*f)(void *) = (void (*)(void *))(i->func);
		f(obj);
	}
#endif
}

bool asCScriptEngine::CallObjectMethodRetBool(void *obj, int func)
{
	asCScriptFunction *s = scriptFunctions[func];
	asSSystemFunctionInterface *i = s->sysFuncIntf;

#ifdef __GNUC__
	if( i->callConv == ICC_GENERIC_METHOD )
	{
		asCGeneric gen(this, s, obj, 0);
		void (*f)(asIScriptGeneric *) = (void (*)(asIScriptGeneric *))(i->func);
		f(&gen);
		return *(bool*)gen.GetReturnPointer();
	}
	else if( i->callConv == ICC_VIRTUAL_THISCALL )
	{
		// For virtual thiscalls we must call the method as a true class method so that the compiler will lookup the function address in the vftable
		union
		{
			asSIMPLEMETHOD_t mthd;
			struct
			{
				asFUNCTION_t func;
				asDWORD baseOffset;
			} f;
		} p;
		p.f.func = (void (*)())(i->func);
		p.f.baseOffset = i->baseOffset;
		bool (asCSimpleDummy::*f)() = (bool (asCSimpleDummy::*)())(p.mthd);
		return (((asCSimpleDummy*)obj)->*f)();
	}
	else /*if( i->callConv == ICC_THISCALL || i->callConv == ICC_CDECL_OBJLAST || i->callConv == ICC_CDECL_OBJFIRST )*/
	{
		// Non-virtual thiscall can be called just like any global function, passing the object as the first parameter
		bool (*f)(void *) = (bool (*)(void *))(i->func);
		return f(obj);
	}
#else
#ifndef AS_NO_CLASS_METHODS
	if( i->callConv == ICC_THISCALL )
	{
		union
		{
			asSIMPLEMETHOD_t mthd;
			asFUNCTION_t func;
		} p;
		p.func = (void (*)())(i->func);
		bool (asCSimpleDummy::*f)() = (bool (asCSimpleDummy::*)())p.mthd;
		return (((asCSimpleDummy*)obj)->*f)();
	}
	else
#endif
	if( i->callConv == ICC_GENERIC_METHOD )
	{
		asCGeneric gen(this, s, obj, 0);
		void (*f)(asIScriptGeneric *) = (void (*)(asIScriptGeneric *))(i->func);
		f(&gen);
		return *(bool*)gen.GetReturnPointer();
	}
	else /*if( i->callConv == ICC_CDECL_OBJLAST || i->callConv == ICC_CDECL_OBJFIRST )*/
	{
		bool (*f)(void *) = (bool (*)(void *))(i->func);
		return f(obj);
	}
#endif
}

int asCScriptEngine::CallObjectMethodRetInt(void *obj, int func)
{
	asCScriptFunction *s = scriptFunctions[func];
	asSSystemFunctionInterface *i = s->sysFuncIntf;

#ifdef __GNUC__
	if( i->callConv == ICC_GENERIC_METHOD )
	{
		asCGeneric gen(this, s, obj, 0);
		void (*f)(asIScriptGeneric *) = (void (*)(asIScriptGeneric *))(i->func);
		f(&gen);
		return *(int*)gen.GetReturnPointer();
	}
	else if( i->callConv == ICC_VIRTUAL_THISCALL )
	{
		// For virtual thiscalls we must call the method as a true class method so that the compiler will lookup the function address in the vftable
		union
		{
			asSIMPLEMETHOD_t mthd;
			struct
			{
				asFUNCTION_t func;
				asDWORD baseOffset;
			} f;
		} p;
		p.f.func = (void (*)())(i->func);
		p.f.baseOffset = i->baseOffset;
		int (asCSimpleDummy::*f)() = (int (asCSimpleDummy::*)())(p.mthd);
		return (((asCSimpleDummy*)obj)->*f)();
	}
	else /*if( i->callConv == ICC_THISCALL || i->callConv == ICC_CDECL_OBJLAST || i->callConv == ICC_CDECL_OBJFIRST )*/
	{
		// Non-virtual thiscall can be called just like any global function, passing the object as the first parameter
		int (*f)(void *) = (int (*)(void *))(i->func);
		return f(obj);
	}
#else
#ifndef AS_NO_CLASS_METHODS
	if( i->callConv == ICC_THISCALL )
	{
		union
		{
			asSIMPLEMETHOD_t mthd;
			asFUNCTION_t func;
		} p;
		p.func = (void (*)())(i->func);
		int (asCSimpleDummy::*f)() = (int (asCSimpleDummy::*)())p.mthd;
		return (((asCSimpleDummy*)obj)->*f)();
	}
	else
#endif
	if( i->callConv == ICC_GENERIC_METHOD )
	{
		asCGeneric gen(this, s, obj, 0);
		void (*f)(asIScriptGeneric *) = (void (*)(asIScriptGeneric *))(i->func);
		f(&gen);
		return *(int*)gen.GetReturnPointer();
	}
	else /*if( i->callConv == ICC_CDECL_OBJLAST || i->callConv == ICC_CDECL_OBJFIRST )*/
	{
		int (*f)(void *) = (int (*)(void *))(i->func);
		return f(obj);
	}
#endif
}

void *asCScriptEngine::CallGlobalFunctionRetPtr(int func)
{
	asCScriptFunction *s = scriptFunctions[func];
	return CallGlobalFunctionRetPtr(s->sysFuncIntf, s);
}

void *asCScriptEngine::CallGlobalFunctionRetPtr(asSSystemFunctionInterface *i, asCScriptFunction *s)
{
	if( i->callConv == ICC_CDECL )
	{
		void *(*f)() = (void *(*)())(i->func);
		return f();
	}
	else if( i->callConv == ICC_STDCALL )
	{
		void *(STDCALL *f)() = (void *(STDCALL *)())(i->func);
		return f();
	}
	else
	{
		asCGeneric gen(this, s, 0, 0);
		void (*f)(asIScriptGeneric *) = (void (*)(asIScriptGeneric *))(i->func);
		f(&gen);
		return *(void**)gen.GetReturnPointer();
	}
}

void asCScriptEngine::CallObjectMethod(void *obj, void *param, int func)
{
	asCScriptFunction *s = scriptFunctions[func];
	CallObjectMethod(obj, param, s->sysFuncIntf, s);
}

void asCScriptEngine::CallObjectMethod(void *obj, void *param, asSSystemFunctionInterface *i, asCScriptFunction *s)
{
#ifdef __GNUC__
	if( i->callConv == ICC_CDECL_OBJLAST )
	{
		void (*f)(void *, void *) = (void (*)(void *, void *))(i->func);
		f(param, obj);
	}
	else if( i->callConv == ICC_GENERIC_METHOD )
	{
		asCGeneric gen(this, s, obj, (asDWORD*)&param);
		void (*f)(asIScriptGeneric *) = (void (*)(asIScriptGeneric *))(i->func);
		f(&gen);
	}
	else /*if( i->callConv == ICC_CDECL_OBJFIRST || i->callConv == ICC_THISCALL )*/
	{
		void (*f)(void *, void *) = (void (*)(void *, void *))(i->func);
		f(obj, param);
	}
#else
#ifndef AS_NO_CLASS_METHODS
	if( i->callConv == ICC_THISCALL )
	{
		union
		{
			asSIMPLEMETHOD_t mthd;
			asFUNCTION_t func;
		} p;
		p.func = (void (*)())(i->func);
		void (asCSimpleDummy::*f)(void *) = (void (asCSimpleDummy::*)(void *))(p.mthd);
		(((asCSimpleDummy*)obj)->*f)(param);
	}
	else
#endif
	if( i->callConv == ICC_CDECL_OBJLAST )
	{
		void (*f)(void *, void *) = (void (*)(void *, void *))(i->func);
		f(param, obj);
	}
	else if( i->callConv == ICC_GENERIC_METHOD )
	{
		asCGeneric gen(this, s, obj, (asDWORD*)&param);
		void (*f)(asIScriptGeneric *) = (void (*)(asIScriptGeneric *))(i->func);
		f(&gen);
	}
	else /*if( i->callConv == ICC_CDECL_OBJFIRST )*/
	{
		void (*f)(void *, void *) = (void (*)(void *, void *))(i->func);
		f(obj, param);
	}
#endif
}

void asCScriptEngine::CallGlobalFunction(void *param1, void *param2, asSSystemFunctionInterface *i, asCScriptFunction *s)
{
	if( i->callConv == ICC_CDECL )
	{
		void (*f)(void *, void *) = (void (*)(void *, void *))(i->func);
		f(param1, param2);
	}
	else if( i->callConv == ICC_STDCALL )
	{
		void (STDCALL *f)(void *, void *) = (void (STDCALL *)(void *, void *))(i->func);
		f(param1, param2);
	}
	else
	{
		asCGeneric gen(this, s, 0, (asDWORD*)&param1);
		void (*f)(asIScriptGeneric *) = (void (*)(asIScriptGeneric *))(i->func);
		f(&gen);
	}
}

bool asCScriptEngine::CallGlobalFunctionRetBool(void *param1, void *param2, asSSystemFunctionInterface *i, asCScriptFunction *s)
{
	if( i->callConv == ICC_CDECL )
	{
		bool (*f)(void *, void *) = (bool (*)(void *, void *))(i->func);
		return f(param1, param2);
	}
	else if( i->callConv == ICC_STDCALL )
	{
		bool (STDCALL *f)(void *, void *) = (bool (STDCALL *)(void *, void *))(i->func);
		return f(param1, param2);
	}
	else
	{
		// TODO: When simulating a 64bit environment by defining AS_64BIT_PTR on a 32bit platform this code
		//       fails, because the stack given to asCGeneric is not prepared with two 64bit arguments.
		asCGeneric gen(this, s, 0, (asDWORD*)&param1);
		void (*f)(asIScriptGeneric *) = (void (*)(asIScriptGeneric *))(i->func);
		f(&gen);
		return *(bool*)gen.GetReturnPointer();
	}
}

void *asCScriptEngine::CallAlloc(asCObjectType *type)
{
    // Allocate 4 bytes as the smallest size. Otherwise CallSystemFunction may try to
    // copy a DWORD onto a smaller memory block, in case the object type is return in registers.
#if defined(AS_DEBUG) 
	return ((asALLOCFUNCDEBUG_t)(userAlloc))(type->size < 4 ? 4 : type->size, __FILE__, __LINE__);
#else
	return userAlloc(type->size < 4 ? 4 : type->size);
#endif
}

void asCScriptEngine::CallFree(void *obj)
{
	userFree(obj);
}

// interface
void asCScriptEngine::NotifyGarbageCollectorOfNewObject(void *obj, int typeId)
{
	asCObjectType *objType = GetObjectTypeFromTypeId(typeId);
	gc.AddScriptObjectToGC(obj, objType);
}

// interface
int asCScriptEngine::GarbageCollect(asEGCFlags flags)
{
	return gc.GarbageCollect(flags);
}

// interface
void asCScriptEngine::GetGCStatistics(asUINT *currentSize, asUINT *totalDestroyed, asUINT *totalDetected)
{
	gc.GetStatistics(currentSize, totalDestroyed, totalDetected);
}

// interface
void asCScriptEngine::GCEnumCallback(void *reference)
{
	gc.GCEnumCallback(reference);
}


// TODO: multithread: The mapTypeIdToDataType must be protected with critical sections in all functions that access it
int asCScriptEngine::GetTypeIdFromDataType(const asCDataType &dt)
{
	// Find the existing type id
	asSMapNode<int,asCDataType*> *cursor = 0;
	mapTypeIdToDataType.MoveFirst(&cursor);
	while( cursor )
	{
		if( mapTypeIdToDataType.GetValue(cursor)->IsEqualExceptRefAndConst(dt) )
			return mapTypeIdToDataType.GetKey(cursor);

		mapTypeIdToDataType.MoveNext(&cursor, cursor);
	}

	// The type id doesn't exist, create it

	// Setup the basic type id
	int typeId = typeIdSeqNbr++;
	if( dt.GetObjectType() )
	{
		if( dt.GetObjectType()->flags & asOBJ_SCRIPT_OBJECT ) typeId |= asTYPEID_SCRIPTOBJECT;
		else if( dt.GetObjectType()->flags & asOBJ_TEMPLATE ) typeId |= asTYPEID_SCRIPTARRAY;
		else if( dt.GetObjectType()->flags & asOBJ_ENUM ); // TODO: Should we have a specific bit for this?
		else typeId |= asTYPEID_APPOBJECT;
	}

	// Insert the basic object type
	asCDataType *newDt = asNEW(asCDataType)(dt);
	newDt->MakeReference(false);
	newDt->MakeReadOnly(false);
	newDt->MakeHandle(false);

	mapTypeIdToDataType.Insert(typeId, newDt);

	// If the object type supports object handles then register those types as well
	// Note: Don't check for addref, as asOBJ_SCOPED don't have this
	if( dt.IsObject() && dt.GetObjectType()->beh.release ) 
	{
		newDt = asNEW(asCDataType)(dt);
		newDt->MakeReference(false);
		newDt->MakeReadOnly(false);
		newDt->MakeHandle(true);
		newDt->MakeHandleToConst(false);

		mapTypeIdToDataType.Insert(typeId | asTYPEID_OBJHANDLE, newDt);

		newDt = asNEW(asCDataType)(dt);
		newDt->MakeReference(false);
		newDt->MakeReadOnly(false);
		newDt->MakeHandle(true);
		newDt->MakeHandleToConst(true);

		mapTypeIdToDataType.Insert(typeId | asTYPEID_OBJHANDLE | asTYPEID_HANDLETOCONST, newDt);
	}

	// Call the method recursively to get the correct type id
	return GetTypeIdFromDataType(dt);
}

const asCDataType *asCScriptEngine::GetDataTypeFromTypeId(int typeId)
{
	asSMapNode<int,asCDataType*> *cursor = 0;
	if( mapTypeIdToDataType.MoveTo(&cursor, typeId) )
		return mapTypeIdToDataType.GetValue(cursor);

	return 0;
}

asCObjectType *asCScriptEngine::GetObjectTypeFromTypeId(int typeId)
{
	asSMapNode<int,asCDataType*> *cursor = 0;
	if( mapTypeIdToDataType.MoveTo(&cursor, typeId) )
		return mapTypeIdToDataType.GetValue(cursor)->GetObjectType();

	return 0;
}

void asCScriptEngine::RemoveFromTypeIdMap(asCObjectType *type)
{
	asSMapNode<int,asCDataType*> *cursor = 0;
	mapTypeIdToDataType.MoveFirst(&cursor);
	while( cursor )
	{
		asCDataType *dt = mapTypeIdToDataType.GetValue(cursor);
		asSMapNode<int,asCDataType*> *old = cursor;
		mapTypeIdToDataType.MoveNext(&cursor, cursor);
		if( dt->GetObjectType() == type )
		{
			asDELETE(dt,asCDataType);
			mapTypeIdToDataType.Erase(old);
		}
	}
}

// interface
int asCScriptEngine::GetTypeIdByDecl(const char *decl)
{
	asCDataType dt;
	asCBuilder bld(this, 0);
	int r = bld.ParseDataType(decl, &dt);
	if( r < 0 )
		return asINVALID_TYPE;

	return GetTypeIdFromDataType(dt);
}



const char *asCScriptEngine::GetTypeDeclaration(int typeId)
{
	const asCDataType *dt = GetDataTypeFromTypeId(typeId);
	if( dt == 0 ) return 0;

	asASSERT(threadManager);
	asCString *tempString = &threadManager->GetLocalData()->string;
	*tempString = dt->Format();

	return tempString->AddressOf();
}

int asCScriptEngine::GetSizeOfPrimitiveType(int typeId)
{
	const asCDataType *dt = GetDataTypeFromTypeId(typeId);
	if( dt == 0 ) return 0;
	if( !dt->IsPrimitive() ) return 0;

	return dt->GetSizeInMemoryBytes();
}

void *asCScriptEngine::CreateScriptObject(int typeId)
{
	// Make sure the type id is for an object type, and not a primitive or a handle
	if( (typeId & (asTYPEID_MASK_OBJECT | asTYPEID_MASK_SEQNBR)) != typeId ) return 0;
	if( (typeId & asTYPEID_MASK_OBJECT) == 0 ) return 0;

	const asCDataType *dt = GetDataTypeFromTypeId(typeId);

	// Is the type id valid?
	if( !dt ) return 0;

	// Allocate the memory
	asCObjectType *objType = dt->GetObjectType();
	void *ptr = 0;

	// Construct the object
	if( objType->flags & asOBJ_SCRIPT_OBJECT )
		ptr = ScriptObjectFactory(objType, this);
	else if( objType->flags & asOBJ_TEMPLATE )
		ptr = ArrayObjectFactory(objType);
	else if( objType->flags & asOBJ_REF )
		ptr = CallGlobalFunctionRetPtr(objType->beh.factory);
	else
	{
		ptr = CallAlloc(objType);
		int funcIndex = objType->beh.construct;
		if( funcIndex )
			CallObjectMethod(ptr, funcIndex);
	}

	return ptr;
}

void *asCScriptEngine::CreateScriptObjectCopy(void *origObj, int typeId)
{
	void *newObj = CreateScriptObject(typeId);
	if( newObj == 0 ) return 0;

	CopyScriptObject(newObj, origObj, typeId);

	return newObj;
}

void asCScriptEngine::CopyScriptObject(void *dstObj, void *srcObj, int typeId)
{
	// Make sure the type id is for an object type, and not a primitive or a handle
	if( (typeId & (asTYPEID_MASK_OBJECT | asTYPEID_MASK_SEQNBR)) != typeId ) return;
	if( (typeId & asTYPEID_MASK_OBJECT) == 0 ) return;

	// Copy the contents from the original object, using the assignment operator
	const asCDataType *dt = GetDataTypeFromTypeId(typeId);

	// Is the type id valid?
	if( !dt ) return;

	asCObjectType *objType = dt->GetObjectType();
	if( objType->beh.copy )
	{
		CallObjectMethod(dstObj, srcObj, objType->beh.copy);
	}
	else if( objType->size )
	{
		memcpy(dstObj, srcObj, objType->size);
	}
}

int asCScriptEngine::CompareScriptObjects(bool &result, int behaviour, void *leftObj, void *rightObj, int typeId)
{
	// Make sure the type id is for an object type, and not a primitive or a handle
	if( (typeId & (asTYPEID_MASK_OBJECT | asTYPEID_MASK_SEQNBR)) != typeId ) return asINVALID_TYPE;
	if( (typeId & asTYPEID_MASK_OBJECT) == 0 ) return asINVALID_TYPE;

	// Is the behaviour valid?
	if( behaviour < (int)asBEHAVE_EQUAL || behaviour > (int)asBEHAVE_GEQUAL ) return asINVALID_ARG;

	// Get the object type information so we can find the comparison behaviours
	asCObjectType *ot = GetObjectTypeFromTypeId(typeId);
	if( !ot ) return asINVALID_TYPE;
	asCDataType dt = asCDataType::CreateObject(ot, true);
	dt.MakeReference(true);

	// Find the behaviour
	unsigned int n;
	for( n = 0; n < globalBehaviours.operators.GetLength(); n += 2 )
	{
		// Is it the right comparison operator?
		if( globalBehaviours.operators[n] == behaviour )
		{
			// Is it the right datatype?
			int func = globalBehaviours.operators[n+1];
			if( scriptFunctions[func]->parameterTypes[0].IsEqualExceptConst(dt) &&
				scriptFunctions[func]->parameterTypes[1].IsEqualExceptConst(dt) )
			{
				// Call the comparison function and return the result
				result = CallGlobalFunctionRetBool(leftObj, rightObj, scriptFunctions[func]->sysFuncIntf, scriptFunctions[func]);
				return 0;
			}
		}
	}

	// If the behaviour is not supported we return an error
	result = false;
	return asNOT_SUPPORTED;
}

void asCScriptEngine::AddRefScriptObject(void *obj, int typeId)
{
	// Make sure it is not a null pointer
	if( obj == 0 ) return;

	// Make sure the type id is for an object type or a handle
	if( (typeId & asTYPEID_MASK_OBJECT) == 0 ) return;

	const asCDataType *dt = GetDataTypeFromTypeId(typeId);

	// Is the type id valid?
	if( !dt ) return;

	asCObjectType *objType = dt->GetObjectType();

	if( objType->beh.addref )
	{
		// Call the addref behaviour
		CallObjectMethod(obj, objType->beh.addref);
	}
}

void asCScriptEngine::ReleaseScriptObject(void *obj, int typeId)
{
	// Make sure it is not a null pointer
	if( obj == 0 ) return;

	// Make sure the type id is for an object type or a handle
	if( (typeId & asTYPEID_MASK_OBJECT) == 0 ) return;

	const asCDataType *dt = GetDataTypeFromTypeId(typeId);

	// Is the type id valid?
	if( !dt ) return;

	asCObjectType *objType = dt->GetObjectType();

	if( objType->beh.release )
	{
		// Call the release behaviour
		CallObjectMethod(obj, objType->beh.release);
	}
	else
	{
		// Call the destructor
		if( objType->beh.destruct )
			CallObjectMethod(obj, objType->beh.destruct);

		// Then free the memory
		CallFree(obj);
	}
}

bool asCScriptEngine::IsHandleCompatibleWithObject(void *obj, int objTypeId, int handleTypeId)
{
	// if equal, then it is obvious they are compatible
	if( objTypeId == handleTypeId )
		return true;

	// Get the actual data types from the type ids
	const asCDataType *objDt = GetDataTypeFromTypeId(objTypeId);
	const asCDataType *hdlDt = GetDataTypeFromTypeId(handleTypeId);

	// A handle to const cannot be passed to a handle that is not referencing a const object
	if( objDt->IsHandleToConst() && !hdlDt->IsHandleToConst() )
		return false;

	if( objDt->GetObjectType() == hdlDt->GetObjectType() )
	{
		// The object type is equal
		return true;
	}
	else if( objDt->IsScriptObject() && obj )
	{
		// There's still a chance the object implements the requested interface
		asCObjectType *objType = ((asCScriptObject*)obj)->objType;
		if( objType->Implements(hdlDt->GetObjectType()) )
			return true;
	}

	return false;
}


int asCScriptEngine::BeginConfigGroup(const char *groupName)
{
	// Make sure the group name doesn't already exist
	for( asUINT n = 0; n < configGroups.GetLength(); n++ )
	{
		if( configGroups[n]->groupName == groupName )
			return asNAME_TAKEN;
	}

	if( currentGroup != &defaultGroup )
		return asNOT_SUPPORTED;

	asCConfigGroup *group = asNEW(asCConfigGroup)();
	group->groupName = groupName;

	configGroups.PushLast(group);
	currentGroup = group;

	return 0;
}

int asCScriptEngine::EndConfigGroup()
{
	// Raise error if trying to end the default config
	if( currentGroup == &defaultGroup )
		return asNOT_SUPPORTED;

	currentGroup = &defaultGroup;

	return 0;
}

int asCScriptEngine::RemoveConfigGroup(const char *groupName)
{
	for( asUINT n = 0; n < configGroups.GetLength(); n++ )
	{
		if( configGroups[n]->groupName == groupName )
		{
			asCConfigGroup *group = configGroups[n];
			if( group->refCount > 0 )
				return asCONFIG_GROUP_IS_IN_USE;

			// Verify if any objects registered in this group is still alive
			if( group->HasLiveObjects(this) )
				return asCONFIG_GROUP_IS_IN_USE;

			// Remove the group from the list
			if( n == configGroups.GetLength() - 1 )
				configGroups.PopLast();
			else
				configGroups[n] = configGroups.PopLast();

			// Remove the configurations registered with this group
			group->RemoveConfiguration(this);

			asDELETE(group,asCConfigGroup);
		}
	}

	return 0;
}

asCConfigGroup *asCScriptEngine::FindConfigGroup(asCObjectType *ot)
{
	// Find the config group where this object type is registered
	for( asUINT n = 0; n < configGroups.GetLength(); n++ )
	{
		for( asUINT m = 0; m < configGroups[n]->objTypes.GetLength(); m++ )
		{
			if( configGroups[n]->objTypes[m] == ot )
				return configGroups[n];
		}
	}

	return 0;
}

asCConfigGroup *asCScriptEngine::FindConfigGroupForFunction(int funcId)
{
	for( asUINT n = 0; n < configGroups.GetLength(); n++ )
	{
		// Check global functions
		asUINT m;
		for( m = 0; m < configGroups[n]->scriptFunctions.GetLength(); m++ )
		{
			if( configGroups[n]->scriptFunctions[m]->id == funcId )
				return configGroups[n];
		}

		// Check global behaviours
		for( m = 0; m < configGroups[n]->globalBehaviours.GetLength(); m++ )
		{
			int id = configGroups[n]->globalBehaviours[m]+1;
			if( funcId == globalBehaviours.operators[id] )
				return configGroups[n];
		}
	}

	return 0;
}


asCConfigGroup *asCScriptEngine::FindConfigGroupForGlobalVar(int gvarId)
{
	for( asUINT n = 0; n < configGroups.GetLength(); n++ )
	{
		for( asUINT m = 0; m < configGroups[n]->globalProps.GetLength(); m++ )
		{
			if( configGroups[n]->globalProps[m]->index == gvarId )
				return configGroups[n];
		}
	}

	return 0;
}

asCConfigGroup *asCScriptEngine::FindConfigGroupForObjectType(const asCObjectType *objType)
{
	for( asUINT n = 0; n < configGroups.GetLength(); n++ )
	{
		for( asUINT m = 0; m < configGroups[n]->objTypes.GetLength(); m++ )
		{
			if( configGroups[n]->objTypes[m] == objType )
				return configGroups[n];
		}
	}

	return 0;
}

int asCScriptEngine::SetConfigGroupModuleAccess(const char *groupName, const char *module, bool hasAccess)
{
	asCConfigGroup *group = 0;

	// Make sure the group name doesn't already exist
	for( asUINT n = 0; n < configGroups.GetLength(); n++ )
	{
		if( configGroups[n]->groupName == groupName )
		{
			group = configGroups[n];
			break;
		}
	}

	if( group == 0 )
		return asWRONG_CONFIG_GROUP;

	return group->SetModuleAccess(module, hasAccess);
}

int asCScriptEngine::GetNextScriptFunctionId()
{
	if( freeScriptFunctionIds.GetLength() )
		return freeScriptFunctionIds.PopLast();

	int id = (int)scriptFunctions.GetLength();
	scriptFunctions.PushLast(0);
	return id;
}

void asCScriptEngine::SetScriptFunction(asCScriptFunction *func)
{
	scriptFunctions[func->id] = func;
}

void asCScriptEngine::DeleteScriptFunction(int id)
{
	if( id < 0 ) return;
	id &= 0xFFFF;
	if( id >= (int)scriptFunctions.GetLength() ) return;

	if( scriptFunctions[id] )
	{
		asCScriptFunction *func = scriptFunctions[id];

		// Remove the function from the list of script functions
		if( id == (int)scriptFunctions.GetLength() - 1 )
		{
			scriptFunctions.PopLast();
		}
		else
		{
			scriptFunctions[id] = 0;
			freeScriptFunctionIds.PushLast(id);
		}

		// Is the function used as signature id?
		if( func->signatureId == id )
		{
			// Remove the signature id
			signatureIds.RemoveValue(func);

			// Update all functions using the signature id
			int newSigId = 0;
			for( asUINT n = 0; n < scriptFunctions.GetLength(); n++ )
			{
				if( scriptFunctions[n] && scriptFunctions[n]->signatureId == id )
				{
					if( newSigId == 0 )
					{
						newSigId = scriptFunctions[n]->id;
						signatureIds.PushLast(scriptFunctions[n]);
					}

					scriptFunctions[n]->signatureId = newSigId;
				}
			}
		}

		// Is the function a registered global function?
		if( func->funcType == asFUNC_SYSTEM && func->objectType == 0 )
		{
			registeredGlobalFuncs.RemoveValue(func);
		}

		// Delete the script function
		asDELETE(func,asCScriptFunction);
	}
}

// interface
// TODO: typedef: Accept complex types for the typedefs
int asCScriptEngine::RegisterTypedef(const char *type, const char *decl)
{
	if( type == 0 ) return ConfigError(asINVALID_NAME);

	// Verify if the name has been registered as a type already
	asUINT n;
	for( n = 0; n < objectTypes.GetLength(); n++ )
	{
		if( objectTypes[n] && objectTypes[n]->name == type )
			return asALREADY_REGISTERED;
	}

	// Grab the data type
	asCTokenizer t;
	size_t tokenLen;
	eTokenType token;
	asCDataType dataType;

	//	Create the data type
	token = t.GetToken(decl, strlen(decl), &tokenLen);
	switch(token)
	{
	case ttBool:
	case ttInt:
	case ttInt8:
	case ttInt16:
	case ttInt64:
	case ttUInt:
	case ttUInt8:
	case ttUInt16:
	case ttUInt64:
	case ttFloat:
	case ttDouble:
		if( strlen(decl) != tokenLen )
		{
			return ConfigError(asINVALID_TYPE);
		}
		break;

	default:
		return ConfigError(asINVALID_TYPE);
	}

	dataType = asCDataType::CreatePrimitive(token, false);

	// Make sure the name is not a reserved keyword
	token = t.GetToken(type, strlen(type), &tokenLen);
	if( token != ttIdentifier || strlen(type) != tokenLen )
		return ConfigError(asINVALID_NAME);

	asCBuilder bld(this, 0);
	int r = bld.CheckNameConflict(type, 0, 0);
	if( r < 0 )
		return ConfigError(asNAME_TAKEN);

	// Don't have to check against members of object
	// types as they are allowed to use the names

	// Put the data type in the list
	asCObjectType *object= asNEW(asCObjectType)(this);
	object->arrayType = 0;
	object->flags = asOBJ_TYPEDEF;
	object->size = dataType.GetSizeInMemoryBytes();
	object->name = type;
	object->tokenType = dataType.GetTokenType();

	objectTypes.PushLast(object);
	registeredTypeDefs.PushLast(object);

	currentGroup->objTypes.PushLast(object);

	return asSUCCESS;
}

// interface
int asCScriptEngine::GetTypedefCount()
{
	return (int)registeredTypeDefs.GetLength();
}

// interface
const char *asCScriptEngine::GetTypedefByIndex(asUINT index, int *typeId, const char **configGroup)
{
	if( index >= registeredTypeDefs.GetLength() )
		return 0;

	if( typeId )
		*typeId = GetTypeIdByDecl(registeredTypeDefs[index]->name.AddressOf());

	if( configGroup )
	{
		asCConfigGroup *group = FindConfigGroupForObjectType(registeredTypeDefs[index]);
		if( group )
			*configGroup = group->groupName.AddressOf();
		else
			*configGroup = 0;
	}

	return registeredTypeDefs[index]->name.AddressOf();
}

// interface
int asCScriptEngine::RegisterEnum(const char *name)
{
	//	Check the name
	if( NULL == name )
		return ConfigError(asINVALID_NAME);

	// Verify if the name has been registered as a type already
	asUINT n;
	for( n = 0; n < objectTypes.GetLength(); n++ )
		if( objectTypes[n] && objectTypes[n]->name == name )
			return asALREADY_REGISTERED;

	// Use builder to parse the datatype
	asCDataType dt;
	asCBuilder bld(this, 0);
	bool oldMsgCallback = msgCallback; msgCallback = false;
	int r = bld.ParseDataType(name, &dt);
	msgCallback = oldMsgCallback;
	if( r >= 0 )
		return ConfigError(asERROR);

	// Make sure the name is not a reserved keyword
	asCTokenizer t;
	size_t tokenLen;
	int token = t.GetToken(name, strlen(name), &tokenLen);
	if( token != ttIdentifier || strlen(name) != tokenLen )
		return ConfigError(asINVALID_NAME);

	r = bld.CheckNameConflict(name, 0, 0);
	if( r < 0 )
		return ConfigError(asNAME_TAKEN);

	asCObjectType *st = asNEW(asCObjectType)(this);

	asCDataType dataType;
	dataType.CreatePrimitive(ttInt, false);

	st->arrayType = 0;
	st->flags = asOBJ_ENUM;
	st->size = dataType.GetSizeInMemoryBytes();
	st->name = name;
	st->tokenType = ttIdentifier;

	objectTypes.PushLast(st);
	registeredEnums.PushLast(st);

	currentGroup->objTypes.PushLast(st);

	return asSUCCESS;
}

// interface
int asCScriptEngine::RegisterEnumValue(const char *typeName, const char *valueName, int value)
{
	// Verify that the correct config group is used
	if( currentGroup->FindType(typeName) == 0 )
		return asWRONG_CONFIG_GROUP;

	asCDataType dt;
	int r;
	asCBuilder bld(this, 0);
	r = bld.ParseDataType(typeName, &dt);
	if( r < 0 )
		return ConfigError(r);

	// Store the enum value
	asCObjectType *ot = dt.GetObjectType();
	if( ot == 0 || !(ot->flags & asOBJ_ENUM) )
		return ConfigError(asINVALID_TYPE);

	if( NULL == valueName )
		return ConfigError(asINVALID_NAME);

	for( unsigned int n = 0; n < ot->enumValues.GetLength(); n++ )
	{
		if( ot->enumValues[n]->name == valueName )
			return ConfigError(asALREADY_REGISTERED);
	}

	asSEnumValue *e = asNEW(asSEnumValue);
	e->name = valueName;
	e->value = value;

	ot->enumValues.PushLast(e);

	return asSUCCESS;
}

// interface
int asCScriptEngine::GetEnumCount()
{
	return (int)registeredEnums.GetLength();
}

// interface
const char *asCScriptEngine::GetEnumByIndex(asUINT index, int *enumTypeId, const char **configGroup)
{
	if( index >= registeredEnums.GetLength() )
		return 0;

	if( configGroup )
	{
		asCConfigGroup *group = FindConfigGroupForObjectType(registeredEnums[index]);
		if( group )
			*configGroup = group->groupName.AddressOf();
		else
			*configGroup = 0;
	}

	if( enumTypeId )
		*enumTypeId = GetTypeIdByDecl(registeredEnums[index]->name.AddressOf());

	return registeredEnums[index]->name.AddressOf();
}

// interface
int asCScriptEngine::GetEnumValueCount(int enumTypeId)
{
	const asCDataType *dt = GetDataTypeFromTypeId(enumTypeId);
	asCObjectType *t = dt->GetObjectType();
	if( t == 0 || !(t->GetFlags() & asOBJ_ENUM) ) 
		return asINVALID_TYPE;

	return (int)t->enumValues.GetLength();
}

// interface
const char *asCScriptEngine::GetEnumValueByIndex(int enumTypeId, asUINT index, int *outValue)
{
	// TODO: This same function is implemented in as_module.cpp as well. Perhaps it should be moved to asCObjectType?
	const asCDataType *dt = GetDataTypeFromTypeId(enumTypeId);
	asCObjectType *t = dt->GetObjectType();
	if( t == 0 || !(t->GetFlags() & asOBJ_ENUM) ) 
		return 0;

	if( index >= t->enumValues.GetLength() )
		return 0;

	if( outValue )
		*outValue = t->enumValues[index]->value;

	return t->enumValues[index]->name.AddressOf();
}

// interface
int asCScriptEngine::GetObjectTypeCount()
{
	return (int)registeredObjTypes.GetLength();
}

// interface
asIObjectType *asCScriptEngine::GetObjectTypeByIndex(asUINT index)
{
	if( index >= registeredObjTypes.GetLength() )
		return 0;

	return registeredObjTypes[index];
}

// interface
asIObjectType *asCScriptEngine::GetObjectTypeById(int typeId)
{
	const asCDataType *dt = GetDataTypeFromTypeId(typeId);

	// Is the type id valid?
	if( !dt ) return 0;

	// Enum types are not objects, so we shouldn't return an object type for them
	if( dt->GetObjectType() && dt->GetObjectType()->GetFlags() & asOBJ_ENUM )
		return 0;

	return dt->GetObjectType();
}


asIScriptFunction *asCScriptEngine::GetFunctionDescriptorById(int funcId)
{
	return GetScriptFunction(funcId);
}


#ifdef AS_DEPRECATED

// deprecated since 2008-12-02, 2.15.0
asIScriptFunction *asCScriptEngine::GetFunctionDescriptorByIndex(const char *module, int index)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return 0;

	return mod->GetFunctionDescriptorByIndex(index);
}

// Deprecated since 2008-11-05, 2.15.0
int asCScriptEngine::GetObjectsInGarbageCollectorCount()
{
	asUINT currentSize;
	gc.GetStatistics(&currentSize, 0, 0);
	return currentSize;
}

// deprecated since 2008-12-01, 2.15.0
int asCScriptEngine::SaveByteCode(const char *module, asIBinaryStream *stream)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return asNO_MODULE;

	return mod->SaveByteCode(stream);
}

// deprecated since 2008-12-01, 2.15.0
int asCScriptEngine::LoadByteCode(const char *module, asIBinaryStream *stream)
{
	asCModule *mod = GetModule(module, true);
	if( mod == 0 ) return asNO_MODULE;

	if( mod->IsUsed() )
	{
		// Discard this module and get another
		mod->Discard();
		mod = GetModule(module, true);
	}

	return mod->LoadByteCode(stream);
}

// deprecated since 2008-12-01, 2.15.0
int asCScriptEngine::GetImportedFunctionCount(const char *module)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return asNO_MODULE;

	return mod->GetImportedFunctionCount();
}

// deprecated since 2008-12-01, 2.15.0
int asCScriptEngine::GetImportedFunctionIndexByDecl(const char *module, const char *decl)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return asNO_MODULE;

	return mod->GetImportedFunctionIndexByDecl(decl);
}

// deprecated since 2008-12-01, 2.15.0
const char *asCScriptEngine::GetImportedFunctionDeclaration(const char *module, int index, int *length)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return 0;

	return mod->GetImportedFunctionDeclaration(index, length);
}

// deprecated since 2008-12-01, 2.15.0
const char *asCScriptEngine::GetImportedFunctionSourceModule(const char *module, int index, int *length)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return 0;

	return mod->GetImportedFunctionSourceModule(index, length);
}

// deprecated since 2008-12-01, 2.15.0
int asCScriptEngine::BindImportedFunction(const char *module, int index, int funcID)
{
	asCModule *dstModule = GetModule(module, false);
	if( dstModule == 0 ) return asNO_MODULE;

	return dstModule->BindImportedFunction(index, funcID);
}

// deprecated since 2008-12-01, 2.15.0
int asCScriptEngine::UnbindImportedFunction(const char *module, int index)
{
	asCModule *dstModule = GetModule(module, false);
	if( dstModule == 0 ) return asNO_MODULE;

	return dstModule->UnbindImportedFunction(index);
}

// deprecated since 2008-12-01, 2.15.0
int asCScriptEngine::BindAllImportedFunctions(const char *module)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return asNO_MODULE;

	return mod->BindAllImportedFunctions();
}

// deprecated since 2008-12-01, 2.15.0
int asCScriptEngine::UnbindAllImportedFunctions(const char *module)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return asNO_MODULE;

	return mod->UnbindAllImportedFunctions();
}

// deprecated since 2008-12-02, 2.15.0
int asCScriptEngine::GetGlobalVarCount(const char *module)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return asNO_MODULE;

	return mod->GetGlobalVarCount();
}

// deprecated since 2008-12-02, 2.15.0
int asCScriptEngine::GetGlobalVarIndexByName(const char *module, const char *name)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return asNO_MODULE;

	return mod->GetGlobalVarIndexByName(name);
}

// deprecated since 2008-12-02, 2.15.0
int asCScriptEngine::GetGlobalVarIndexByDecl(const char *module, const char *decl)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return asNO_MODULE;

	return mod->GetGlobalVarIndexByDecl(decl);
}

// deprecated since 2008-12-02, 2.15.0
const char *asCScriptEngine::GetGlobalVarDeclaration(const char *module, int index, int *length)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return 0;

	return mod->GetGlobalVarDeclaration(index, length);
}

// deprecated since 2008-12-02, 2.15.0
const char *asCScriptEngine::GetGlobalVarName(const char *module, int index, int *length)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return 0;

	return mod->GetGlobalVarName(index, length);
}

// deprecated since 2008-12-02, 2.15.0
void *asCScriptEngine::GetAddressOfGlobalVar(const char *module, int index)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return 0;

	return mod->GetAddressOfGlobalVar(index);
}

// deprecated since 2008-12-02, 2.15.0
int asCScriptEngine::ResetModule(const char *module)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return asNO_MODULE;

	return mod->ResetGlobalVars();
}

// deprecated since 2008-12-02, 2.15.0
int asCScriptEngine::GetFunctionCount(const char *module)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return asNO_MODULE;

	return mod->GetFunctionCount();
}

// deprecated since 2008-12-02, 2.15.0
int asCScriptEngine::GetFunctionIDByIndex(const char *module, int index)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return asNO_MODULE;

	return mod->GetFunctionIdByIndex(index);
}

// deprecated since 2008-12-02, 2.15.0
int asCScriptEngine::GetFunctionIDByName(const char *module, const char *name)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return asNO_MODULE;

	return mod->GetFunctionIdByName(name);
}

// deprecated since 2008-12-02, 2.15.0
int asCScriptEngine::GetFunctionIDByDecl(const char *module, const char *decl)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return asNO_MODULE;

	return mod->GetFunctionIdByDecl(decl);
}

// deprecated since 2008-12-02, 2.15.0
int asCScriptEngine::AddScriptSection(const char *module, const char *name, const char *code, size_t codeLength, int lineOffset)
{
	asCModule *mod = GetModule(module, true);
	if( mod == 0 ) return asNO_MODULE;

	// Discard the module if it is in use
	if( mod->IsUsed() )
	{
		mod->Discard();

		// Get another module
		mod = GetModule(module, true);
	}

	return mod->AddScriptSection(name, code, (int)codeLength, lineOffset);
}

// deprecated since 2008-12-02, 2.15.0
int asCScriptEngine::Build(const char *module)
{
	asCModule *mod = GetModule(module, false);
	if( mod == 0 ) return asNO_MODULE;

	return mod->Build();
}

// deprecated since 2008-12-03, 2.15.0
int asCScriptEngine::Discard(const char *module)
{
	return DiscardModule(module);
}

// deprecated since 2008-12-04, 2.15.0
int asCScriptEngine::GetTypeIdByDecl(const char *module, const char *decl)
{
	asCModule *mod = GetModule(module, false);

	asCDataType dt;
	asCBuilder bld(this, mod);
	int r = bld.ParseDataType(decl, &dt);
	if( r < 0 )
		return asINVALID_TYPE;

	return GetTypeIdFromDataType(dt);
}

#endif


END_AS_NAMESPACE

