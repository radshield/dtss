add_library(dtss MODULE
    DTSS.cc
)

set_target_properties(dtss PROPERTIES CXX_STANDARD 17)

target_compile_options(dtss PUBLIC -fno-rtti)

if(APPLE)
    target_link_options(dtss PUBLIC -undefined dynamic_lookup)
endif(APPLE)