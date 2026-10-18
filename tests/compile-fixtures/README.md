# Compile-only external sensor fixture

This synthetic MPU6050 binding and address compile the non-empty external IMU driver branch against Zephyr's actual six-axis sensor API. The product sensor/model, electrical pins and address remain unselected. Existing reference-board I2C pin definitions are not verified wiring. This is not a physical sensor emulator or an accuracy test. **Do not flash this fixture.**

Use a separate generated directory with `tools/build-board.py --overlay tests/compile-fixtures/mpu6050.overlay --out .work/zephyr-external-compile` plus the SDK/toolchain/protocol arguments documented in the architecture guide. No flash or probe command is provided.
