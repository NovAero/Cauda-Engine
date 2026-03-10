#pragma once
#include <stack>
#include "Core/CoreDefinitions.h"

namespace Cauda
{
	class HistoryStack
	{
	public:
		HistoryStack();
		~HistoryStack();

		static void Execute(Command* cmd);
		static void Undo();
		static void Redo();
		static void PopRedo();

	private:

		std::stack<Command*> m_undoStack;
		std::stack<Command*> m_redoStack;

		static HistoryStack* s_instance;
	};
}