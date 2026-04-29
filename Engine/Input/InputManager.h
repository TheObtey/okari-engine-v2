#pragma once

#include "Input/InputContext.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Okari
{
    class InputManager
    {
    public:
        static void RegisterContext(std::shared_ptr<InputContext> context);

        static void PushContext(const std::string& name);
        static void PopContext();
        static void PopContext(const std::string& name);

        static bool IsActionPressed(const std::string& action);
        static bool IsActionHeld(const std::string& action);
        static bool IsActionReleased(const std::string& action);

    private:
        static InputContext* GetActiveContextForAction(const std::string& action);

    private:
        static std::unordered_map<std::string, std::shared_ptr<InputContext>> s_Contexts;
        static std::vector<std::string> s_ContextStack;
    };
}