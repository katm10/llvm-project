set(LLVM_LINK_COMPONENTS 
  Support
  )

add_clang_executable(extract-trace
  ExtractTrace.cpp
  )

clang_target_link_libraries(extract-trace
  PRIVATE
  clangTooling
  clangBasic
  clangASTMatchers
  )
