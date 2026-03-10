#include "cepch.h"
#include "LayerStack.h"

namespace Cauda
{
	LayerStack::~LayerStack()
	{
		for (Layer* layer : m_layers) {
			if (!layer) continue;
			layer->OnDetach();
			delete layer;
		}
	}

	void LayerStack::PushLayer(Layer* layer)
	{
		m_layers.emplace(m_layers.begin() + m_layerInsertIndex, layer);
		m_layerInsertIndex++;
	}

	void LayerStack::PushOverlay(Layer* overlay)
	{
		m_layers.emplace_back(overlay);
	}

	void LayerStack::PopLayer(Layer* layer)
	{
		auto it = std::find(m_layers.begin(), m_layers.end(), layer);
		if (it != m_layers.end())
		{
			layer->OnDetach();
			m_layers.erase(it);
			m_layerInsertIndex--;
		}
	}

	void LayerStack::PopOverlay(Layer* overlay)
	{
		auto it = std::find(m_layers.begin() + m_layerInsertIndex, m_layers.end(), overlay);
		if (it != m_layers.end())
		{
			overlay->OnDetach();
			m_layers.erase(it);
		}
	}

	bool LayerStack::QueryLayer(Layer* layer)
	{
		if (std::find(m_layers.begin(), m_layers.end(), layer) == m_layers.end())
		{
			return false;
		}
		return true;
	}

}

