# Cross toolchain configuration for using clang on linux hosts to
# target Vali.
#
# Usage:
# cmake -G Ninja
#    -DCMAKE_TOOLCHAIN_FILE=/path/to/this/file
#    -DHOST_ARCH=[aarch64|arm64|armv7|arm|i686|x86|x86_64|x64]
#
# VALICC:
#    The path to the toolchain root.
# VALI_ARCH:
#    The architecture to build for.
# VALI_SDK_PATH:
#    The path to the root of the SDK.
#

# Sanitize expected environmental variables
if(NOT DEFINED ENV{VALICC})
    message(FATAL_ERROR "Please set the VALICC environmental variable to the path of the Vali Crosscompiler.")
endif()

if(NOT DEFINED ENV{VALI_ARCH})
    message(STATUS "VALI_ARCH environmental variable was not set, defauling to amd64.")
    set(ENV{VALI_ARCH} amd64)
endif()

if(NOT DEFINED ENV{VALI_SDK_PATH})
    message(FATAL_ERROR "Please set the VALI_SDK_PATH environmental variable to the path of the Vali SDK.")
endif()

# Update the module list to include the cmake path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/Modules")

# Setup environment stuff for cmake configuration
set(CMAKE_SYSTEM_NAME valicc)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_C_COMPILER "$ENV{VALICC}/bin/clang" CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "$ENV{VALICC}/bin/clang++" CACHE FILEPATH "")
set(CMAKE_AR "$ENV{VALICC}/bin/llvm-ar" CACHE FILEPATH "")
set(CMAKE_RANLIB "$ENV{VALICC}/bin/llvm-ranlib" CACHE FILEPATH "")
set(VERBOSE 1)

# We need to preserve any flags that were passed in by the user. However, we
# can't append to CMAKE_C_FLAGS and friends directly, because toolchain files
# will be re-invoked on each reconfigure and therefore need to be idempotent.
# The assignments to the _INIT cache variables don't use FORCE, so they'll
# only be populated on the initial configure, and their values won't change
# afterward.
if(DEFINED ENV{LIBCXX_BOOTSTRAP})
    string(APPEND CMAKE_CXX_FLAGS_INIT " -nostdlib++")
endif()

if(DEFINED ENV{VALI_RUNTIME_PATH})
    string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " -L$ENV{VALI_RUNTIME_PATH}/lib ")
    string(APPEND CMAKE_MODULE_LINKER_FLAGS_INIT " -L$ENV{VALI_RUNTIME_PATH}/lib ")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_INIT " -L$ENV{VALI_RUNTIME_PATH}/lib ")
    include_directories(BEFORE "$ENV{VALI_RUNTIME_PATH}/include/c++/v1")
endif()

##################################################
# Setup LLVM custom options
##################################################

# Must have a local host llvm-tblgen in path
set(LLVM_TABLEGEN "$ENV{VALICC}/bin/llvm-tblgen" CACHE FILEPATH "")
set(LLVM_CONFIG_PATH "$ENV{VALICC}/bin/llvm-config" CACHE FILEPATH "")

