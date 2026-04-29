#include "Input/InputManager.h"
#include "Input/Input.h"
#include <algorithm>

namespace Okari
{
    std::unordered_map<std::string, std::shared_ptr<InputContext>> InputManager::s_Contexts;
    std::vector<std::string> InputManager::s_ContextStack;

    void InputManager::RegisterContext(std::shared_ptr<InputContext> context)
    {
        s_Contexts[context->GetName()] = context;
    }

    void InputManager::PushContext(const std::string& name)
    {
        if (s_Contexts.find(name) == s_Contexts.end())
            return;

        s_ContextStack.push_back(name);
    }

    void InputManager::PopContext()
    {
        if (!s_ContextStack.empty())
            s_ContextStack.pop_back();
    }

    void InputManager::PopContext(const std::string& name)
    {
        auto it = std::find(s_ContextStack.begin(), s_ContextStack.end(), name);

        if (it != s_ContextStack.end())
            s_ContextStack.erase(it);
    }

    bool InputManager::IsActionPressed(const std::string& action)
    {
        InputContext* context = GetActiveContextForAction(action);

        if (!context)
            return false;

        return Input::IsKeyPressed(context->GetKeyForAction(action));
    }

    bool InputManager::IsActionHeld(const std::string& action)
    {
        InputContext* context = GetActiveContextForAction(action);

        if (!context)
            return false;

        return Input::IsKeyHeld(context->GetKeyForAction(action));
    }

    bool InputManager::IsActionReleased(const std::string& action)
    {
        InputContext* context = GetActiveContextForAction(action);

        if (!context)
            return false;

        return Input::IsKeyReleased(context->GetKeyForAction(action));
    }

    InputContext* InputManager::GetActiveContextForAction(const std::string& action)
    {
        for (auto it = s_ContextStack.rbegin(); it != s_ContextStack.rend(); ++it)
        {
            auto contextIt = s_Contexts.find(*it);

            if (contextIt == s_Contexts.end())
                continue;

            InputContext* context = contextIt->second.get();

            if (context->HasAction(action))
                return context;
            
            if (context->IsBlocking())
                return nullptr;
        }

        return nullptr;
    }
}