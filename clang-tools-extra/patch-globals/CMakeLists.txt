set(LLVM_LINK_COMPONENTS 
  FrontendOpenMP
  Support
  )

add_clang_executable(patch-globals
  PatchGlobals.cpp
  )

clang_target_link_libraries(patch-globals
  PRIVATE
  clangTooling
  clangBasic
  clangASTMatchers
  )
