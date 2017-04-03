# Copyright 2014 Dextra, all rights reserved

set(WIN_AGENT_SRCS
	pganalytics-agent.cpp
	../pganalytics.rc
	)

add_executable(pganalytics-agent WIN32 ${WIN_AGENT_SRCS})
target_link_libraries(pganalytics-agent pganalytics_impl pgautil ${requiredlibs})
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/pganalytics-agent DESTINATION bin)

