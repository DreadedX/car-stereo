idf_component_register(SRCS "src/main.cpp" "src/helper.cpp" "src/wav.cpp" "src/storage.cpp" "src/i2s.cpp" "src/bluetooth.cpp" "src/avrcp.cpp" "src/a2dp.cpp"
                    INCLUDE_DIRS "include"
                    EMBED_FILES "assets/connect.wav" "assets/disconnect.wav")
