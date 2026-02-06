include_guard(GLOBAL)

include(FetchContent)

set(IBKR_BOOST_VERSION "1.90.0" CACHE STRING "Boost version tag (boost-x.y.z)")

if(NOT DEFINED BOOST_INCLUDE_LIBRARIES)
  set(BOOST_INCLUDE_LIBRARIES
      system;thread;beast;asio;config;property_tree
      CACHE STRING "Boost libraries to build")
endif()

if(NOT DEFINED BOOST_ENABLE_TESTS)
  set(BOOST_ENABLE_TESTS OFF CACHE BOOL "Disable Boost tests")
endif()

if(NOT DEFINED BOOST_ENABLE_EXAMPLES)
  set(BOOST_ENABLE_EXAMPLES OFF CACHE BOOL "Disable Boost examples")
endif()

if(NOT DEFINED FETCHCONTENT_BASE_DIR)
  set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps" CACHE PATH "FetchContent base dir")
endif()

FetchContent_Declare(
  boost
  GIT_REPOSITORY https://github.com/boostorg/boost
  GIT_TAG        boost-${IBKR_BOOST_VERSION}
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
  GIT_SUBMODULES_RECURSE TRUE
  USES_TERMINAL_DOWNLOAD TRUE
)

FetchContent_GetProperties(boost)
if(NOT boost_POPULATED)
  message(STATUS "Git: Fetching boost-${IBKR_BOOST_VERSION}")
endif()

FetchContent_MakeAvailable(boost)

set(_ibkr_boost_targets "")
  foreach(t IN ITEMS
    Boost::system
    Boost::thread
    Boost::beast
    Boost::asio
    Boost::config
    Boost::property_tree
    Boost::headers
  )
  if (TARGET ${t})
    list(APPEND _ibkr_boost_targets ${t})
  endif()
endforeach()

if(NOT _ibkr_boost_targets)
  message(FATAL_ERROR "Boost fetched but no Boost:: targets were created. Check Boost version boost-${IBKR_BOOST_VERSION}.")
endif()

target_link_libraries(ibapi_stack_deps_boost INTERFACE ${_ibkr_boost_targets})
