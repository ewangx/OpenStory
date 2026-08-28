if(NOT DEFINED APP_BUNDLE)
    message(FATAL_ERROR "APP_BUNDLE is required")
endif()

include(BundleUtilities)

file(REMOVE_RECURSE "${APP_BUNDLE}/Contents/Frameworks")
fixup_bundle("${APP_BUNDLE}" "" "")
verify_app("${APP_BUNDLE}")

execute_process(
    COMMAND /usr/bin/codesign --force --deep --sign - "${APP_BUNDLE}"
    RESULT_VARIABLE codesign_result)

if(NOT codesign_result EQUAL 0)
    message(FATAL_ERROR "Ad-hoc signing failed: ${codesign_result}")
endif()
