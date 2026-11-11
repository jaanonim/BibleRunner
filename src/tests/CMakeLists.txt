# the plugins are loadable Qt modules (not linkable), so the tests link the
# shared module library; every test file is its own executable (and its own
# CTest entry) so failures are reported and selectable per file

# usage: biblerunner_add_test(<name> <source> [link-libraries...])
function(biblerunner_add_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE
            Boost::unit_test_framework ${ARGN})
    # BOOST_TEST_MODULE names the single-TU Boost.Test module and provides
    # main()
    target_compile_definitions(${name} PRIVATE
            BOOST_TEST_DYN_LINK
            "BOOST_TEST_MODULE=${name}")
    # compile option, not include dir: CMake drops the boost include as an
    # implicit dir, but the module scan (clang-scan-deps) still needs it
    target_compile_options(${name} PRIVATE "-isystem${Boost_INCLUDE_DIR}")
    set_target_properties(${name} PROPERTIES CXX_SCAN_FOR_MODULES ON)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

biblerunner_add_test(biblerunner_test_query test_query.cpp biblerunner_shared biblerunner_books)
biblerunner_add_test(biblerunner_test_api test_api.cpp biblerunner_shared)
biblerunner_add_test(biblerunner_test_cache test_cache.cpp biblerunner_shared)
biblerunner_add_test(biblerunner_test_config test_config.cpp biblerunner_shared)
