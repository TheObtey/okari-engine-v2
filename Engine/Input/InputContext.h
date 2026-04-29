#pragma once

#include <string>
#include <unordered_map>

namespace Okari
{
	class InputContext
	{
	public:
		InputContext(const std::string& name);

		void BindKey(int key, const std::string& action);
		void SetBlocking(bool blocking) { m_IsBlocking = blocking; }

		bool IsBlocking() const { return m_IsBlocking; }
		bool HasAction(const std::string& action) const;
		int GetKeyForAction(const std::string& action) const;

		const std::string& GetName() const { return m_Name; }

	private:
		std::string m_Name;
		std::unordered_map<std::string, int> m_ActionBindings;
		bool m_IsBlocking = false;
	};
}