// Copyright (C) 2015 Gregor Klinke
// All rights reserved.

#include "catch/catch.hpp"

#include "../nodeclass.hpp"
#include "../nodes.hpp"

#include <string>
#include <sstream>


TEST_CASE("Base node creation", "[nodes]")
{
  eyestep::Node nd(eyestep::elementClassDefinition());
  REQUIRE(nd.className() == "element");
}


TEST_CASE("Default node as no gi", "[nodes]")
{
  eyestep::Node nd;
  REQUIRE(nd.gi().empty());
}


TEST_CASE("Set properties", "[nodes]")
{
  eyestep::Node nd;
  nd.setProperty("name", "foo");
  nd.setProperty("size", 42);

  REQUIRE(nd.property<std::string>("name") == "foo");
  REQUIRE(nd.property<int>("size") == 42);
}


TEST_CASE("Unknown properties report as default", "[nodes]")
{
  eyestep::Node nd;

  REQUIRE(nd.property<std::string>("name").empty());
  REQUIRE(!nd.hasProperty("name"));
}


TEST_CASE("Add node", "[nodes]")
{
  eyestep::Grove grove;
  eyestep::Node* nd = grove.makeNode(eyestep::documentClassDefinition());

  nd->addNode("kids", grove.makeEltNode("foo"));
  nd->addNode("kids", grove.makeEltNode("bar"));

  REQUIRE(nd->property<eyestep::Nodes>("kids").size() == 2u);
  REQUIRE(nd->property<eyestep::Nodes>("kids")[0]->gi() == "foo");
  REQUIRE(nd->property<eyestep::Nodes>("kids")[1]->gi() == "bar");
}


TEST_CASE("Parent property", "[nodes]")
{
  eyestep::Grove grove;
  auto* a = grove.setRootNode(eyestep::rootClassDefinition());
  auto* b = grove.makeEltNode("b");

  a->addChildNode(b);

  REQUIRE(b->gi() == "b");

  REQUIRE(b->property<eyestep::Node*>("parent")->className() == "root");
  REQUIRE(b->parent()->className() == "root");
  REQUIRE(b->parent() == a);
}


TEST_CASE("Traverse", "[nodes]")
{
  eyestep::Grove grove;
  auto* nd = grove.makeEltNode("foo");

  nd->setProperty("name", "bar");
  nd->setProperty("size", 42);

  auto* type = grove.makeEltNode("type");
  type->setProperty("const?", true);

  auto* args = grove.makeEltNode("args");
  args->addNode("params", grove.makeEltNode("p1"));
  args->addNode("params", grove.makeEltNode("p2"));
  args->addNode("params", grove.makeEltNode("p3"));

  nd->addChildNode(grove.makeEltNode("title"));
  nd->addChildNode(args);
  nd->addChildNode(type);

  SECTION("Full recursion")
  {
    using ExpectedGiType = std::vector<std::tuple<std::string, int>>;
    ExpectedGiType gis;

    eyestep::nodeTraverse(nd, [&gis](const eyestep::Node* n, int depth) {
      gis.emplace_back(std::make_tuple(n->gi(), depth));
      return eyestep::TraverseRecursion::kRecurse;
    });

    REQUIRE(gis == (ExpectedGiType{{"foo", 0},
                                   {"title", 1},
                                   {"args", 1},
                                   {"type", 1}}));
  }

  SECTION("Only siblings")
  {
    std::vector<std::string> gis;
    eyestep::nodeTraverse(nd, [&gis](const eyestep::Node* nd, int depth) {
      gis.emplace_back(nd->gi());
      return eyestep::TraverseRecursion::kContinue;
    });

    REQUIRE(gis == (std::vector<std::string>{"foo"}));
  }

  SECTION("Mixed recursion/siblings")
  {
    std::vector<std::string> gis;
    eyestep::nodeTraverse(nd, [&gis](const eyestep::Node* nd, int depth) {
      gis.emplace_back(nd->gi());
      if (nd->gi() == "foo") {
        return eyestep::TraverseRecursion::kRecurse;
      }
      else {
        return eyestep::TraverseRecursion::kContinue;
      }
    });

    REQUIRE(gis == (std::vector<std::string>{"foo", "title", "args", "type"}));
  }

  SECTION("breaks")
  {
    std::vector<std::string> gis;
    eyestep::nodeTraverse(nd, [&gis](const eyestep::Node* nd, int depth) {
      gis.emplace_back(nd->gi());
      if (nd->gi() == "foo") {
        return eyestep::TraverseRecursion::kRecurse;
      }
      else if (nd->gi() == "args") {
        return eyestep::TraverseRecursion::kBreak;
      }
      else {
        return eyestep::TraverseRecursion::kContinue;
      }
    });

    REQUIRE(gis == (std::vector<std::string>{"foo", "title", "args"}));
  }
}


TEST_CASE("Serialize", "[nodes]")
{
  eyestep::Grove grove;

  auto* nd = grove.makeEltNode("foo");
  nd->setProperty("name", "bar");
  nd->setProperty("size", 42);

  auto* type = grove.makeEltNode("type");
  type->setProperty("const?", true);

  auto* args = grove.makeEltNode("args");
  args->addNode("params", grove.makeEltNode("p1"));
  args->addNode("params", grove.makeEltNode("p2"));
  args->addNode("params", grove.makeEltNode("p3"));

  nd->addChildNode(grove.makeEltNode("title"));
  nd->addChildNode(args);
  nd->addNode("types", type);

  const std::string exptected_output =
      "<node gi='foo'>\n"
      "  <prop nm='name'>bar</prop>\n"
      "  <prop nm='size'>42</prop>\n"
      "  <prop nm='types'>\n"
      "    <node gi='type'>\n"
      "      <prop nm='const?'>1</prop>\n"
      "    </node>\n"
      "  </prop>\n"
      "  <node gi='title'>\n"
      "  </node>\n"
      "  <node gi='args'>\n"
      "    <prop nm='params'>\n"
      "      <node gi='p1'>\n"
      "      </node>\n"
      "      <node gi='p2'>\n"
      "      </node>\n"
      "      <node gi='p3'>\n"
      "      </node>\n"
      "    </prop>\n"
      "  </node>\n"
      "</node>\n";

  std::stringstream ss;
  serialize(ss, grove.rootNode());
  REQUIRE(ss.str() == exptected_output);
}
