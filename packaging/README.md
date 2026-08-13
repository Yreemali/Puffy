# Packaging status

The project exposes a CPack entry point and a Linux CI build. Linux packaging can ship the unprivileged application and PipeWire integration. Windows packaging must additionally include a signed virtual audio driver package; macOS packaging must include the signed/notarized Audio Server Plug-in or DriverKit component. Those privileged components are intentionally not fabricated by the portable CMake build.
