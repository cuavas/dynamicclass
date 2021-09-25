#include "util/dynamicclass.ipp"

#include <cstdio>


namespace {

class virtual_destructor_base
{
public:
	virtual ~virtual_destructor_base() { printf("calling x(3) from virtual_destructor_base::~virtual_destructor_base: "); x(3); }
	virtual void x(int i) { printf("virtual_destructor_base::x(%d)\n", i); }
	virtual void y(int i) { printf("virtual_destructor_base::y(%d)\n", i); }
};


class non_virtual_destructor_base
{
public:
	virtual ~non_virtual_destructor_base() { printf("calling b(4) from non_virtual_destructor_base::~virtual_destructor_base: "); b(4); }
	virtual int a(int i) const { printf("non_virtual_destructor_base::a(%d)\n", i); return i + 1; }
	virtual int b(int i) { printf("non_virtual_destructor_base::b(%d)\n", i); return i + 2; }
	virtual int c(int i) { printf("non_virtual_destructor_base::c(%d)\n", i); return i + 3; }
};


using simple_extender = util::dynamic_derived_class<virtual_destructor_base, void, 2>;

void MAME_ABI_CXX_MEMBER_CALL simple_override(simple_extender::type &object, int i)
{
	printf("simple_override(%p, %d)\n", &object, i);
}

void simple_extender_test()
{
	printf("Testing simple extender with no extra data\n");

	simple_extender::type *simple;

	printf("Creating extension class test::a\n");
	auto test1 = std::make_unique<simple_extender>("test::a");
	printf("Type info @%p name: %s\n", &test1->type_info(), test1->type_info().name());

	printf("Overriding void x(int) in test::a\n");
	test1->override_member_function(&virtual_destructor_base::x, &simple_override);

	printf("Creating instance i1 of class test::a\n");
	auto i1 = test1->instantiate(simple);
	printf("i1 @%p, extended storage @%p\n", i1.get(), simple);
	printf("typeid(i1) == typeid(virtual_destructor_base): %d\n", typeid(*i1) == typeid(virtual_destructor_base));
	printf("typeid(i1) == test1.type_info(): %d\n", typeid(*i1) == test1->type_info());
	printf("typeid(i1).name(): %s\n", typeid(*i1).name());
	printf("dynamic_cast<virtual_destructor_base *>(i1) @%p\n", dynamic_cast<virtual_destructor_base *>(i1.get()));
	printf("dynamic_cast<void *>(i1) @%p\n", dynamic_cast<void *>(i1.get()));
	printf("i1->x(4): ");
	i1->x(4);
	printf("i1->y(5): ");
	i1->y(5);

	printf("Creating extension class test::b using prototype test::a\n");
	auto test2 = std::make_unique<simple_extender>(*test1, "test::b");
	printf("Type info @%p name: %s\n", &test2->type_info(), test2->type_info().name());
	printf("test::b.type_info() == test::a.type_info(): %d\n", test2->type_info() == test1->type_info());

	printf("Creating instance i2 of class test::b\n");
	auto i2 = test2->instantiate(simple);
	printf("i2 @%p, extended storage @%p\n", i2.get(), simple);
	printf("typeid(i2) == typeid(i1): %d\n", typeid(*i2) == typeid(*i1));
	printf("typeid(i2) == test2.type_info(): %d\n", typeid(*i2) == test2->type_info());
	printf("typeid(i2).name(): %s\n", typeid(*i2).name());
	printf("dynamic_cast<virtual_destructor_base *>(i2) @%p\n", dynamic_cast<virtual_destructor_base *>(i2.get()));
	printf("dynamic_cast<void *>(i2) @%p\n", dynamic_cast<void *>(i2.get()));
	printf("i2->x(6): ");
	i2->x(6);
	printf("i2->y(7): ");
	i2->y(7);

	printf("Overriding void y(int) in test::a\n");
	test1->override_member_function(&virtual_destructor_base::y, &simple_override);
	printf("i1->x(9): ");
	i1->x(9);
	printf("i1->y(8): ");
	i1->y(8);
	printf("i2->x(7): ");
	i2->x(7);
	printf("i2->y(6): ");
	i2->y(6);

	printf("Restoring virtual_destructor_base::x in test::b\n");
	test2->restore_base_member_function(&virtual_destructor_base::x);
	printf("i1->x(4): ");
	i1->x(4);
	printf("i1->y(5): ");
	i1->y(5);
	printf("i2->x(6): ");
	i2->x(6);
	printf("i2->y(7): ");
	i2->y(7);

	printf("Restoring virtual_destructor_base::x in test::a\n");
	test1->restore_base_member_function(&virtual_destructor_base::x);
	printf("Overriding void x(int) in test::b\n");
	test2->override_member_function(&virtual_destructor_base::x, &simple_override);
	printf("i1->x(4): ");
	i1->x(4);
	printf("i1->y(5): ");
	i1->y(5);
	printf("i2->x(6): ");
	i2->x(6);
	printf("i2->y(7): ");
	i2->y(7);

	printf("Destroying i1 and test::a\n");
	i1.reset();
	test1.reset();
	printf("i2->x(6): ");
	i2->x(6);
	printf("i2->y(7): ");
	i2->y(7);

	printf("Destroying i2 and test::b\n");
	i2.reset();
	test2.reset();
}


using extra_data_extender = util::dynamic_derived_class<virtual_destructor_base, int, 2>;

void MAME_ABI_CXX_MEMBER_CALL extra_data_override(extra_data_extender::type &object, int i)
{
	printf("extra_data_override(%p, %d) extra = %d\n", &object, i, object.extra);
}

void extra_data_extender_test()
{
	printf("Testing extender with extra int data\n");

	extra_data_extender::type *extra;

	printf("Creating extension class test1 and overriding y(int)\n");
	auto test1 = std::make_unique<extra_data_extender>("test1");
	printf("Type info @%p name: %s\n", &test1->type_info(), test1->type_info().name());
	test1->override_member_function(&virtual_destructor_base::y, &extra_data_override);

	printf("Creating instance i1 of class test1 with extra data 69\n");
	auto i1 = test1->instantiate(
			extra,
			std::piecewise_construct,
			std::forward_as_tuple(),
			std::forward_as_tuple(69));
	printf("i1 @%p, extended storage @%p\n", i1.get(), extra);
	printf("i1->x(9): ");
	i1->x(9);
	printf("i1->y(8): ");
	i1->y(8);

	printf("Assigning extra data = 35\n");
	extra->extra = 35;
	printf("i1->x(7): ");
	i1->x(7);
	printf("i1->y(6): ");
	i1->y(6);

	printf("Creating instance i2 of class test1 with extra data 88\n");
	auto i2 = test1->instantiate(
			extra,
			std::piecewise_construct,
			std::forward_as_tuple(),
			std::forward_as_tuple(88));
	printf("i2 @%p, extended storage @%p\n", i2.get(), extra);
	printf("i1->x(1): ");
	i1->x(1);
	printf("i1->y(2): ");
	i1->y(2);
	printf("i2->x(3): ");
	i2->x(3);
	printf("i2->y(4): ");
	i2->y(4);
}


struct metaclass;

using class_referencing_extender = util::dynamic_derived_class<virtual_destructor_base, metaclass, 2>;

struct metaclass
{
	metaclass(std::unique_ptr<class_referencing_extender> &&c_) : c(std::move(c_)) { }
	~metaclass() { printf("metaclass::~metaclass() name: %s\n", c->type_info().name()); }

	std::unique_ptr<class_referencing_extender> c;
};

void MAME_ABI_CXX_MEMBER_CALL class_referencing_override(class_referencing_extender::type &obj, int i)
{
	printf("class_referencing_override(%p, %d) calling base::x(i): ", &obj, i);
	obj.call_base_member_function(&virtual_destructor_base::x, i);
}

void class_referencing_extender_test()
{
	printf("Testing extender for objects that own their own metaclass\n");

	printf("Creating extension class testclass and overriding x(int)\n");
	auto testclass = std::make_unique<class_referencing_extender>("testclass");
	auto &c = *testclass;
	c.override_member_function(&virtual_destructor_base::x, &class_referencing_override);

	printf("Creating instance i1 of class testclass\n");
	class_referencing_extender::type *obj;
	auto i1 = c.instantiate(
			obj,
			std::piecewise_construct,
			std::forward_as_tuple(),
			std::forward_as_tuple(std::move(testclass)));
	printf("i1 @%p, extended storage @%p\n", i1.get(), obj);
	printf("i1->x(9): ");
	i1->x(9);
	printf("i1->y(8): ");
	i1->y(8);
}


using non_virtual_destructor_extender = util::dynamic_derived_class<non_virtual_destructor_base, void, 3>;

int MAME_ABI_CXX_MEMBER_CALL non_virtual_destructor_const_override(non_virtual_destructor_extender::type const &object, int i)
{
	printf("non_virtual_destructor_const_override(%p, %d)\n", &object, i);
	return -i;
}

int MAME_ABI_CXX_MEMBER_CALL non_virtual_destructor_override(non_virtual_destructor_extender::type &object, int i)
{
	printf("non_virtual_destructor_override(%p, %d)\n", &object, i);
	return -i;
}

void non_virtual_destructor_test()
{
	printf("Testing extender for class without virtual destructor\n");

	non_virtual_destructor_extender::type *actual;

	printf("Creating extension class extender and overriding a(int) and b(int)\n");
	non_virtual_destructor_extender extender("extender");
	extender.override_member_function(&non_virtual_destructor_base::a, &non_virtual_destructor_const_override);
	extender.override_member_function(&non_virtual_destructor_base::b, &non_virtual_destructor_override);

	printf("Creating instance i1 of class extender\n");
	auto i1 = extender.instantiate(actual);
	printf("i1 @%p, extended storage @%p\n", i1.get(), actual);
	printf("typeid(i1).name(): %s\n", typeid(*i1).name());
	printf("i1->a(4): ");
	printf("returned %d\n", i1->a(4));
	printf("i1->b(5): ");
	printf("returned %d\n", i1->b(5));
	printf("i1->c(6): ");
	printf("returned %d\n", i1->c(6));
}

} // anonymous namespace


int main(int argc, char *argv[])
{
	simple_extender_test();
	printf("\n");
	extra_data_extender_test();
	printf("\n");
	class_referencing_extender_test();
	printf("\n");
	non_virtual_destructor_test();

	return 0;
}
