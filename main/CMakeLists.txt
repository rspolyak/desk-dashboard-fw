idf_component_register(SRCS "main.cpp" REQUIRES app)
# Source flags come after IDF's default language-standard flag.
set_source_files_properties(main.cpp PROPERTIES COMPILE_OPTIONS "-std=c++17")
