#include "Input/InputContext.h"

namespace Okari
{
    InputContext::InputContext(const std::string& name)
        : m_Name(name)
    { }

    void InputContext::BindKey(int key, const std::string& action)
    {
        m_ActionBindings[action] = key;
    }

    bool InputContext::HasAction(const std::string& action) const
    {
        return m_ActionBindings.find(action) != m_ActionBindings.end();
    }

    int InputContext::GetKeyForAction(const std::string& action) const
    {
        auto it = m_ActionBindings.find(action);

        if (it == m_ActionBindings.end())
            return -1;

        return it->second;
    }
}