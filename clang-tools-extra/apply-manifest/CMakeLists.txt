set(LLVM_LINK_COMPONENTS 
  FrontendOpenMP
  Support
  )

add_clang_executable(apply-manifest
  ApplyManifest.cpp
  )

clang_target_link_libraries(apply-manifest
  PRIVATE
  clangTooling
  clangBasic
  clangASTMatchers
  )
