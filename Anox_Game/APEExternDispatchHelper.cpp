#include "APEExternDispatchHelper.h"

#include "anox/Game/APEScriptValues.h"
#include "ScriptEnvironment.h"


#define ANOX_APE_EXTERN_PARSER_STUB(type)	\
	rkit::Result ExternDispatch::Parse ## type ## Arg(type ## Arg_t &arg, ScriptEnvironment &env, const ScriptPackage &pkg, const ScriptExprValue &operand) \
	{ \
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented); \
	}


namespace anox::game::ape::externs
{
	rkit::Result ExternDispatch::ParseStrArg(rkit::ByteString &arg, ScriptEnvironment &env, const ScriptPackage &pkg, const ScriptExprValue &operand)
	{
		bool succeeded = false;
		RKIT_CHECK(env.TryEvaluateStringScriptExpr(succeeded, arg, pkg, operand));
		if (!succeeded)
			arg.Clear();

		RKIT_RETURN_OK;
	}

	rkit::Result ExternDispatch::ParseFileResourceArg(rkit::data::ContentID &arg, ScriptEnvironment &env, const ScriptPackage &pkg, const ScriptExprValue &operand)
	{
		if (!env.TryEvaluateContentIDScriptExpr(arg, pkg, operand))
			arg = rkit::data::ContentID();

		RKIT_RETURN_OK;
	}

	ANOX_APE_EXTERN_PARSER_STUB(Bool)
	ANOX_APE_EXTERN_PARSER_STUB(UInt32)
	ANOX_APE_EXTERN_PARSER_STUB(Int32)
	ANOX_APE_EXTERN_PARSER_STUB(Label)

	ANOX_APE_EXTERN_PARSER_STUB(Float)
	ANOX_APE_EXTERN_PARSER_STUB(FloatExpr)
	ANOX_APE_EXTERN_PARSER_STUB(FloatVar)

	ANOX_APE_EXTERN_PARSER_STUB(Obj)
	ANOX_APE_EXTERN_PARSER_STUB(ObjVar)

	ANOX_APE_EXTERN_PARSER_STUB(Texture)
	ANOX_APE_EXTERN_PARSER_STUB(TextureVar)
	ANOX_APE_EXTERN_PARSER_STUB(StrVar)

	ANOX_APE_EXTERN_PARSER_STUB(ScriptResource)
	ANOX_APE_EXTERN_PARSER_STUB(ImageResource)
	ANOX_APE_EXTERN_PARSER_STUB(SoundResource)
	ANOX_APE_EXTERN_PARSER_STUB(FontResource)
	ANOX_APE_EXTERN_PARSER_STUB(SceneResource)
	ANOX_APE_EXTERN_PARSER_STUB(ParticleResource)
	ANOX_APE_EXTERN_PARSER_STUB(ModelResource)
}

#undef ANOX_APE_EXTERN_PARSER_STUB
