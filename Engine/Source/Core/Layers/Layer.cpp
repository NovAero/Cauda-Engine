#include "cepch.h"
#include "Layer.h"

Cauda::Layer::Layer(const std::string& name)
{
#ifdef _DEBUG
	m_debugName = name;
#endif // _DEBUG
}

Cauda::Layer::~Layer()
{

}