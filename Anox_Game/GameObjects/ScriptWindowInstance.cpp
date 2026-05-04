#include "ScriptWindowInstance.h"

#include "ScriptWindowInstance.generated.inl"

#include "anox/Game/APECommandDispatcher.generated.h"

namespace anox::game
{
	const Label &ScriptWindowInstance::GetWindowID() const
	{
		return this->m_windowID;
	}

	rkit::Result ScriptWindowInstance::InvalidCommand()
	{
		RKIT_THROW(rkit::ResultCode::kDataError);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::StartSwitch &cmd)
	{
		m_startSwitch = Label::FromRawValue(cmd.m_label);
		RKIT_RETURN_OK;
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::ThinkSwitch &cmd)
	{
		m_thinkSwitch = Label::FromRawValue(cmd.m_label);
		RKIT_RETURN_OK;
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::FinishSwitch &cmd)
	{
		m_finishSwitch = Label::FromRawValue(cmd.m_label);
		RKIT_RETURN_OK;
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::StartConsole &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::Body &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::Choice &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::Background &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::End &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::Font &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::Dimensions &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::SubWindow &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::Image &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::Flags &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::Cam &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::XYPrintFX &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::FinishConsole &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::NextWindow &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::Title &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::Style &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result ScriptWindowInstance::HandleCommand(ScriptEnvironment &env, const ape::Talk &cmd)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}
}
