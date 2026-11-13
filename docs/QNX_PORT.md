---
title: QNX Neutrino POSIX sockets
description: POSIX socket toolchain for QNX SDP 8.0.0. Not compiled with a licensed SDP, and not verified in CI.
---

# QNX Neutrino (POSIX sockets)

POSIX sockets only. No message passing.

The host library on QNX Neutrino keeps the existing POSIX socket PAL.
Transport calls stay in `include/platform/posix/net_impl.h`. This tree
does not call `MsgSend`, `MsgReceive`, `MsgSendv`, `ChannelCreate`,
`ConnectAttach`, or `name_attach`, and it does not add a transport that
would.

`CMakeLists.txt` selects `include/platform/posix` for a build that is
not FreeRTOS, ThreadX, or Windows. `cmake/toolchains/qnx-sdp.cmake` sets
`CMAKE_SYSTEM_NAME` to `QNX`, so that branch applies. There is no new PAL.

## SDP 8.0.0

The intended SDP is 8.0.0, matching the gateway `--config=x86_64-qnx` /
`x86_64-qnx-sdp_8.0.0-posix`.

This change was not compiled with a licensed SDP. CI does not compile
this library with the QNX SDP, and no check reports a QNX build that
did not run.

## CMake invocation for an SDP user

Source the SDP 8.0.0 environment first. `qnxsdp-env.sh` sets `QNX_HOST`
and `QNX_TARGET`. From this repository:

```bash
cmake -S . -B build/qnx-x86_64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/qnx-sdp.cmake \
  -DBUILD_TESTS=OFF \
  -DBUILD_EXAMPLES=OFF
cmake --build build/qnx-x86_64 --target opensomeip
```

That cmake toolchain invocation was not run here. With `QNX_HOST` or
`QNX_TARGET` unset, configure fails and does not use the host compiler.

The default qcc target is `gcc_ntox86_64` (`CMAKE_SYSTEM_PROCESSOR` is
`x86_64`). Override `QNX_SDP_ARCH` and `QNX_SDP_PROCESSOR` together for
a different Neutrino CPU. Those overrides were not compiled here either.

## Later command in the incubation repo

The later real command, for someone who has the SDP, is in the incubation
repo after the adapter:

```bash
bazel test --config=x86_64-qnx //tests/tc8_conformance/...
```

That command is not a target in this library, and it was not run here.
