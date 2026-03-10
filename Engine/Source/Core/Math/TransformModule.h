#pragma once
#include "ThirdParty/Flecs.h"
#include "Core/Components/Components.h"

class EditorModule;


class TransformModule
{
	friend class EditorModule;
public:
	TransformModule(flecs::world& world);
	~TransformModule() {}

	void ResetSpawnPointIDs();

private:
	void SetupComponents();
	void SetupSystems();
	void SetupObservers();
	void SetupQueries();

	void DrawTransformComponent(flecs::entity entity, TransformComponent& transform);

	void RegisterWithEditor();

private:
	EditorModule* m_editorModule;

	flecs::world& m_world;
	flecs::observer m_transformSetObserver;
	flecs::observer m_selectEntityObserver;

	TransformComponent m_cachedTransform;
	TransformComponent m_displayedTransform;

	int m_nextSpawnPointID = 0;

	template<typename T>
	void PushValueCommand(T* target, T* display);
};

#include "Core/Editor/EditorCommands.h"

template<typename T>
inline void TransformModule::PushValueCommand(T* target, T* display)
{
	Cauda::HistoryStack::Execute(new Cauda::EditorValueCommand<T>(target, display));
}