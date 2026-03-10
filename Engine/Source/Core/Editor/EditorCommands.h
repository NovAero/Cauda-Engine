#pragma once
#include "Core/CoreDefinitions.h"
#include "ThirdParty/Flecs.h"
#include "Core/Components/Components.h"
#include "Core/Utilities/HistoryStack.h"
#include "Core/Utilities/Logger.h"
#include "Core/Math/EngineMath.h"
#include "ResourceLibrary.h"

namespace Cauda
{
	template <typename T>
	struct ValueCommand : public Command
	{
		ValueCommand(T* target, T oldVal, T newVal) : m_valueTarget(target), m_oldValue(oldVal), m_newValue(newVal) {}

		virtual void Execute() override {
			*m_valueTarget = m_newValue;
		}
		virtual void Undo() override {
			*m_valueTarget = m_oldValue;
		}

	private:
		T* m_valueTarget;
		T m_oldValue;
		T m_newValue;
	};

	template <typename T>
	struct EditorValueCommand : public Command
	{
		EditorValueCommand(T* target, T* editorValue) :
			m_valueTarget(target),
			m_oldValue(*target),
			m_displayValue(editorValue),
			m_newValue(*editorValue)
		{}

		virtual void Execute() override
		{
			*m_valueTarget = m_newValue;
			*m_displayValue = m_newValue;
		}

		virtual void Undo() override
		{
			*m_valueTarget = m_oldValue;
			*m_displayValue = m_oldValue;
		}

	private:

		T*	m_valueTarget;
		T	m_oldValue;
		T*	m_displayValue;
		T   m_newValue;
	};

	//Always treat this command as local space for objects (Automatically triggers OnSet event for TransformComponents)
	struct GizmoModifyCommand : public Command
	{
		GizmoModifyCommand(flecs::entity entity, TransformComponent* target, TransformComponent* display) :
			m_entityTarget(entity),
			m_valueTarget(target),
			m_oldValue(*target),
			m_displayValue(display),
			m_newValue(*display)
		{}

		virtual void Execute() override
		{
			*m_valueTarget = m_newValue;
			*m_displayValue = m_newValue;
			m_entityTarget.modified<TransformComponent>();
		}

		virtual void Undo() override
		{
			*m_valueTarget = m_oldValue;
			*m_displayValue = m_oldValue;
			m_entityTarget.modified<TransformComponent>();
		}

		flecs::entity m_entityTarget;
		TransformComponent* m_valueTarget;
		TransformComponent	m_oldValue;
		TransformComponent* m_displayValue;
		TransformComponent  m_newValue;

	};

	//struct QuatVec3EVCommand : public Command
	//{
	//	QuatVec3EVCommand(glm::quat* target, glm::vec3* slider) :
	//		m_quatTarget(target),
	//		m_vec3Target(slider),
	//		m_oldQuatValue(*target)
	//	{
	//		m_newQuatValue = glm::quat(glm::radians(*m_vec3Target));
	//	}

	//	virtual void Execute() override
	//	{
	//		glm::quat temp = m_newQuatValue;
	//		*m_quatTarget = m_newQuatValue;
	//		*m_vec3Target = glm::degrees(glm::eulerAngles(temp));
	//	}

	//	virtual void Undo() override
	//	{
	//		glm::quat temp = m_oldQuatValue;
	//		*m_quatTarget = m_oldQuatValue;
	//		*m_vec3Target = glm::degrees(glm::eulerAngles(temp));
	//	}

	//private:

	//	glm::quat* m_quatTarget;
	//	glm::vec3* m_vec3Target;
	//	glm::quat m_oldQuatValue;
	//	glm::quat m_newQuatValue;
	//};
	
	struct SelectEntityCommand : public Command
	{
		SelectEntityCommand(flecs::entity selected, flecs::entity previous) :
			m_selectedEntity(selected),
			m_previousEntity(previous)
		{}

		virtual void Execute() override
		{
			if (m_previousEntity.is_alive()) 
			{
				m_previousEntity.remove<SelectedTag>();
			}
			if (m_selectedEntity.is_alive())
			{
				m_selectedEntity.add<SelectedTag>();
			}
		}

		virtual void Undo() override
		{
			if (m_selectedEntity.is_alive())
			{
				m_selectedEntity.remove<SelectedTag>();
			}
			if (m_previousEntity.is_alive())
			{
				m_previousEntity.add<SelectedTag>();
			}
		}

	private:

		flecs::entity m_selectedEntity;
		flecs::entity m_previousEntity;
	};


	struct RenameEntityCommand : public Command
	{
		RenameEntityCommand(flecs::entity entity, flecs::entity parent, std::string name, flecs::world& world) :
			m_entity(entity),
			m_parent(parent),
			m_world(world),
			m_name(name)
		{
		}

		virtual void Execute() override
		{
			if (m_entity.is_alive())
			{
				PropagateRename(m_entity, m_parent, m_name, false, true);
			}
		}

		virtual void Undo() override
		{
			if (m_entity.is_alive())
			{
				PropagateRename(m_entity, m_parent, m_previousName, true, true);
			}
		}

	private:

		bool PropagateRename(flecs::entity entity, flecs::entity parent, std::string name, bool undo = false, bool first = false)
		{
			std::string parentName = parent ? parent.name().c_str() : "";
			std::string formattedName = ResourceLibrary::FormatRelationshipName(ResourceLibrary::GetStrippedName(name), parentName);
			if (first)
			{
				if (undo) {
					m_name = formattedName;
				}
				else {
					m_previousName = formattedName;
				}
			}
			
			if (parent)
			{
				bool nameUsed = ResourceLibrary::DoesNameExistInScope(parent, formattedName);
				while (nameUsed) {
					formattedName = ResourceLibrary::GenerateUniqueName(formattedName, parent.name().c_str());
					nameUsed = ResourceLibrary::DoesNameExistInScope(parent, formattedName);
				};
			}
			else
			{
				bool nameUsed = m_world.lookup(formattedName.c_str());
				while (nameUsed) {
					formattedName = ResourceLibrary::GenerateUniqueName(formattedName, parentName);
					nameUsed = m_world.lookup(formattedName.c_str());
				};
			}

			entity.set_name(formattedName.c_str());

			bool hasChildren = false;
			entity.children([&](flecs::entity child)
				{
					hasChildren = PropagateRename(child, entity, child.name().c_str(), undo);
				});
			return hasChildren;
		}

		flecs::world& m_world;
		flecs::entity m_entity;
		flecs::entity m_parent;
		std::string m_name;
		std::string m_previousName;
	};

	struct CreateEntityCommand : public Command
	{
		CreateEntityCommand(std::string name, flecs::entity parent, flecs::world& world) :
			m_entityName(name),
			m_parent(parent),
			m_world(world)
		{}

		virtual void Execute() override
		{
			m_entity = m_world.entity()
				.set<TransformComponent>(TransformComponent{
					.position = glm::vec3(0),
					.cachedEuler = glm::vec3(0),
					//.rotation = glm::quat(1,0,0,0),
					.scale = glm::vec3(1) })
				.add(flecs::ChildOf, m_parent);

			auto command = new RenameEntityCommand(m_entity, m_parent, m_entityName, m_world);
			command->Execute();
			delete command;

			m_entityName = m_entity.name().c_str();

			EditorConsole::Inst()->AddConsoleMessage("Created entity " + m_entityName, false);
		}

		virtual void Undo() override
		{
			m_entity.destruct();
			HistoryStack::PopRedo();
		}

		std::string m_entityName;
		flecs::entity m_entity;
		flecs::entity& m_parent;
		flecs::world& m_world;
	};

	struct DeleteEntityCommand : public Command
	{
		DeleteEntityCommand(flecs::entity entity) : 
			m_entity(entity),
			m_entityName(entity.name())
		{}

		virtual void Execute() override
		{
			m_entity.destruct();
			EditorConsole::Inst()->AddConsoleMessage("Deleted entity " + m_entityName, false);
		}

		virtual void Undo() override
		{
			HistoryStack::PopRedo();
		}

	private:

		std::string m_entityName;
		flecs::entity m_entity;
	};

	template<typename C>
	struct AddComponentCommand : public Command
	{
		AddComponentCommand(flecs::entity entity) :
			m_entity(entity)
		{}

		virtual void Execute() override
		{
			if (!m_entity.is_alive()) return;
			if (!m_entity.has<C>())
			{
				if constexpr (std::is_default_constructible_v<C>)
				{
					m_entity.add<C>();
				}
				else
				{
					m_entity.set<C>({});
				}

				std::string message = "Added ";
				std::string typeName = typeid(C).name();
				typeName.erase(0, typeName.find_first_of(" ") + 1);
				message.append(typeName);
				message.append(" to entity ");
				message.append(m_entity.name());
				EditorConsole::Inst()->AddConsoleMessage(message);
			}
		}

		virtual void Undo() override
		{
			if (!m_entity.is_alive()) return;
			m_entity.remove<C>();

			std::string message = "Removed ";
			std::string typeName = typeid(C).name();
			typeName.erase(0, typeName.find_first_of(" ") + 1);
			message.append(typeName);
			message.append(" from entity ");
			message.append(m_entity.name());
		}

	private:

		flecs::entity m_entity;
	};

	template<typename C>
	struct RemoveComponentCommand : public Command
	{
		RemoveComponentCommand(flecs::entity entity) : m_entity(entity) {}

		virtual void Execute()
		{
			if (!m_entity.is_alive()) return;
			if (m_entity.has<C>())
			{
				m_cachedValue = m_entity.get<C>();
				m_entity.remove<C>();

				std::string message = "Removed ";
				std::string typeName = typeid(C).name();
				typeName.erase(0, typeName.find_first_of(" ") + 1);
				message.append(typeName);
				message.append(" from entity ");
				message.append(m_entity.name());

				EditorConsole::Inst()->AddConsoleMessage(message, false);
			}
		}

		virtual void Undo()
		{
			if (!m_entity.is_alive()) return;
			if (!m_entity.has<C>())
			{
				if constexpr (std::is_default_constructible_v<C>)
				{
					m_entity.add<C>();
					m_entity.set<C>(m_cachedValue);
				}
				else
				{
					m_entity.set<C>(m_cachedValue);
				}

				std::string message = "Added ";
				std::string typeName = typeid(C).name();
				typeName.erase(0, typeName.find_first_of(" ") + 1);
				message.append(typeName);
				message.append(" to entity ");
				message.append(m_entity.name());

				EditorConsole::Inst()->AddConsoleMessage(message, false);
			}
		}

	private:
		flecs::entity m_entity;
		C m_cachedValue;
	};

	struct ReparentCommand : public Command
	{
		ReparentCommand(flecs::entity target, flecs::entity parent, flecs::entity previous, flecs::world& world) :
			m_target(target),
			m_parent(parent),
			m_previousParent(previous),
			m_world(world)
		{
		}

		virtual void Execute() override
		{
			if (!m_target.is_alive() && !m_parent.is_alive()) return;

			auto transform = Math::LocalToWorldTform(m_target);

			auto command = new RenameEntityCommand(m_target, m_parent, ResourceLibrary::GetStrippedName(m_target.name().c_str()), m_world);
			command->Execute();
			delete command;

			if (m_previousParent.is_alive() && m_target.has(flecs::ChildOf, m_previousParent))
			{
				m_target.remove(flecs::ChildOf, m_previousParent);
			}

			m_target.add(flecs::ChildOf, m_parent);

			UpdateLocalToParent(transform);

			std::string message = "Reparented entity ";
			message.append(m_target.name());
			message.append(" to ");
			message.append(m_parent.name());

			EditorConsole::Inst()->AddConsoleMessage(message, false);
		}

		virtual void Undo() override
		{
			if (!m_target.is_alive() && !m_previousParent.is_alive()) return;

			auto transform = Math::LocalToWorldTform(m_target);

			auto command = new RenameEntityCommand(m_target, m_previousParent, ResourceLibrary::GetStrippedName(m_target.name().c_str()), m_world);
			command->Execute();
			delete command;

			if (m_parent.is_alive())
			{
				m_target.remove(flecs::ChildOf, m_parent);
			}

			m_target.add(flecs::ChildOf, m_previousParent);

			UpdateLocalToParent(transform);

			std::string message = "Reparented entity ";
			message.append(m_target.name());
			message.append(" to ");
			message.append(m_previousParent.name());

			EditorConsole::Inst()->AddConsoleMessage(message, false);
		}

	private:

		void UpdateLocalToParent(TransformComponent previousWorldTransform)
		{
			auto transform = m_target.try_get_mut<TransformComponent>();
			auto parentTransform = Math::LocalToWorldTform(m_target.parent());
			auto previousWorldMat = Math::TransformToMatrix(previousWorldTransform);

			glm::mat4 parentWorldMatrix = Math::LocalToWorldMat(m_target.parent());

			glm::mat4 newLocalMatrix = glm::inverse(parentWorldMatrix) * previousWorldMat;

			*transform = Math::MatrixToTransform(newLocalMatrix);

			m_target.modified<TransformComponent>();
		}

		flecs::entity m_target;
		flecs::entity m_parent;
		flecs::entity m_previousParent;
		flecs::world& m_world;
	};
}