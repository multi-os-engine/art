#include <cassert>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <string.h>

std::map<std::string, std::set<std::string>> ReadDeps() {
  // Check for valid characters.
  std::string cmd = "git grep -E '^#include \".*\"$'";
  FILE* in = popen(cmd.c_str(), "re");
  if (in == NULL) {
    std::cerr << "Failed to run git grep: " << strerror(errno) << "\n";
    exit(EXIT_FAILURE);
  }
  std::map<std::string, std::set<std::string>> map;
  struct Line {
    Line() : str(nullptr) { }
    ~Line() { if (str != nullptr) free(str); }
    char* str;
  };
  Line line;
  size_t len;
  while (getline(&line.str, &len, in) != -1) {
    const char* colon = strchr(line.str, ':');
    if (colon == nullptr) {
      std::cerr << "No colon on line: " << line.str << "\n";
      exit(EXIT_FAILURE);
    }
    assert(*colon == ':');
    if (strncmp(colon + 1, "#include \"", strlen("#include \"")) != 0) {
      std::cerr << "Missing #include after colon on line: " << line.str << "\n";
      exit(EXIT_FAILURE);
    }
    const char* raw_included = colon + 1 + strlen("#include \"");
    const size_t len_included = strlen(raw_included);
    assert(len_included >= 2);
    std::string included(raw_included, len_included - 2);
    std::string src(line.str, colon - line.str);
    auto slash_pos = src.rfind('/');
    if (slash_pos != std::string::npos) {
      src = src.substr(slash_pos + 1u);
    }
    map[src].insert(included);
    // std::cout << src << " -> " << included << "\n";
  }
  fclose(in);
  return map;
}

class Dumper {
 public:
  Dumper(std::ostream& os,
         const char* indent,
         const std::map<std::string, std::set<std::string>>* deps)
      : os_(os), indent_(indent), deps_(deps), seen_() { }

  void Dump(const std::string& current, size_t depth) {
    for (size_t i = 0; i != depth; ++i) {
      os_ << "  ";
    }
    os_ << current;
    if (seen_.count(current) != 0) {
      os_ << " SEEN\n";
    } else {
      os_ << "\n";
      seen_.insert(current);
      auto it = deps_->find(current);
      if (it != deps_->end()) {
        for (const std::string& dependent : it->second) {
          Dump(dependent, depth + 1u);
        }
      }
    }
  }

 private:
  std::ostream& os_;
  const char* indent_;
  const std::map<std::string, std::set<std::string>>* const deps_;
  std::set<std::string> seen_;
};

int main(int argc, const char* const* argv) {
  if (argc > 2 || (argc == 2 && argv[1][0] == '-')) {
    std::cerr << "Usage:\n"
        "  deps <filename>\n"
        "    Displays the dependency tree of <filename>.\n"
        "  deps\n"
        "    Displays all dependencies.\n";
    exit(argc == 2 ? EXIT_SUCCESS : EXIT_FAILURE);
  }
  auto map = ReadDeps();
  if (argc == 2) {
    Dumper dumper(std::cout, "  ", &map);
    dumper.Dump(argv[1], 0u);
    std::cout << std::flush;
  } else {
    for (auto&& entry : map) {
      for (auto&& value : entry.second) {
        std::cout << entry.first << " -> " << value << "\n";
      }
    }
  }
}

