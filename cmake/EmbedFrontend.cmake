function(embed_frontend frontend_dir output_file)
    # Only public assets are embedded. Tests, package metadata, and dependencies
    # remain outside the HTTP asset table.
    file(GLOB frontend_entries CONFIGURE_DEPENDS
         RELATIVE "${frontend_dir}"
         "${frontend_dir}/*.html"
         "${frontend_dir}/*.css"
         "${frontend_dir}/*.js"
         "${frontend_dir}/vendor/*.js")
    file(GLOB_RECURSE frontend_modules CONFIGURE_DEPENDS
         RELATIVE "${frontend_dir}"
         "${frontend_dir}/assets/*.js"
         "${frontend_dir}/assets/*.css")
    set(frontend_files ${frontend_entries} ${frontend_modules})
    list(SORT frontend_files)
    list(LENGTH frontend_files FRONTEND_ASSET_COUNT)
    set(FRONTEND_ASSETS "")

    foreach(asset_path IN LISTS frontend_files)
        set(asset_file "${frontend_dir}/${asset_path}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${asset_file}")
        file(READ "${asset_file}" asset_content)
        # C++ raw strings make the source readable and avoid escaping JS/CSS.
        string(FIND "${asset_content}" ")RUMPEL_ASSET\"" delimiter_position)
        if(NOT delimiter_position EQUAL -1)
            message(FATAL_ERROR "Frontend asset contains the raw-string delimiter: ${asset_path}")
        endif()

        if(asset_path MATCHES "\\.js$")
            set(content_type "text/javascript; charset=utf-8")
        elseif(asset_path MATCHES "\\.css$")
            set(content_type "text/css; charset=utf-8")
        else()
            set(content_type "text/html; charset=utf-8")
        endif()

        string(APPEND FRONTEND_ASSETS
               "    {\"/${asset_path}\", \"${content_type}\", R\"RUMPEL_ASSET(${asset_content})RUMPEL_ASSET\"},\n")
    endforeach()

    configure_file("${frontend_dir}/../src/frontend/assets.hpp.in" "${output_file}" @ONLY)
endfunction()
