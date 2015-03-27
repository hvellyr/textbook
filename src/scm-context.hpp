// Copyright (c) 2015 Gregor Klinke
// All rights reserved.

#pragma once

#include <boost/filesystem.hpp>

#include <memory>
#include <vector>


namespace eyestep {

class Node;
class Sosofo;

class ISchemeContext {
public:
  virtual ~ISchemeContext(){};

  virtual void
  initialize(const std::vector<boost::filesystem::path>& module_paths) = 0;
  virtual bool load_module_file(const boost::filesystem::path& script_file) = 0;
  virtual bool load_script(const boost::filesystem::path& script_file) = 0;

  virtual std::unique_ptr<Sosofo> process_root_node(const Node* root_node) = 0;
};

std::unique_ptr<ISchemeContext> create_scheme_context();

} // ns eyestep
