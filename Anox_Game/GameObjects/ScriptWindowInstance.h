#pragma once

#include "ScriptWindowInstance.generated.h"

#include "anox/Game/APEWindowCommandHandler.generated.h"

namespace anox::game
{
	class ScriptWindowInstance final : public ObjectRTTI<ScriptWindowInstance>, public ape::IAPEWindowCommandHandler
	{
	public:
		ScriptWindowInstance();

		void Initialize(Label label);

		const Label &GetWindowID() const;
		const Label &GetStartSwitch() const;
		const Label &GetFinishSwitch() const;
		const Label &GetThinkSwitch() const;

		void ClearStyle();

	private:
		rkit::Result InvalidCommand() override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::StartSwitch &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::ThinkSwitch &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::FinishSwitch &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::StartConsole &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Body &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Choice &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Background &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::End &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Font &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Dimensions &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::SubWindow &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Image &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Flags &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Cam &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::XYPrintFX &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::FinishConsole &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::NextWindow &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Title &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Style &cmd) override;
		rkit::Result HandleCommand(ScriptEnvironment &env, const ScriptPackage &pkg, const ape::Talk &cmd) override;

	};
}
