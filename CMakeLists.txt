add_llvm_loadable_module(clang-ifso clang-ifso.cpp PLUGIN_TOOL clang)

if(LLVM_ENABLE_PLUGINS AND (WIN32 OR CYGWIN))
  target_link_libraries(clang-ifso PRIVATE
    clangAST
    clangBasic
    clangFrontend
    LLVMSupport
    )
endif()
