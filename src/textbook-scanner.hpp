// Copyright (c) 2015 Gregor Klinke
// All rights reserved.

#pragma once

#include "scanner.hpp"

#include "program_options/program_options.hpp"

#include <string>
#include <vector>
#include <unordered_set>

namespace eyestep {

class Node;
class Grove;

class TextbookScanner : public IScanner {
  bool _debug;
  std::vector<boost::filesystem::path> _prefix_path;
  std::vector<boost::filesystem::path> _catalog_path;

public:
  TextbookScanner();
  TextbookScanner(const program_options::variables_map& /*args*/);

  std::string scanner_id() const override;
  std::unordered_set<std::string> supported_extensions() const override;
  program_options::options_description program_options() const override;

  Node* scan_file(eyestep::Grove& grove,
                  const boost::filesystem::path& srcfile) override;
};

} // ns eyestep
