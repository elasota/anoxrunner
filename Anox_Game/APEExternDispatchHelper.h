#pragma once

#include "rkit/Core/Result.h"
#include "rkit/Core/String.h"

#include "anox/Label.h"

#define ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(type)	\
		static rkit::Result Parse ## type ## Arg(type ## Arg_t &arg, ScriptEnvironment &env, const ScriptPackage &pkg, const ScriptExprValue &operand)

namespace rkit::data
{
	struct ContentID;
}

namespace anox::game
{
	class ScriptEnvironment;
	struct ScriptExprValue;
	class ScriptPackage;
}

namespace anox::game::ape::externs
{
	class ExternDispatch
	{
	public:
		typedef bool BoolArg_t;
		typedef rkit::ByteString StrArg_t;
		typedef uint32_t UInt32Arg_t;
		typedef int32_t Int32Arg_t;
		typedef int32_t LabelArg_t;
		typedef float FloatArg_t;

		typedef int FloatExprArg_t;
		typedef int FloatVarArg_t;
		typedef int ObjArg_t;
		typedef int ObjVarArg_t;
		typedef int TextureArg_t;
		typedef int StrVarArg_t;
		typedef int TextureVarArg_t;

		typedef int ScriptResourceArg_t;
		typedef int ImageResourceArg_t;
		typedef int SoundResourceArg_t;
		typedef int FontResourceArg_t;
		typedef rkit::data::ContentID FileResourceArg_t;
		typedef int SceneResourceArg_t;
		typedef int ParticleResourceArg_t;
		typedef int ModelResourceArg_t;

		// Parse funcs
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(Bool);
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(Str);
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(UInt32);
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(Int32);
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(Label);

		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(Float);
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(FloatExpr);
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(FloatVar);

		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(Obj);
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(ObjVar);

		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(Texture);
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(TextureVar);
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(StrVar);

		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(ScriptResource);
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(ImageResource);
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(SoundResource);
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(FontResource);
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(FileResource);
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(SceneResource);
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(ParticleResource);
		ANOX_APE_DECLARE_EXTERN_PARSER_FUNC(ModelResource);
	};
}

#undef ANOX_APE_DECLARE_EXTERN_PARSER_FUNC
