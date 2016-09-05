
set(  MAIN_MODULE_VALUES 
NAME_MODULE                            #
MODULE_TYPE                            #"[ INLINE STATIC DYNAMIC ]"
#
EXTERNAL_MODULES
EXTERNAL_MODULES_${DAVA_PLATFORM_CURENT} 
#
SRC_FOLDERS             
ERASE_FOLDERS              
ERASE_FOLDERS_${DAVA_PLATFORM_CURENT}   
#
CPP_FILES                  
HPP_FILES                  
CPP_FILES_${DAVA_PLATFORM_CURENT}       
HPP_FILES_${DAVA_PLATFORM_CURENT}       
#
CPP_FILES_RECURSE            
HPP_FILES_RECURSE            
CPP_FILES_RECURSE_${DAVA_PLATFORM_CURENT} 
HPP_FILES_RECURSE_${DAVA_PLATFORM_CURENT} 
#
ERASE_FILES                
ERASE_FILES_${DAVA_PLATFORM_CURENT}     
ERASE_FILES_NOT_${DAVA_PLATFORM_CURENT} 
#
UNITY_IGNORE_LIST             
UNITY_IGNORE_LIST_${DAVA_PLATFORM_CURENT}
#
CUSTOM_PACK_1
CUSTOM_PACK_1_${DAVA_PLATFORM_CURENT}
#
INCLUDES         
INCLUDES_PRIVATE 
INCLUDES_${DAVA_PLATFORM_CURENT} 
INCLUDES_PRIVATE_${DAVA_PLATFORM_CURENT} 
#
DEFINITIONS                
DEFINITIONS_PRIVATE             
DEFINITIONS_${DAVA_PLATFORM_CURENT}     
DEFINITIONS_PRIVATE_${DAVA_PLATFORM_CURENT}  
#
STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}           
STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}_RELEASE   
STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}_DEBUG     
#
DYNAMIC_LIBRARIES_${DAVA_PLATFORM_CURENT}           
#
FIND_SYSTEM_LIBRARY                   
FIND_SYSTEM_LIBRARY_${DAVA_PLATFORM_CURENT}
#
FIND_PACKAGE
FIND_PACKAGE_${DAVA_PLATFORM_CURENT}
#
DEPLOY_TO_BIN
DEPLOY_TO_BIN_${DAVA_PLATFORM_CURENT}
BINARY_WIN32_DIR_RELEASE
BINARY_WIN32_DIR_DEBUG
BINARY_WIN32_DIR_RELWITHDEB
BINARY_WIN64_DIR_RELEASE
BINARY_WIN64_DIR_DEBUG
BINARY_WIN64_DIR_RELWITHDEB
#
EXCLUDE_FROM_ALL
#
)
#
macro ( load_external_modules EXTERNAL_MODULES )
    foreach( FOLDER_MODULE ${EXTERNAL_MODULES} )
        file( GLOB FIND_CMAKELIST "${FOLDER_MODULE}/CMakeLists.txt" )
        if( FIND_CMAKELIST )
            get_filename_component ( FOLDER_NAME ${FOLDER_MODULE} NAME )
            add_subdirectory ( ${FOLDER_MODULE}  ${FOLDER_NAME} )       
        else()
            file( GLOB LIST ${FOLDER_MODULE} )
            foreach( ITEM ${LIST} )
                if( IS_DIRECTORY ${ITEM} )
                    load_external_modules( ${ITEM} )
                endif()
            endforeach()
        endif()
    endforeach()
endmacro()
#
macro( reset_MAIN_MODULE_VALUES )
    foreach( VALUE ${MAIN_MODULE_VALUES} TARGET_MODULES_LIST 
                                         QT_DEPLOY_LIST_VALUE 
                                         QT_LINKAGE_LIST 
                                         QT_LINKAGE_LIST_VALUE 
                                         DEPENDENT_LIST )
        set( ${VALUE} )
        set_property( GLOBAL PROPERTY ${VALUE} ${${VALUE}} )
    endforeach()
endmacro()
#
macro( setup_main_module )
    if( NOT MODULE_TYPE )
        set( MODULE_TYPE INLINE )
    endif()

    set( ORIGINAL_NAME_MODULE ${NAME_MODULE} )

    if( NOT ( ${MODULE_TYPE} STREQUAL "INLINE" ) )
        get_property( MODULES_ARRAY GLOBAL PROPERTY MODULES_ARRAY )
        list (FIND MODULES_ARRAY ${NAME_MODULE} _index)
        if ( JOIN_PROJECT_NAME OR ${_index} GREATER -1)
            set( NAME_MODULE ${NAME_MODULE}_${PROJECT_NAME} )
        endif() 
        list( APPEND MODULES_ARRAY ${NAME_MODULE} )
        set_property( GLOBAL PROPERTY MODULES_ARRAY "${MODULES_ARRAY}" )

        project ( ${NAME_MODULE} )
        include ( CMake-common )
    endif()

    set( INIT )

    get_filename_component (DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    get_property( DAVA_COMPONENTS GLOBAL PROPERTY  DAVA_COMPONENTS )

    list (FIND DAVA_COMPONENTS "ALL" _index)
    if ( ${_index} GREATER -1 AND NOT EXCLUDE_FROM_ALL)
        set( INIT true )
    else()
        if( ORIGINAL_NAME_MODULE )
            list (FIND DAVA_COMPONENTS ${ORIGINAL_NAME_MODULE} _index)
            if ( ${_index} GREATER -1)
                set( INIT true )
            endif()
        else()
            set( INIT true )
        endif()
    endif()


    if ( INIT )
        if( IOS AND ${MODULE_TYPE} STREQUAL "DYNAMIC" )
            set( MODULE_TYPE "STATIC" )
        endif()

        #"APPLE VALUES"
        if( APPLE )
            foreach( VALUE CPP_FILES 
                           CPP_FILES_RECURSE 
                           ERASE_FILES 
                           ERASE_FILES_NOT
                           DEFINITIONS 
                           DEFINITIONS_PRIVATE 
                           INCLUDES
                           INCLUDES_PRIVATE 
                           UNITY_IGNORE_LIST
                           CUSTOM_PACK_1 )
                if( ${VALUE}_APPLE)
                    list( APPEND ${VALUE}_${DAVA_PLATFORM_CURENT} ${${VALUE}_APPLE} )  
                endif()
            endforeach()
        endif()

        if( ANDROID )
            list( APPEND STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT} ${DYNAMIC_LIBRARIES_${DAVA_PLATFORM_CURENT}}  )
        endif()
        
        if( WIN )
            list( APPEND STATIC_LIBRARIES_WIN         ${STATIC_LIBRARIES_WIN${DAVA_PROJECT_BIT}} )
            list( APPEND STATIC_LIBRARIES_WIN_RELEASE ${STATIC_LIBRARIES_WIN${DAVA_PROJECT_BIT}_RELEASE} ) 
            list( APPEND STATIC_LIBRARIES_WIN_DEBUG   ${STATIC_LIBRARIES_WIN${DAVA_PROJECT_BIT}_DEBUG} )
            list( APPEND DYNAMIC_LIBRARIES_WIN        ${DYNAMIC_LIBRARIES_WIN${DAVA_PROJECT_BIT}} )
        endif()      
       
        #"FIND LIBRARY"
        foreach( NAME ${FIND_SYSTEM_LIBRARY} ${FIND_SYSTEM_LIBRARY_${DAVA_PLATFORM_CURENT}} )
            FIND_LIBRARY( ${NAME}_LIBRARY  ${NAME} )

            if( ${NAME}_LIBRARY )
                list ( APPEND STATIC_LIBRARIES_SYSTEM_${DAVA_PLATFORM_CURENT} ${${NAME}_LIBRARY} )
            else()
                if ( NOT NOT_TARGET_EXECUTABLE )
                    find_package( ${NAME} )
                    list ( APPEND STATIC_LIBRARIES_SYSTEM_${DAVA_PLATFORM_CURENT} ${${NAME}_LIBRARY} )
                endif()
            endif()
        endforeach()

        #"FIND PACKAGE"
        foreach( NAME ${FIND_PACKAGE} ${FIND_PACKAGE${DAVA_PLATFORM_CURENT}} )
            find_package( ${NAME} )
            if (PACKAGE_${NAME}_INCLUDES)
                foreach( PACKAGE_INCLUDE ${PACKAGE_${NAME}_INCLUDES} )
                    include_directories(${${PACKAGE_INCLUDE}})
                endforeach()
            endif()
        endforeach()

        #"ERASE FILES"
        foreach( PLATFORM  ${DAVA_PLATFORM_LIST} )
            if( NOT ${PLATFORM} AND ERASE_FILES_NOT_${PLATFORM} )
                list( APPEND ERASE_FILES ${ERASE_FILES_NOT_${PLATFORM}} ) 
            endif()
        endforeach()
        if( ERASE_FILES_NOT_${DAVA_PLATFORM_CURENT} AND ERASE_FILES )
             list(REMOVE_ITEM ERASE_FILES ${ERASE_FILES_NOT_${DAVA_PLATFORM_CURENT}} )
        endif()

        set( ALL_SRC )
        set( ALL_SRC_HEADER_FILE_ONLY )
        set( EXTERNAL_MODULES ${EXTERNAL_MODULES} ${EXTERNAL_MODULES_${DAVA_PLATFORM_CURENT}} ) 
        
        if( SRC_FOLDERS OR EXTERNAL_MODULES )

            foreach( VALUE ${MAIN_MODULE_VALUES} )
                set( ${VALUE}_DIR_NAME ${${VALUE}} )
                set( ${VALUE})
            endforeach()

            if( EXTERNAL_MODULES_DIR_NAME )
                load_external_modules( "${EXTERNAL_MODULES_DIR_NAME}" )
            endif()
            
            if( SRC_FOLDERS_DIR_NAME )
                define_source( SOURCE        ${SRC_FOLDERS_DIR_NAME}
                               IGNORE_ITEMS  ${ERASE_FOLDERS_DIR_NAME} ${ERASE_FOLDERS_${DAVA_PLATFORM_CURENT}_DIR_NAME} )
                                         
                set_project_files_properties( "${PROJECT_SOURCE_FILES_CPP}" )
                list( APPEND ALL_SRC  ${PROJECT_SOURCE_FILES} )
                list( APPEND ALL_SRC_HEADER_FILE_ONLY  ${PROJECT_HEADER_FILE_ONLY} )
            endif()
 
            foreach( VALUE ${MAIN_MODULE_VALUES} )
                set(  ${VALUE} ${${VALUE}_DIR_NAME} )
            endforeach()

        endif()

        define_source( SOURCE         ${CPP_FILES} ${CPP_FILES_${DAVA_PLATFORM_CURENT}}
                                      ${HPP_FILES} ${HPP_FILES_${DAVA_PLATFORM_CURENT}}
                       SOURCE_RECURSE ${CPP_FILES_RECURSE} ${CPP_FILES_RECURSE_${DAVA_PLATFORM_CURENT}}
                                      ${HPP_FILES_RECURSE} ${HPP_FILES_RECURSE_${DAVA_PLATFORM_CURENT}}
                       IGNORE_ITEMS   ${ERASE_FILES} ${ERASE_FILES_${DAVA_PLATFORM_CURENT}}
                     )


        list( APPEND ALL_SRC  ${PROJECT_SOURCE_FILES} )
        list( APPEND ALL_SRC_HEADER_FILE_ONLY  ${PROJECT_HEADER_FILE_ONLY} )

        set_project_files_properties( "${ALL_SRC}" )

        #"SAVE PROPERTY"
        save_property( PROPERTY_LIST 
                DEFINITIONS
                DEFINITIONS_${DAVA_PLATFORM_CURENT}
                DYNAMIC_LIBRARIES_${DAVA_PLATFORM_CURENT}          
                STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT} 
                STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}_RELEASE 
                STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}_DEBUG 
                STATIC_LIBRARIES_SYSTEM_${DAVA_PLATFORM_CURENT}
                DEPLOY_TO_BIN
                DEPLOY_TO_BIN_${DAVA_PLATFORM_CURENT}
                INCLUDES
                INCLUDES_${DAVA_PLATFORM_CURENT}
                INCLUDES_PRIVATE
                INCLUDES_PRIVATE_${DAVA_PLATFORM_CURENT}                
                BINARY_WIN32_DIR_RELEASE
                BINARY_WIN32_DIR_DEBUG
                BINARY_WIN32_DIR_RELWITHDEB
                BINARY_WIN64_DIR_RELEASE
                BINARY_WIN64_DIR_DEBUG
                BINARY_WIN64_DIR_RELWITHDEB
                )

        load_property( PROPERTY_LIST 
                DEFINITIONS
                DEFINITIONS_${DAVA_PLATFORM_CURENT}
                STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT} 
                STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}_RELEASE 
                STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}_DEBUG 
                STATIC_LIBRARIES_SYSTEM_${DAVA_PLATFORM_CURENT}
                INCLUDES
                INCLUDES_${DAVA_PLATFORM_CURENT}
                INCLUDES_PRIVATE
                INCLUDES_PRIVATE_${DAVA_PLATFORM_CURENT}                  
                )


        #"DEFINITIONS"
        if( DEFINITIONS )
            add_definitions( ${DEFINITIONS} )
        endif()
        if( DEFINITIONS_${DAVA_PLATFORM_CURENT} )
            add_definitions( ${DEFINITIONS_${DAVA_PLATFORM_CURENT}} )
        endif()

        #"INCLUDES_DIR"
        load_property( PROPERTY_LIST INCLUDES )
        if( INCLUDES )
            include_directories( "${INCLUDES}" )  
        endif()
        if( INCLUDES_${DAVA_PLATFORM_CURENT} )
            include_directories( "${INCLUDES_${DAVA_PLATFORM_CURENT}}" )  
        endif()

        if( ${MODULE_TYPE} STREQUAL "INLINE" )
            set (${DIR_NAME}_PROJECT_SOURCE_FILES_CPP ${PROJECT_SOURCE_FILES_CPP} PARENT_SCOPE)
            set (${DIR_NAME}_PROJECT_SOURCE_FILES_HPP ${PROJECT_SOURCE_FILES_HPP} PARENT_SCOPE)
        else()
            project( ${NAME_MODULE} )
            
            generated_unity_sources( ALL_SRC  IGNORE_LIST ${UNITY_IGNORE_LIST}
                                              IGNORE_LIST_${DAVA_PLATFORM_CURENT} ${UNITY_IGNORE_LIST_${DAVA_PLATFORM_CURENT}}
                                              CUSTOM_PACK_1 ${CUSTOM_PACK_1} ${CUSTOM_PACK_1_${DAVA_PLATFORM_CURENT}}) 
                               
            if( ${MODULE_TYPE} STREQUAL "STATIC" )
                add_library( ${NAME_MODULE} STATIC  ${ALL_SRC} ${ALL_SRC_HEADER_FILE_ONLY} )
                append_property( TARGET_MODULES_LIST ${NAME_MODULE} )  
            elseif( ${MODULE_TYPE} STREQUAL "DYNAMIC" )
                add_library( ${NAME_MODULE} SHARED  ${ALL_SRC} ${ALL_SRC_HEADER_FILE_ONLY} )
                load_property( PROPERTY_LIST TARGET_MODULES_LIST )
                append_property( TARGET_MODULES_LIST ${NAME_MODULE} )            
                add_definitions( -DDAVA_MODULE_EXPORTS )                

                if( WIN32 AND NOT DEPLOY )
                    set( BINARY_WIN32_DIR_RELEASE    "${CMAKE_CURRENT_BINARY_DIR}/Release" )
                    set( BINARY_WIN32_DIR_DEBUG      "${CMAKE_CURRENT_BINARY_DIR}/Debug" )
                    set( BINARY_WIN32_DIR_RELWITHDEB "${CMAKE_CURRENT_BINARY_DIR}/RelWithDebinfo" )
                    set( BINARY_WIN64_DIR_RELEASE    "${CMAKE_CURRENT_BINARY_DIR}/Release" )
                    set( BINARY_WIN64_DIR_DEBUG      "${CMAKE_CURRENT_BINARY_DIR}/Debug" )
                    set( BINARY_WIN64_DIR_RELWITHDEB "${CMAKE_CURRENT_BINARY_DIR}/RelWithDebinfo" )
                    save_property( PROPERTY_LIST BINARY_WIN32_DIR_RELEASE 
                                                 BINARY_WIN32_DIR_DEBUG
                                                 BINARY_WIN32_DIR_RELWITHDEB
                                                 BINARY_WIN64_DIR_RELEASE 
                                                 BINARY_WIN64_DIR_DEBUG
                                                 BINARY_WIN64_DIR_RELWITHDEB )
                endif()

            endif()

            file_tree_check( "${CMAKE_CURRENT_LIST_DIR}" )

            if( TARGET_FILE_TREE_FOUND )
                add_dependencies(  ${NAME_MODULE} FILE_TREE_${NAME_MODULE} )
            endif()

            if( DEFINITIONS_PRIVATE )
                add_definitions( ${DEFINITIONS_PRIVATE} )
            endif()

            if( DEFINITIONS_PRIVATE_${DAVA_PLATFORM_CURENT} )
                add_definitions( ${DEFINITIONS_PRIVATE_${DAVA_PLATFORM_CURENT}} )
            endif()

            if( INCLUDES_PRIVATE )
                include_directories( ${INCLUDES_PRIVATE} ) 
            endif() 

            if( INCLUDES_PRIVATE_${DAVA_PLATFORM_CURENT} )
                include_directories( ${INCLUDES_PRIVATE_${DAVA_PLATFORM_CURENT}} ) 
            endif() 


            if( WIN32 )
                grab_libs(LIST_SHARED_LIBRARIES_DEBUG   "${STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}_DEBUG}"   EXCLUDE_LIBS ADDITIONAL_DEBUG_LIBS)
                grab_libs(LIST_SHARED_LIBRARIES_RELEASE "${STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}_RELEASE}" EXCLUDE_LIBS ADDITIONAL_RELEASE_LIBS)
                set( STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}_DEBUG   ${LIST_SHARED_LIBRARIES_DEBUG} )
                set( STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}_RELEASE ${LIST_SHARED_LIBRARIES_RELEASE} )
            endif()

            if( LINK_THIRD_PARTY )                 
                MERGE_STATIC_LIBRARIES( ${NAME_MODULE} ALL "${STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}}" )
                MERGE_STATIC_LIBRARIES( ${PROJECT_NAME} DEBUG "${STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}_DEBUG}" )
                MERGE_STATIC_LIBRARIES( ${PROJECT_NAME} RELEASE "${STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}_RELEASE}" )
            endif()

            target_link_libraries  ( ${NAME_MODULE}  ${STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}}
                                                     ${STATIC_LIBRARIES_SYSTEM_${DAVA_PLATFORM_CURENT}} )  

            foreach ( FILE ${STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}_DEBUG} )
                target_link_libraries  ( ${NAME_MODULE} debug ${FILE} )
            endforeach ()

            foreach ( FILE ${STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}_RELEASE} )
                target_link_libraries  ( ${NAME_MODULE} optimized ${FILE} )
            endforeach ()

            if (QT5_FOUND)
                link_with_qt5(${PROJECT_NAME})
            endif()

            if( MACOS AND COVERAGE AND NOT DAVA_MEGASOLUTION )
                add_definitions( -DTEST_COVERAGE )
                add_definitions( -DDAVA_FOLDERS="${DAVA_FOLDERS}" )
                add_definitions( -DDAVA_UNITY_FOLDER="${CMAKE_BINARY_DIR}/unity_pack" )
            endif()

            reset_property( STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT} )
            reset_property( STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}_RELEASE )
            reset_property( STATIC_LIBRARIES_${DAVA_PLATFORM_CURENT}_DEBUG )
            reset_property( STATIC_LIBRARIES_SYSTEM_${DAVA_PLATFORM_CURENT} )
            reset_property( INCLUDES_PRIVATE )
            reset_property( INCLUDES_PRIVATE_${DAVA_PLATFORM_CURENT} )

            if ( WINDOWS_UAP )
                set_property(TARGET ${NAME_MODULE} PROPERTY VS_MOBILE_EXTENSIONS_VERSION ${WINDOWS_UAP_MOBILE_EXT_SDK_VERSION} )
            endif()

        endif()

        set_property( GLOBAL PROPERTY MODULES_NAME "${NAME_MODULE}" )

    endif()

endmacro ()



