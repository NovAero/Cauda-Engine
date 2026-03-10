#include "cepch.h"
#include "HistoryStack.h"
#include "Logger.h"

namespace Cauda
{
	HistoryStack* HistoryStack::s_instance = nullptr;

	HistoryStack::HistoryStack()
	{
		s_instance = this;
	}

	HistoryStack::~HistoryStack()
	{
		while (!m_undoStack.empty())
		{
			delete m_undoStack.top();
			m_undoStack.pop();
		}
		while (!m_redoStack.empty())
		{
			delete m_redoStack.top();
			m_redoStack.pop();
		}
		s_instance = nullptr;
	}

	void HistoryStack::Execute(Command* cmd)
	{
		if (!cmd) return; //No nullptrs allowed >:(

		cmd->Execute();
		s_instance->m_undoStack.push(cmd);
		//Clear redo stack cause new command was pushed 
		while (!s_instance->m_redoStack.empty())
		{
			delete s_instance->m_redoStack.top();
			s_instance->m_redoStack.pop();
		}
	}

	void HistoryStack::Undo()
	{
		if (!s_instance->m_undoStack.empty())
		{
			Command* cmd = s_instance->m_undoStack.top();

			s_instance->m_undoStack.pop();
			s_instance->m_redoStack.push(cmd);

			cmd->Undo();
		
			Logger::PrintLog("Undo command called");
		}
	}

	void HistoryStack::Redo()
	{
		if (!s_instance->m_redoStack.empty())
		{
			Command* cmd = s_instance->m_redoStack.top();
			s_instance->m_redoStack.pop();
			cmd->Execute();

			Logger::PrintLog("Redo command called");

			s_instance->m_undoStack.push(cmd);
		}
	}

	void HistoryStack::PopRedo()
	{
		if (!s_instance->m_redoStack.empty())
		{
			delete s_instance->m_redoStack.top();
			s_instance->m_redoStack.pop();
		}
	}
}