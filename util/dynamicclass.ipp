// license:BSD-3-Clause
// copyright-holders:Vas Crabb
#ifndef MAME_LIB_UTIL_DYNAMICCLASS_IPP
#define MAME_LIB_UTIL_DYNAMICCLASS_IPP

#pragma once

#include "dynamicclass.h"


namespace util {

namespace detail {

inline std::ptrdiff_t dynamic_derived_class_base::base_vtable_offset()
{
	return
			reinterpret_cast<std::uint8_t *>(&reinterpret_cast<dynamic_derived_class_base *>(std::uintptr_t(0))->m_base_vtable) -
			reinterpret_cast<std::uint8_t *>(&reinterpret_cast<dynamic_derived_class_base *>(std::uintptr_t(0))->m_type_info);
}


template <typename Base>
inline void dynamic_derived_class_base::restore_base_vptr(
		Base &object)
{
	std::uintptr_t &vptr = *reinterpret_cast<uintptr_t *>(&object);
	std::uintptr_t const typeinfo = reinterpret_cast<uintptr_t const *>(vptr)[-1];
	vptr = *reinterpret_cast<uintptr_t const *>(typeinfo + base_vtable_offset());
	assert(reinterpret_cast<void const *>(vptr));
}


template <class Base, typename Extra>
void MAME_ABI_CXX_MEMBER_CALL dynamic_derived_class_base::destroyer<Base, Extra, std::enable_if_t<std::has_virtual_destructor_v<Base> > >::complete_object_destructor(
		value_type<Base, Extra> &object)
{
	restore_base_vptr(object.base);
	object.~value_type();
}


template <class Base, typename Extra>
void MAME_ABI_CXX_MEMBER_CALL dynamic_derived_class_base::destroyer<Base, Extra, std::enable_if_t<std::has_virtual_destructor_v<Base> > >::deleting_destructor(
		value_type<Base, Extra> *object)
{
	restore_base_vptr(object->base);
	delete object;
}


template <class Base, typename Extra>
void dynamic_derived_class_base::destroyer<Base, Extra, std::enable_if_t<!std::has_virtual_destructor_v<Base> > >::operator()(
		Base *object) const
{
	restore_base_vptr(object->base);
	delete reinterpret_cast<value_type<Base, Extra> *>(object);
}

} // namespace detail


/// \brief Create a dynamic derived class
///
/// Creates a new dynamic derived class.  No base member functions are
/// overridden initially.
/// \param [in] name The unmangled name for the new dynamic derived
///   class.  This will be mangled for use in the generated type info.
template <class Base, typename Extra, std::size_t VirtualCount>
dynamic_derived_class<Base, Extra, VirtualCount>::dynamic_derived_class(
		std::string_view name) :
	detail::dynamic_derived_class_base(name)
{
	m_type_info.base_type = &typeid(Base);
	m_vtable[0] = 0; // offset to top
	m_vtable[1] = uintptr_t(&m_type_info); // type info
	if constexpr (std::has_virtual_destructor_v<Base>)
	{
		if (MAME_ABI_CXX_VTABLE_FNDESC)
		{
			std::copy_n(
					reinterpret_cast<std::uintptr_t const *>(std::uintptr_t(&destroyer<Base, Extra>::complete_object_destructor)),
					MEMBER_FUNCTION_SIZE,
					&m_vtable[2]);
			std::copy_n(
					reinterpret_cast<std::uintptr_t const *>(std::uintptr_t(&destroyer<Base, Extra>::deleting_destructor)),
					MEMBER_FUNCTION_SIZE,
					&m_vtable[2 + MEMBER_FUNCTION_SIZE]);
		}
		else
		{
			m_vtable[2] = std::uintptr_t(&destroyer<Base, Extra>::complete_object_destructor);
			m_vtable[3] = std::uintptr_t(&destroyer<Base, Extra>::deleting_destructor);
		}
		std::fill(
				std::next(m_vtable.begin(), 2 + (2 * MEMBER_FUNCTION_SIZE)),
				m_vtable.end(),
				std::uintptr_t(static_cast<void *>(nullptr)));
	}
	else
	{
		std::fill(
				std::next(m_vtable.begin(), 2),
				m_vtable.end(),
				std::uintptr_t(static_cast<void *>(nullptr)));
	}
}


/// \brief Create a dynamic derived class using a prototype
///
/// Creates a new dynamic derived class using an existing dynamic
/// derived class as a prototype.  Overridden base class member
/// functions are inherited from the current state of the prototype.
/// The new dynamic derived class is not affected by any future
/// modifications to the prototype.  The prototype may be destroyed
/// safely before the new dynamic derived class is destroyed (provided
/// all its instances are destroyed first).
/// \param [in] prototype The dynamic derived class to use as a
///   prototype.
/// \param [in] name The unmangled name for the new dynamic derived
///   class.  This will be mangled for use in the generated type info.
template <class Base, typename Extra, std::size_t VirtualCount>
dynamic_derived_class<Base, Extra, VirtualCount>::dynamic_derived_class(
		dynamic_derived_class const &prototype,
		std::string_view name) :
	detail::dynamic_derived_class_base(name),
	m_vtable(prototype.m_vtable),
	m_overridden(prototype.m_overridden)
{
	m_type_info.base_type = &typeid(Base);
	m_base_vtable = prototype.m_base_vtable;
	m_vtable[1] = uintptr_t(&m_type_info); // type info
}


/// \brief Override a virtual member function
///
/// Replace the virtual table entry for the specified base member
/// function with the supplied function.  This applies to existing
/// instances as well as newly created instances.  Note that if you are
/// using some technique to resolve pointers to virtual member functions
/// in advance, resolved pointers may not reflect the change.
/// \tparam R Return type of member function to override (usually
///   determined automatically).
/// \tparam T Parameter types expected by the member function to
///   to override (usually determined automatically).
/// \param [in] slot A pointer to the base class member function to
///   override.  Must be a pointer to a virtual member function.
/// \param [in] func A pointer to the function to use to override the
///   base class member function.
/// \sa restore_base_member_function
template <class Base, typename Extra, std::size_t VirtualCount>
template <typename R, typename... T>
void dynamic_derived_class<Base, Extra, VirtualCount>::override_member_function(
		R (Base::*slot)(T...),
		R MAME_ABI_CXX_MEMBER_CALL (*func)(type &, T...))
{
	static_assert(sizeof(slot) == sizeof(member_function_pointer_equiv), "Unsupported member function size");
	union pointer_thunk
	{
		R (Base::*ptr)(T...);
		member_function_pointer_equiv equiv;
	};
	pointer_thunk thunk;
	thunk.ptr = slot;
	if (MAME_ABI_CXX_ITANIUM_MFP_TYPE == MAME_ABI_CXX_ITANIUM_MFP_ARM)
	{
		assert(thunk.equiv.adj & 1);
		assert(!(thunk.equiv.adj >> 1));
	}
	else
	{
		assert(thunk.equiv.ptr & 1);
		assert(!thunk.equiv.adj);
		thunk.equiv.ptr -= 1;
	}
	assert(!(thunk.equiv.ptr % sizeof(uintptr_t)));
	assert(!((thunk.equiv.ptr / sizeof(uintptr_t)) % MEMBER_FUNCTION_SIZE));
	std::size_t const index = thunk.equiv.ptr / sizeof(uintptr_t) / MEMBER_FUNCTION_SIZE;
	assert(index < VIRTUAL_MEMBER_FUNCTION_COUNT);
	assert(FIRST_OVERRIDABLE_MEMBER_OFFSET <= index);
	m_overridden[index - FIRST_OVERRIDABLE_MEMBER_OFFSET] = true;
	if (MAME_ABI_CXX_VTABLE_FNDESC)
	{
		std::copy_n(
				reinterpret_cast<std::uintptr_t const *>(uintptr_t(func)),
				MEMBER_FUNCTION_SIZE,
				&m_vtable[2 + (thunk.equiv.ptr / sizeof(uintptr_t))]);
	}
	else
	{
		m_vtable[2 + index] = uintptr_t(func);
	}
}


/// \brief Restore the base implementation of a member function
///
/// If the specified virtual member function of the base class has been
/// overridden, restore the base class implementation.  This applies to
/// existing instances as well as newly created instances.  Note that if
/// you are using some technique to resolve pointers to virtual member
/// functions in advance, resolved pointers may not reflect the change.
/// \tparam R Return type of member function to restore (usually
///   determined automatically).
/// \tparam T Parameter types expected by the member function to
///   to restore (usually determined automatically).
/// \param [in] slot A pointer to the base class member function to
///   restore.  Must be a pointer to a virtual member function.
/// \sa override_member_function
template <class Base, typename Extra, std::size_t VirtualCount>
template <typename R, typename... T>
void dynamic_derived_class<Base, Extra, VirtualCount>::restore_base_member_function(
		R (Base::*slot)(T...))
{
	static_assert(sizeof(slot) == sizeof(member_function_pointer_equiv), "Unsupported member function size");
	union pointer_thunk
	{
		R (Base::*ptr)(T...);
		member_function_pointer_equiv equiv;
	};
	pointer_thunk thunk;
	thunk.ptr = slot;
	if (MAME_ABI_CXX_ITANIUM_MFP_TYPE == MAME_ABI_CXX_ITANIUM_MFP_ARM)
	{
		assert(thunk.equiv.adj & 1);
		assert(!(thunk.equiv.adj >> 1));
	}
	else
	{
		assert(thunk.equiv.ptr & 1);
		assert(!thunk.equiv.adj);
		thunk.equiv.ptr -= 1;
	}
	assert(!(thunk.equiv.ptr % sizeof(uintptr_t)));
	assert(!((thunk.equiv.ptr / sizeof(uintptr_t)) % MEMBER_FUNCTION_SIZE));
	std::size_t const index = thunk.equiv.ptr / sizeof(uintptr_t) / MEMBER_FUNCTION_SIZE;
	assert(index < VIRTUAL_MEMBER_FUNCTION_COUNT);
	assert(FIRST_OVERRIDABLE_MEMBER_OFFSET <= index);
	if (m_overridden[index - FIRST_OVERRIDABLE_MEMBER_OFFSET] && m_base_vtable)
	{
		std::copy_n(
				reinterpret_cast<std::uintptr_t const *>(m_base_vtable) + (thunk.equiv.ptr / sizeof(uintptr_t)),
				MEMBER_FUNCTION_SIZE,
				&m_vtable[2 + (thunk.equiv.ptr / sizeof(uintptr_t))]);
	}
	m_overridden[index - FIRST_OVERRIDABLE_MEMBER_OFFSET] = false;
}


/// \brief Create a new instance
///
/// Creates a new instance of the dynamic derived class constructed with
/// the supplied arguments.
/// \tparam T Constructor argument types (usually determined
///   automatically).
/// \param [out] object Receives an pointer to the object storing the
///   base type and extra data.
/// \param [in] args Constructor arguments for the object to be
///   instantiated.  If the first argument is
///   \c std::piecewise_construct, it must be followed by two tuples
///   containing the arguments to forward to the base class and extra
///   data constructors, respectively.  If the first argument is not
///   \c std::piecewise_construct, all the arguments are forwarded to
///   the base class constructor.
/// \return A unique pointer to the new instance.
template <class Base, typename Extra, std::size_t VirtualCount>
template <typename... T>
typename dynamic_derived_class<Base, Extra, VirtualCount>::pointer dynamic_derived_class<Base, Extra, VirtualCount>::instantiate(
		type *&object,
		T &&... args)
{
	std::unique_ptr<type> result(new type(std::forward<T>(args)...));
	void const *&vptr = *reinterpret_cast<void const **>(&result->base);
	if (!m_base_vtable)
	{
		assert(uintptr_t(result.get()) == uintptr_t(&result->base));
		m_base_vtable = vptr;
		for (std::size_t i = 0; VirtualCount > i; ++i)
		{
			if (!m_overridden[i])
			{
				std::size_t const offset = (i + FIRST_OVERRIDABLE_MEMBER_OFFSET) * MEMBER_FUNCTION_SIZE;
				std::copy_n(
						reinterpret_cast<uintptr_t const *>(vptr) + offset,
						MEMBER_FUNCTION_SIZE,
						&m_vtable[2 + offset]);
			}
		}
	}
	vptr = &m_vtable[2];
	object = result.get();
	return pointer(&result.release()->base);
}

} // namespace util

#endif // MAME_LIB_UTIL_DYNAMICCLASS_IPP
