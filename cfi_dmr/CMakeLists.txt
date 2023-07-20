add_library(cfi_dmr
    SHARED
        ControlFlowDMR.cc
)

llvm_map_components_to_libnames(llvm_libs passes)
target_link_libraries(cfi_dmr ${llvm_libs})
