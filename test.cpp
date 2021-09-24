#include "util/dynamicclass.ipp"

#include <cstdio>


class foo
{
public:
	virtual ~foo() { }
	virtual void x(int) { printf("foo::x\n"); }
	virtual void y(int) { printf("foo::y\n"); }
};


using foo_extender = util::dynamic_derived_class<foo, int, 2>;


int main(int argc, char *argv[])
{
	foo a;
	foo_extender x("test");
	x.override_member_function(
			&foo::x,
			static_cast<void (*)(foo_extender::type &, int)>([] (foo_extender::type &f, int x) { std::printf("%d %d\n", f.extra, x); }));

	foo_extender::type *actual;
	auto i1 = x.instantiate(
			actual,
			std::piecewise_construct,
			std::forward_as_tuple(),
			std::forward_as_tuple(69));
	auto i2 = x.instantiate(
			actual,
			std::piecewise_construct,
			std::forward_as_tuple(),
			std::forward_as_tuple(35));

	i1->x(0);
	i1->y(1);
	i2->x(3);
	i2->y(4);

	x.restore_base_member_function(&foo::x);
	i1->x(0);
	i1->y(1);
	i2->x(3);
	i2->y(4);

	return 0;
}
