# Python Environment Configuration for Raspberry Pi 4B TSN Build
# Ensures stable Python environment for devicetree processing

# Prefer system Python over Miniconda/Anaconda
set(Python3_FIND_VIRTUALENV STANDARD)

# Look for Python in standard system locations first
set(Python3_FIND_STRATEGY LOCATION)

# Explicitly set Python executable paths (in order of preference)
if(NOT DEFINED PYTHON_EXECUTABLE)
    find_program(PYTHON_EXECUTABLE
        NAMES python3.11 python3.10 python3.9 python3.8 python3
        PATHS
            /usr/bin
            /usr/local/bin
            /bin
        NO_DEFAULT_PATH
    )
endif()

# Fallback to system search if not found
if(NOT PYTHON_EXECUTABLE)
    find_program(PYTHON_EXECUTABLE
        NAMES python3.11 python3.10 python3.9 python3.8 python3
    )
endif()

# Verify Python version compatibility
if(PYTHON_EXECUTABLE)
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')"
        OUTPUT_VARIABLE PYTHON_VERSION_CHECK
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    if(PYTHON_VERSION_CHECK VERSION_LESS "3.8")
        message(FATAL_ERROR "Python 3.8 or newer is required. Found: ${PYTHON_VERSION_CHECK}")
    endif()
    
    message(STATUS "Using Python: ${PYTHON_EXECUTABLE} (version ${PYTHON_VERSION_CHECK})")
else()
    message(FATAL_ERROR "Python 3.8+ not found. Please install Python or set PYTHON_EXECUTABLE.")
endif()

# Set CMake Python variables
set(Python3_EXECUTABLE ${PYTHON_EXECUTABLE})
set(PYTHON3_EXECUTABLE ${PYTHON_EXECUTABLE})