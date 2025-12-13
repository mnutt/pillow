add_subdirectory(simple)
add_subdirectory(clientbench)
add_subdirectory(fileserver)

if(NOT PILLOW_NO_SSL)
    add_subdirectory(simplessl)
endif()

# Note: declarative and fileserver examples are not included as they
# depend on Qt Script/Declarative which are deprecated in Qt6
