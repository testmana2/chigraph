#include <catch.hpp>

#include <chi/Context.hpp>
#include <chi/GraphModule.hpp>
#include <chi/GraphStruct.hpp>
#include <chi/Support/Result.hpp>

using namespace chi;

TEST_CASE("GraphModuleTest", "[module]") {
	Context c;

	auto gMod = c.newGraphModule("test/main");
	REQUIRE(c.modules().size() == 1);
	REQUIRE(c.modules()[0] == gMod);
	REQUIRE(c.moduleByFullName("test/main") == gMod);
	REQUIRE(gMod->fullName() == "test/main");
	REQUIRE(gMod->shortName() == "main");
	REQUIRE(gMod->typeNames().size() == 0);

	auto res = gMod->addDependency("lang");
	REQUIRE(!!res);
	REQUIRE(c.modules().size() == 2);
	REQUIRE(c.moduleByFullName("lang") != nullptr);
	REQUIRE(gMod->dependencies().size() == 1);
	REQUIRE(gMod->dependencies().find("lang") != gMod->dependencies().end());

	REQUIRE(gMod->removeDependency("lang"));
	REQUIRE(c.modules().size() == 2);  // it didn't unload it
	REQUIRE(gMod->dependencies().size() == 0);

	res = gMod->addDependency("notarealmodule");
	REQUIRE(!res);
	REQUIRE(c.modules().size() == 2);
	REQUIRE(gMod->dependencies().size() == 0);

	WHEN("We create a new struct") {
		bool inserted;
		auto str = gMod->getOrCreateStruct("hello", &inserted);
		REQUIRE(inserted);
		REQUIRE(str != nullptr);
		REQUIRE(gMod->structs().size() == 1);
		REQUIRE(gMod->structs()[0].get() == str);
		REQUIRE(gMod->typeNames().size() == 1);
		REQUIRE(gMod->nodeTypeNames().size() == 2);
		REQUIRE(gMod->typeNames()[0] == "hello");
		REQUIRE(gMod->typeFromName("hello") == DataType{});
		REQUIRE((gMod->nodeTypeNames()[0] == "_make_hello" ||
		         gMod->nodeTypeNames()[1] == "_make_hello"));
		REQUIRE((gMod->nodeTypeNames()[0] == "_break_hello" ||
		         gMod->nodeTypeNames()[1] == "_break_hello"));
		REQUIRE(gMod->structFromName("hello") == str);
		REQUIRE(str == gMod->getOrCreateStruct("hello", &inserted));
		REQUIRE_FALSE(inserted);

		auto assertStructRemoved = [&] {
			REQUIRE(gMod->structs().size() == 0);
			REQUIRE(gMod->typeNames().size() == 0);
			REQUIRE(gMod->nodeTypeNames().size() == 0);
			REQUIRE(gMod->structFromName("hello") == nullptr);
		};

		WHEN("We remove that struct") {
			REQUIRE(gMod->removeStruct("hello"));
			REQUIRE_FALSE(gMod->removeStruct("hello"));

			assertStructRemoved();
		}
		WHEN("We remove that struct using the other method") {
			gMod->removeStruct(*str);

			assertStructRemoved();
		}
	}

	WHEN("We create a new function") {
		bool inserted;
		auto func = gMod->getOrCreateFunction("mysexyfunc", {}, {}, {""}, {""}, &inserted);
		REQUIRE(inserted);
		REQUIRE(func != nullptr);
		REQUIRE(gMod->functions().size() == 1);
		REQUIRE(gMod->functions()[0].get() == func);
		REQUIRE(gMod->nodeTypeNames().size() == 1);
		REQUIRE(gMod->nodeTypeNames()[0] == "mysexyfunc");
		REQUIRE(gMod->functionFromName("mysexyfunc") == func);
		REQUIRE(gMod->getOrCreateFunction("mysexyfunc", {}, {}, {""}, {""}, &inserted));
		REQUIRE_FALSE(inserted);

		auto assertFuncRemoved = [&] {
			REQUIRE(gMod->functions().size() == 0);
			REQUIRE(gMod->nodeTypeNames().size() == 0);
			REQUIRE(gMod->functionFromName("mysexyfunc") == nullptr);
		};

		WHEN("We remove that function") {
			REQUIRE(gMod->removeFunction("mysexyfunc"));
			REQUIRE_FALSE(gMod->removeFunction("mysexyfunc"));

			assertFuncRemoved();
		}
		WHEN("We remove that function using the other method") {
			gMod->removeFunction(*func);

			assertFuncRemoved();
		}
	}
}
