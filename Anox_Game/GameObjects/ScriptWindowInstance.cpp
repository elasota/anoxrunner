#include "ScriptWindowInstance.h"

#include "ScriptWindowInstance.generated.inl"

#include "ScriptEnvironment.h"

#include "anox/Game/APECommandDispatcher.generated.h"

namespace anox::game
{
	ScriptWindowInstance::ScriptWindowInstance()
	{
	}

	void ScriptWindowInstance::Initialize(Label label)
	{
		m_windowID = label;
	}

	const Label &ScriptWindowInstance::GetWindowID() const
	{
		return this->m_windowID;
	}

	rkit::Result ScriptWindowInstance::InvalidCommand()
	{
		RKIT_THROW(rkit::ResultCode::kDataError);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::StartSwitch &cmd)
	{
		m_startSwitch = Label::FromRawValue(cmd.m_label);
		RKIT_RETURN_OK;
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::ThinkSwitch &cmd)
	{
		m_thinkSwitch = Label::FromRawValue(cmd.m_label);
		RKIT_RETURN_OK;
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::FinishSwitch &cmd)
	{
		m_finishSwitch = Label::FromRawValue(cmd.m_label);
		RKIT_RETURN_OK;
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::StartConsole &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Body &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Choice &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Background &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::End &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Font &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Dimensions &cmd)
	{
		const ScriptExprValue *const scriptExprs[4] =
		{
			&cmd.m_xpos,
			&cmd.m_ypos,
			&cmd.m_width,
			&cmd.m_height,
		};

		float *floatOutputs[4] =
		{
			&m_pos.ModifyX(),
			&m_pos.ModifyY(),
			&m_size.ModifyX(),
			&m_size.ModifyY(),
		};

		for (int i = 0; i < 4; i++)
		{
			float floatValue = 0.f;
			if (env.TryEvaluateFloatScriptExpr(floatValue, pkg, *scriptExprs[i]))
				(*floatOutputs[i]) = floatValue;
		}

		RKIT_RETURN_OK;
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::SubWindow &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Image &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Flags &cmd)
	{
		m_persist = cmd.m_persist;
		m_noBackground = cmd.m_noBackground;
		m_noScroll = cmd.m_noScroll;
		m_noGrab = cmd.m_noGrab;
		m_noRelease = cmd.m_noRelease;
		m_subtitle = cmd.m_subtitle;
		m_passive2D = cmd.m_passive2D;
		m_passive = cmd.m_passive;

		RKIT_RETURN_OK;
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Cam &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::XYPrintFX &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::FinishConsole &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::NextWindow &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Title &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Style &cmd)
	{
		switch (cmd.m_str.m_refType)
		{
		case ScriptMaterialReferenceType::Null:
			ClearStyle();
			RKIT_RETURN_OK;
		case ScriptMaterialReferenceType::WildcardString:
		case ScriptMaterialReferenceType::ContentID:
			RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
		default:
			RKIT_THROW(rkit::ResultCode::kDataError);
		}
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Talk &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	void ScriptWindowInstance::ClearStyle()
	{
	}
}
