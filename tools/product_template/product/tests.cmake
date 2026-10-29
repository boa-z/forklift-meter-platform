add_test(NAME product-i18n-generated COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_i18n.py --product-root ${HMI_PRODUCT_ROOT} --check)
add_test(NAME dbc-generation COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/protocol/generate_can.py --product-root ${HMI_PRODUCT_ROOT} --check) # [can]
add_test(NAME product-domain COMMAND ${Python3_EXECUTABLE} "${HMI_PRODUCT_ROOT}/tests/test_product.py" $<TARGET_FILE:hmi-protocol-runner>) # [can]
if(HMI_BUILD_UI)
    add_test(NAME product-ui COMMAND hmi-sim --smoke)
endif()
