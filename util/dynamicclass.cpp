// license:BSD-3-Clause
// copyright-holders:Vas Crabb
#include "dynamicclass.ipp"

#include <locale>
#include <sstream>


namespace util {

namespace detail {

dynamic_derived_class_base::dynamic_derived_class_base(std::string_view name) :
	m_base_vtable(nullptr)
{
	assert(!reinterpret_cast<void *>(uintptr_t(static_cast<void (*)()>(nullptr))));
	assert(!reinterpret_cast<void (*)()>(uintptr_t(static_cast<void *>(nullptr))));

	class base { };
	class derived : base { };

	m_type_info.vptr = *reinterpret_cast<void const *const *>(&typeid(derived));

	std::locale const &clocale(std::locale::classic());
	assert(!name.empty());
	assert(('_' == name[0]) || std::isalpha(name[0], clocale));
	assert(std::find_if_not(name.begin(), name.end(), [&clocale] (char c) { return ('_' == c) || std::isalnum(c, clocale); }) == name.end());

	std::ostringstream str;
	str.imbue(clocale);
	str << name.length() << name;
	m_name = str.str();

	m_type_info.name = m_name.c_str();
}

} // namespace detail

} // namespace util
