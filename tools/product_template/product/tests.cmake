add_test(NAME product-domain COMMAND ${Python3_EXECUTABLE} "${METER_PRODUCT_ROOT}/tests/test_product.py" $<TARGET_FILE:meter-protocol-runner>)
if(METER_BUILD_UI)
    add_test(NAME product-ui COMMAND meter-demo --smoke)
endif()
