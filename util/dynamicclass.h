// license:BSD-3-Clause
// copyright-holders:Vas Crabb
/// \file
/// \brief Dynamic derived class creator
///
/// Allows on-the-fly creation of derived classes that override selected
/// virtual member functions of the base class.  The overridden base
/// class member functions can be changed even while instances exist.
/// This is particularly useful in conjunction with code generation.
///
/// Only implemented for Itanium C++ ABI in configurations where it is
/// possible to define a non-member function that uses the same calling
/// convention as a non-static member function, and subject to a number
/// of limitations on classes that can be used as base classes.
///
/// It goes without saying that this is not something you are supposed
/// to do.  This depends heavily on implementation details, and careful
/// use by the developer.  Use it at your own risk.
#ifndef MAME_LIB_UTIL_DYNAMICCLASS_H
#define MAME_LIB_UTIL_DYNAMICCLASS_H

#pragma once

#include "abi.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>


namespace util {

namespace detail {

class dynamic_derived_class_base
{
protected:
	static constexpr std::size_t MEMBER_FUNCTION_SIZE = MAME_ABI_CXX_VTABLE_FNDESC ? MAME_ABI_FNDESC_SIZE : 1;

	struct itanium_si_slass_type_info_equiv
	{
		void const *vptr;
		char const *name;
		std::type_info const *base_type;
	};

	struct member_function_pointer_equiv
	{
		uintptr_t ptr;
		ptrdiff_t adj;
	};

	template <class Base, typename Extra>
	class value_type
	{
	private:
		template <typename... T, typename... U, std::size_t... N, std::size_t... O>
		value_type(
				std::tuple<T...> &a,
				std::tuple<U...> &b,
				std::integer_sequence<std::size_t, N...>,
				std::integer_sequence<std::size_t, O...>) :
			base(std::forward<T>(std::get<N>(a))...),
			extra(std::forward<U>(std::get<O>(b))...)
		{
		}
	public:
		template <typename... T>
		value_type(T &&... a) :
			base(std::forward<T>(a)...)
		{
		}

		template <typename... T, typename... U>
		value_type(std::piecewise_construct_t, std::tuple<T...> a, std::tuple<U...> b) :
			value_type(a, b, std::make_integer_sequence<std::size_t, sizeof...(T)>(), std::make_integer_sequence<std::size_t, sizeof...(U)>())
		{
		}

		Base base;
		Extra extra;
	};

	template <class Base>
	class value_type<Base, void>
	{
	public:
		template <typename... T> value_type(T &&... args) : base(std::forward<T>(args)...) { }

		Base base;
	};

	template <class Base, typename Extra, typename Enable = void>
	struct destroyer;

	template <class Base, typename Extra>
	struct destroyer<Base, Extra, std::enable_if_t<std::has_virtual_destructor_v<Base> > >
	{
		using pointer_type = std::unique_ptr<Base>;

		static void MAME_ABI_CXX_MEMBER_CALL complete_object_destructor(value_type<Base, Extra> &object);
		static void MAME_ABI_CXX_MEMBER_CALL deleting_destructor(value_type<Base, Extra> *object);
	};

	template <class Base, typename Extra>
	struct destroyer<Base, Extra, std::enable_if_t<!std::has_virtual_destructor_v<Base> > >
	{
		using pointer_type = std::unique_ptr<Base, destroyer>;

		void operator()(Base *object) const;
	};

	dynamic_derived_class_base(std::string_view name);

	itanium_si_slass_type_info_equiv m_type_info;
	std::string m_name;
	void const *m_base_vtable;

private:
	static std::ptrdiff_t base_vtable_offset();

	template <typename Base>
	static void restore_base_vptr(Base &object);
};

} // namespace detail



/// \brief Dynamic derived class
///
/// Allows dynamically creating classes derived from a supplied base
/// class and overriding virtual member functions.  A class must meet a
/// number of requirements to be suitable for use as a base class.  Most
/// of these requirements cannot be checked automatically.  It is the
/// developer's responsibility to ensure the requirements are met.
///
/// The dynamic derived class object must not be destroyed until after
/// all instances of the class have been destroyed.
///
/// When destroying an instance of the dynamic derived class, the base
/// class vtable is restored before the extra data destructor is called.
/// This allows the extra data type to hold a smart pointer to the
/// dynamic derived class so it can be cleaned up automatically when all
/// instances are destroyed.  However, it means you must keep in mind
/// that the base class implementation of all virtual member functions
/// will be restored before the extra data destructor is called.
/// \tparam Base Base class for the dynamic derived class.  Must meet
///   the following requirements:
///   * Must be a concrete class with at least one public constructor
///     and a public destructor.
///   * Must have at least one virtual member function.
///   * Must not have any direct or indirect virtual base classes.
///   * Must have no secondary virtual tables.  This means the class and
///     all of its direct and indirect base classes may only inherit
///     virtual member functions from one base class at most.
///   * If the class has a virtual destructor, it must be declared
///     declared before any other virtual member functions in the least
///     derived base class containing virtual member functions.
/// \tparam Extra Extra data type, or \c void if not required.  Must be
///   a concrete type with at least one public constructor and a public
///   destructor.
/// \tparam VirtualCount The total number of virtual member functions of
///   the base class, excluding the virtual destructor if present.  This
///   must be correct, and cannot be checked automatically.  It is the
///   developer's responsibility to ensure the value is correct.  This
///   potentially includes additional entries for overridden member
///   functions with covariant return types and implicitly-declared
///   assignment operators.
template <class Base, typename Extra, std::size_t VirtualCount>
class dynamic_derived_class : private detail::dynamic_derived_class_base
{
private:
	static_assert(sizeof(std::uintptr_t) == sizeof(std::ptrdiff_t), "Pointer and pointer difference must be the same size");
	static_assert(sizeof(void *) == sizeof(void (*)()), "Code and data pointers must be the same size");
	static_assert(std::is_polymorphic_v<Base>, "Base class must be polymorphic");

	static constexpr std::size_t FIRST_OVERRIDABLE_MEMBER_OFFSET = std::has_virtual_destructor_v<Base> ? 2 : 0;
	static constexpr std::size_t VIRTUAL_MEMBER_FUNCTION_COUNT = VirtualCount + FIRST_OVERRIDABLE_MEMBER_OFFSET;
	static constexpr std::size_t VTABLE_SIZE = 2 + (VIRTUAL_MEMBER_FUNCTION_COUNT * MEMBER_FUNCTION_SIZE);

	std::array<std::uintptr_t, VTABLE_SIZE> m_vtable;
	std::bitset<VirtualCount> m_overridden;

public:
	/// \brief Type used to store base class and extra data
	///
	/// Has a member \c base of the base class type, and a member
	/// \c extra of the extra data type if it is not \c void.
	using type = value_type<Base, Extra>;

	/// \brief Smart pointer to instance
	///
	/// A unique pointer type suitable for taking ownership of an
	/// instance of the dynamic derived class.
	using pointer = typename destroyer<Base, Extra>::pointer_type;

	dynamic_derived_class(dynamic_derived_class const &) = delete;
	dynamic_derived_class &operator=(dynamic_derived_class const &) = delete;

	dynamic_derived_class(std::string_view name);
	dynamic_derived_class(dynamic_derived_class const &prototype, std::string_view name);

	/// \brief Get type info for dynamic derived class
	///
	/// Gets a reference to the type info for the dynamic derived class.
	/// The reference remains valid and unique as long as the dynamic
	/// derived class is not destroyed.
	/// \return A reference to an object equivalent to \c std::type_info.
	std::type_info const &type_info() const
	{
		return *reinterpret_cast<std::type_info const *>(&m_type_info);
	}

	template <typename R, typename... T>
	void override_member_function(R (Base::*slot)(T...), R MAME_ABI_CXX_MEMBER_CALL (*func)(type &, T...));

	template <typename R, typename... T>
	void restore_base_member_function(R (Base::*slot)(T...));

	template <typename... T>
	pointer instantiate(type *&object, T &&... args);
};

} // namespace util

#endif // MAME_LIB_UTIL_DYNAMICCLASS_H
