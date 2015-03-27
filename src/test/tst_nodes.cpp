// Copyright (C) 2015 Gregor Klinke
// All rights reserved.

#include "catch/catch.hpp"

#include "../nodeclass.hpp"
#include "../nodes.hpp"

#include <string>
#include <sstream>
#include <tuple>


TEST_CASE("Base node creation", "[nodes]")
{
  eyestep::Node nd(eyestep::element_class_definition());
  REQUIRE(nd.classname() == "element");
}


TEST_CASE("Default node as no gi", "[nodes]")
{
  eyestep::Node nd;
  REQUIRE(nd.gi().empty());
}


TEST_CASE("Set properties", "[nodes]")
{
  eyestep::Node nd;
  nd.set_property("name", "foo");
  nd.set_property("size", 42);

  REQUIRE(nd.property<std::string>("name") == "foo");
  REQUIRE(nd.property<int>("size") == 42);
}


TEST_CASE("Unknown properties report as default", "[nodes]")
{
  eyestep::Node nd;

  REQUIRE(nd.property<std::string>("name").empty());
  REQUIRE(!nd.has_property("name"));
}


TEST_CASE("Add node", "[nodes]")
{
  eyestep::Grove grove;
  eyestep::Node* nd = grove.make_node(eyestep::document_class_definition());

  nd->add_node("kids", grove.make_elt_node("foo"));
  nd->add_node("kids", grove.make_elt_node("bar"));

  REQUIRE(nd->property<eyestep::Nodes>("kids").size() == 2u);
  REQUIRE(nd->property<eyestep::Nodes>("kids")[0]->gi() == "foo");
  REQUIRE(nd->property<eyestep::Nodes>("kids")[1]->gi() == "bar");
}


TEST_CASE("Parent property", "[nodes]")
{
  eyestep::Grove grove;
  auto* a = grove.set_root_node(eyestep::root_class_definition());
  auto* b = grove.make_elt_node("b");

  a->add_child_node(b);

  REQUIRE(b->gi() == "b");

  REQUIRE(b->property<eyestep::Node*>("parent")->classname() == "root");
  REQUIRE(b->parent()->classname() == "root");
  REQUIRE(b->parent() == a);
}


TEST_CASE("Traverse", "[nodes]")
{
  eyestep::Grove grove;
  auto* nd = grove.make_elt_node("foo");

  nd->set_property("name", "bar");
  nd->set_property("size", 42);

  auto* type = grove.make_elt_node("type");
  type->set_property("const?", true);

  auto* args = grove.make_elt_node("args");
  args->add_node("params", grove.make_elt_node("p1"));
  args->add_node("params", grove.make_elt_node("p2"));
  args->add_node("params", grove.make_elt_node("p3"));

  nd->add_child_node(grove.make_elt_node("title"));
  nd->add_child_node(args);
  nd->add_child_node(type);

  SECTION("Full recursion")
  {
    using GiType = std::tuple<std::string, int>;
    using ExpectedGiType = std::vector<GiType>;
    ExpectedGiType gis;

    eyestep::node_traverse(nd, [&gis](const eyestep::Node* n, int depth) {
      gis.emplace_back(std::make_tuple(n->gi(), depth));
      return eyestep::TraverseRecursion::k_recurse;
    });

    REQUIRE(gis == (ExpectedGiType{GiType{"foo", 0}, GiType{"title", 1},
                                   GiType{"args", 1}, GiType{"type", 1}}));
  }

  SECTION("Only siblings")
  {
    std::vector<std::string> gis;
    eyestep::node_traverse(nd, [&gis](const eyestep::Node* nd, int depth) {
      gis.emplace_back(nd->gi());
      return eyestep::TraverseRecursion::k_continue;
    });

    REQUIRE(gis == (std::vector<std::string>{"foo"}));
  }

  SECTION("Mixed recursion/siblings")
  {
    std::vector<std::string> gis;
    eyestep::node_traverse(nd, [&gis](const eyestep::Node* nd, int depth) {
      gis.emplace_back(nd->gi());
      if (nd->gi() == "foo") {
        return eyestep::TraverseRecursion::k_recurse;
      }
      else {
        return eyestep::TraverseRecursion::k_continue;
      }
    });

    REQUIRE(gis == (std::vector<std::string>{"foo", "title", "args", "type"}));
  }

  SECTION("breaks")
  {
    std::vector<std::string> gis;
    eyestep::node_traverse(nd, [&gis](const eyestep::Node* nd, int depth) {
      gis.emplace_back(nd->gi());
      if (nd->gi() == "foo") {
        return eyestep::TraverseRecursion::k_recurse;
      }
      else if (nd->gi() == "args") {
        return eyestep::TraverseRecursion::k_break;
      }
      else {
        return eyestep::TraverseRecursion::k_continue;
      }
    });

    REQUIRE(gis == (std::vector<std::string>{"foo", "title", "args"}));
  }
}


TEST_CASE("Serialize", "[nodes]")
{
  eyestep::Grove grove;

  auto* nd = grove.make_elt_node("foo");
  nd->set_property("name", "bar");
  nd->set_property("size", 42);

  auto* type = grove.make_elt_node("type");
  type->set_property("const?", true);

  auto* args = grove.make_elt_node("args");
  args->add_node("params", grove.make_elt_node("p1"));
  args->add_node("params", grove.make_elt_node("p2"));
  args->add_node("params", grove.make_elt_node("p3"));

  nd->add_child_node(grove.make_elt_node("title"));
  nd->add_child_node(args);
  nd->add_node("types", type);

  const std::string exptected_output =
    "<element gi='foo'>\n"
    "  <prop nm='name'>bar</prop>\n"
    "  <prop nm='size'>42</prop>\n"
    "  <prop nm='types'>\n"
    "    <element gi='type'>\n"
    "      <prop nm='const?'>1</prop>\n"
    "    </element>\n"
    "  </prop>\n"
    "  <element gi='title'>\n"
    "  </element>\n"
    "  <element gi='args'>\n"
    "    <prop nm='params'>\n"
    "      <element gi='p1'>\n"
    "      </element>\n"
    "      <element gi='p2'>\n"
    "      </element>\n"
    "      <element gi='p3'>\n"
    "      </element>\n"
    "    </prop>\n"
    "  </element>\n"
    "</element>\n";

  std::stringstream ss;
  serialize(ss, grove.root_node());
  REQUIRE(ss.str() == exptected_output);
}
