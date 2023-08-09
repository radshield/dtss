add_library(rad_rating
    SHARED
    RadRating.cc
)

llvm_map_components_to_libnames(llvm_libs passes)
target_link_libraries(rad_rating ${llvm_libs})
