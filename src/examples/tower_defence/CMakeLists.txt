add_executable(tower_defence
    main.cpp
    entity_types.cpp entity_types.h
    entities.cpp entities.h
    game_state.cpp game_state.h
    map_data.cpp map_data.h
    game_session.cpp game_session.h
    helper_functions.cpp helper_functions.h
    pch.h)

target_link_libraries(tower_defence
    engine
)

target_precompile_headers(tower_defence PRIVATE pch.h)
