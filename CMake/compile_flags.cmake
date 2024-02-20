macro(set_compile_flags target)
	target_compile_features(${target} PUBLIC c_std_17 cxx_std_23)
	
	target_compile_definitions(${target} PRIVATE UNICODE)

	if(ENABLE_LTO)
		set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
	endif()

	if(ENABLE_AVX2)
		if(MSVC)
			target_compile_options(${target} PRIVATE /arch:AVX2)
		else()
			target_compile_options(${target} PRIVATE -mavx2)
		endif()
	endif()
endmacro()

macro(mark_third_party target)
	set_compile_flags(${target})
	set_property(TARGET ${target} PROPERTY FOLDER "ThirdParty")
endmacro()