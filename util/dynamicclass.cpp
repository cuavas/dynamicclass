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

	std::locale const &clocale(std::locale::classic());
	assert(!name.empty());
	assert(('_' == name[0]) || std::isalpha(name[0], clocale));
	assert(std::find_if_not(name.begin(), name.end(), [&clocale] (char c) { return (':' == c) || ('_' == c) || std::isalnum(c, clocale); }) == name.end());

	std::ostringstream str;
	str.imbue(clocale);

	class base { };
	class derived : base { };

	m_type_info.vptr = *reinterpret_cast<void const *const *>(&typeid(derived));

	std::string_view::size_type found = name.find(':');
	bool const qualified = std::string::npos != found;
	if (qualified)
		str << 'N';
	while (std::string_view::npos != found)
	{
		assert((found + 2) < name.length());
		assert(name[found + 1] == ':');
		assert(('_' == name[found + 2]) || std::isalpha(name[found + 2], clocale));

		str << found;
		str.write(&name[0], found);
		name.remove_prefix(found + 2);
		found = name.find(':');
	}
	str << name.length() << name;
	if (qualified)
		str << 'E';
	m_name = std::move(str).str();

	m_type_info.name = m_name.c_str();
}

} // namespace detail

} // namespace util
