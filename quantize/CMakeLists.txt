find_package(Clang REQUIRED CONFIG)

include_directories(${CLANG_INCLUDE_DIRS})

add_library(quantize
    SHARED
    Quantize.cc
)

llvm_map_components_to_libnames(llvm_libs passes)
target_link_libraries(quantize ${llvm_libs} ${CLANG_libs})
target_link_libraries(quantize "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")
