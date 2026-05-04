#pragma once

#include "ScriptWindowInstance.generated.h"

#include "anox/Game/APEWindowCommandHandler.generated.h"

namespace anox::game
{
	class ScriptWindowInstance final : public ObjectRTTI<ScriptWindowInstance>, public ape::IAPEWindowCommandHandler
	{
	public:
		void Initialize(Label label);

		const Label &GetWindowID() const;

	private:
		rkit::Result InvalidCommand() override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::StartSwitch &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::ThinkSwitch &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::FinishSwitch &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::StartConsole &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::Body &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::Choice &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::Background &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::End &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::Font &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::Dimensions &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::SubWindow &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::Image &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::Flags &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::Cam &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::XYPrintFX &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::FinishConsole &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::NextWindow &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::Title &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::Style &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ape::Talk &cmd) override;

	};
}
