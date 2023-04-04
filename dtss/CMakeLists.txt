add_library(dtss
    SHARED
    DTSS.cc
)

llvm_map_components_to_libnames(llvm_libs passes)
target_link_libraries(dtss ${llvm_libs})
