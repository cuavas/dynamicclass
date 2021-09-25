// license:BSD-3-Clause
// copyright-holders:Vas Crabb
#ifndef MAME_LIB_UTIL_DYNAMICCLASS_IPP
#define MAME_LIB_UTIL_DYNAMICCLASS_IPP

#pragma once

#include "dynamicclass.h"


namespace util {

namespace detail {

/// \brief Get offset to saved base virtual table pointer from type info
///
/// Gets the offset to the saved base class virtual table pointer from
/// the location the dynamic derived class virtual table entry used for
/// recovery points to.
/// \return Offset from the type info to saved base class virtual table
///   pointer in bytes.
inline std::ptrdiff_t dynamic_derived_class_base::base_vtable_offset()
{
	return
			reinterpret_cast<std::uint8_t *>(&reinterpret_cast<dynamic_derived_class_base *>(std::uintptr_t(0))->m_base_vtable) -
			reinterpret_cast<std::uint8_t *>(&reinterpret_cast<dynamic_derived_class_base *>(std::uintptr_t(0))->m_type_info);
}


/// \brief Get base class virtual table pointer
///
/// Gets the base class virtual pointer for an instance of a dynamic
/// derived class.
/// \tparam Base The base class type (usually determined automatically).
/// \param [in] object Base class member of dynamic derived class
///   instance.
/// \return Pointer to first virtual member function entry in base class
///   virtual table.
template <class Base>
inline std::uintptr_t const *dynamic_derived_class_base::get_base_vptr(
		Base const &object)
{
	auto const vptr = *reinterpret_cast<uintptr_t const *>(&object);
	auto const recovery = reinterpret_cast<uintptr_t const *>(vptr)[VTABLE_BASE_RECOVERY_INDEX];
	return *reinterpret_cast<uintptr_t const *const *>(recovery + base_vtable_offset());
}


/// \brief Restore base class virtual table pointer
///
/// Restores the base class virtual pointer in an instance of a dynamic
/// derived class.  Must be called before calling the base class
/// destructor.
/// \tparam Base The base class type (usually determined automatically).
/// \param [in,out] object Base class member of dynamic derived class
///   instance.
template <class Base>
inline void dynamic_derived_class_base::restore_base_vptr(
		Base &object)
{
	auto &vptr = *reinterpret_cast<uintptr_t *>(&object);
	auto const recovery = reinterpret_cast<uintptr_t const *>(vptr)[VTABLE_BASE_RECOVERY_INDEX];
	vptr = *reinterpret_cast<uintptr_t const *>(recovery + base_vtable_offset());
	assert(reinterpret_cast<void const *>(vptr));
}


/// \brief Resolve pointer to base class member function
///
/// Given an instance and pointer to a base class member function, gets
/// the adjusted \c this pointer and conventional function pointer.
/// \tparam Base The base class type (usually determined automatically).
/// \tparam R Return type of member function (usually determined
/// automatically).
/// \tparam T Parameter types expected by the member function (usually
///   determined automatically).
/// \param [in] object Base class member of dynamic derived class
///   instance.
/// \param [in] func Pointer to member function of base class.
/// \return A \c std::pair containing the conventional function pointer
///   and adjusted \c this pointer.
template <typename Base, typename R, typename... T>
inline std::pair<R MAME_ABI_CXX_MEMBER_CALL (*)(void *, T...), void *> dynamic_derived_class_base::resolve_base_member_function(
		Base &object,
		R (Base::*func)(T...))
{
	member_function_pointer_pun_t<decltype(func)> thunk;
	thunk.ptr = func;
	if (thunk.equiv.is_virtual())
	{
		assert(!thunk.equiv.this_pointer_offset());
		auto const vptr = reinterpret_cast<std::uint8_t const *>(get_base_vptr(object));
		auto const entryptr = reinterpret_cast<std::uintptr_t const *>(vptr + thunk.equiv.virtual_table_entry_offset());
		return std::make_pair(
				MAME_ABI_CXX_VTABLE_FNDESC
					? reinterpret_cast<R MAME_ABI_CXX_MEMBER_CALL (*)(void *, T...)>(uintptr_t(entryptr))
					: reinterpret_cast<R MAME_ABI_CXX_MEMBER_CALL (*)(void *, T...)>(*entryptr),
				&object);
	}
	else
	{
		return std::make_pair(
				reinterpret_cast<R MAME_ABI_CXX_MEMBER_CALL (*)(void *, T...)>(thunk.equiv.function_pointer()),
				reinterpret_cast<std::uint8_t *>(&object) + thunk.equiv.this_pointer_offset());
	}
}


/// \brief Complete object destructor for dynamic derived class
///
/// Restores the base class virtual table pointer, calls the extra data
/// and base class destructors, but does not free the memory occupied by
/// the object.  Used when the base class type has a virtual destructor
/// to allow deleting instances of a dynamic derived class through
/// pointers to the base class type.
///
/// Only used for the Itanium C++ ABI.
/// \param [in] object Reference to the object to destroy.
template <class Base, typename Extra>
void MAME_ABI_CXX_MEMBER_CALL dynamic_derived_class_base::destroyer<Base, Extra, std::enable_if_t<std::has_virtual_destructor_v<Base> > >::complete_object_destructor(
		value_type<Base, Extra> &object)
{
	restore_base_vptr(object.base);
	object.~value_type();
}


/// \brief Deleting destructor for dynamic derived class
///
/// Restores the base class virtual table pointer, calls the extra data
/// and base class destructors, and frees the memory occupied by the
/// object.  Used when the base class type has a virtual destructor to
/// allow deleting instances of a dynamic derived class through pointers
/// to the base class type.
///
/// Only used for the Itanium C++ ABI.
/// \param [in] object Pointer to the object to destroy.
template <class Base, typename Extra>
void MAME_ABI_CXX_MEMBER_CALL dynamic_derived_class_base::destroyer<Base, Extra, std::enable_if_t<std::has_virtual_destructor_v<Base> > >::deleting_destructor(
		value_type<Base, Extra> *object)
{
	restore_base_vptr(object->base);
	delete object;
}


/// \brief Deleter for dynamic derived classes
///
/// Restores the base class virtual table pointer, calls the extra data
/// and base class destructors, and frees the memory occupied by the
/// object.  Used to delete instances of a dynamic derived class when
/// the base class type does not have a virtual destructor.
/// \param [in] object Pointer to the object to destroy.
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
					&m_vtable[VTABLE_PREFIX_ENTRIES]);
			std::copy_n(
					reinterpret_cast<std::uintptr_t const *>(std::uintptr_t(&destroyer<Base, Extra>::deleting_destructor)),
					MEMBER_FUNCTION_SIZE,
					&m_vtable[VTABLE_PREFIX_ENTRIES + MEMBER_FUNCTION_SIZE]);
		}
		else
		{
			m_vtable[VTABLE_PREFIX_ENTRIES] = std::uintptr_t(&destroyer<Base, Extra>::complete_object_destructor);
			m_vtable[VTABLE_PREFIX_ENTRIES + 1] = std::uintptr_t(&destroyer<Base, Extra>::deleting_destructor);
		}
	}
	std::fill(
			std::next(m_vtable.begin(), VTABLE_PREFIX_ENTRIES + (VTABLE_DESTRUCTOR_ENTRIES * MEMBER_FUNCTION_SIZE)),
			m_vtable.end(),
			std::uintptr_t(static_cast<void *>(nullptr)));
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
///   override (usually determined automatically).
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
	member_function_pointer_pun_t<decltype(slot)> thunk;
	thunk.ptr = slot;
	override_member_function(thunk.equiv, uintptr_t(func));
}

template <class Base, typename Extra, std::size_t VirtualCount>
template <typename R, typename... T>
void dynamic_derived_class<Base, Extra, VirtualCount>::override_member_function(
		R (Base::*slot)(T...) const,
		R MAME_ABI_CXX_MEMBER_CALL (*func)(type const &, T...))
{
	member_function_pointer_pun_t<decltype(slot)> thunk;
	thunk.ptr = slot;
	override_member_function(thunk.equiv, uintptr_t(func));
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
	member_function_pointer_pun_t<decltype(slot)>  thunk;
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
				&m_vtable[VTABLE_PREFIX_ENTRIES + (thunk.equiv.ptr / sizeof(uintptr_t))]);
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
	auto &vptr = *reinterpret_cast<uintptr_t const **>(&result->base);
	if (!m_base_vtable)
	{
		assert(uintptr_t(result.get()) == uintptr_t(&result->base));
		m_base_vtable = vptr;
		for (std::size_t i = 0; VirtualCount > i; ++i)
		{
			if (!m_overridden[i])
			{
				std::size_t const offset = (i + FIRST_OVERRIDABLE_MEMBER_OFFSET) * MEMBER_FUNCTION_SIZE;
				std::copy_n(vptr + offset, MEMBER_FUNCTION_SIZE, &m_vtable[VTABLE_PREFIX_ENTRIES + offset]);
			}
		}
	}
	vptr = &m_vtable[VTABLE_PREFIX_ENTRIES];
	object = result.get();
	return pointer(&result.release()->base);
}


/// \brief Replace member function in virtual table
///
/// Does the actual work involved in replacing a virtual table entry to
/// override a virtual member function of the base class, avoiding
/// duplication between overloads.
/// \param [in] slot Internal representation of pointer to a virtual
///   member function of the base class.  May be modified.
/// \param [in] func A pointer to the function to use to override the
///   base class member function reinterpreted as an unsigned integer of
///   equivalent size.
template <class Base, typename Extra, std::size_t VirtualCount>
inline void dynamic_derived_class<Base, Extra, VirtualCount>::override_member_function(
		itanium_member_function_pointer_equiv &slot,
		std::uintptr_t func)
{
	assert(slot.is_virtual());
	assert(!slot.this_pointer_offset());
	if (MAME_ABI_CXX_ITANIUM_MFP_TYPE == MAME_ABI_CXX_ITANIUM_MFP_STANDARD)
		slot.ptr -= 1;
	assert(!(slot.ptr % sizeof(uintptr_t)));
	assert(!((slot.ptr / sizeof(uintptr_t)) % MEMBER_FUNCTION_SIZE));
	std::size_t const index = slot.ptr / sizeof(uintptr_t) / MEMBER_FUNCTION_SIZE;
	assert(index < VIRTUAL_MEMBER_FUNCTION_COUNT);
	assert(FIRST_OVERRIDABLE_MEMBER_OFFSET <= index);
	m_overridden[index - FIRST_OVERRIDABLE_MEMBER_OFFSET] = true;
	if (MAME_ABI_CXX_VTABLE_FNDESC)
	{
		std::copy_n(
				reinterpret_cast<std::uintptr_t const *>(func),
				MEMBER_FUNCTION_SIZE,
				&m_vtable[VTABLE_PREFIX_ENTRIES + (slot.ptr / sizeof(uintptr_t))]);
	}
	else
	{
		m_vtable[VTABLE_PREFIX_ENTRIES + index] = func;
	}
}

} // namespace util

#endif // MAME_LIB_UTIL_DYNAMICCLASS_IPP
