
struct CompilationUnit {
  std::vector<const MethodDebugInfo*> methods_;
  size_t debug_line_offset_ = 0;
  uintptr_t low_pc_ = std::numeric_limits<uintptr_t>::max();
  uintptr_t high_pc_ = 0;
};

