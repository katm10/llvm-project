set(LLVM_LINK_COMPONENTS 
  FrontendOpenMP
  Support
  )

add_clang_executable(patch-functions
  PatchFunctions.cpp
  )

clang_target_link_libraries(patch-functions
  PRIVATE
  clangTooling
  clangBasic
  clangASTMatchers
  )
