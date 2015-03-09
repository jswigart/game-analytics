include_directories(".")

get_property(includes DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)

# Store the includes so parent scripts can reference them if needed
set ( GAMEANALYTICS_INCLUDES ${includes} CACHE INTERNAL "Game Analytics Includes" )

file(GLOB src "*.cpp")

add_library(gameanalytics STATIC ${src} )
