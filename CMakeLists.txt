cmake_minimum_required (VERSION 3.8)
project ("bin2cpp")

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SOURCES 
	"Source/*.cpp" 
	"Source/*.c" 
	"Headers/*.hpp" 
	"Headers/*.h" 
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
PUBLIC 
    "Headers"
)

if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    add_definitions(-D_DEBUG)
endif()
