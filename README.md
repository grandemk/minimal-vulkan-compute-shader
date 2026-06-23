# A minimal application with Vulkan compute shaders using DXC and HLSL

A minimal Vulkan application that uses compute shaders compiled with DXC (DirectX Shader Compiler) and HLSL.

I created [a video](https://www.youtube.com/watch?v=KN9nHo9kvZs) explaining Vulkan compute shaders.

## Requirements

Vulkan code on shader in Vulkan SDK needs installed.

### Shader Compiler

DXC (DirectX Shader Compiler) must be installed in `PATH`.

You can install DXC through:
- **Linux**: `sudo apt install libshaderc-dev glslang-tools` (includes dxc)
- **Windows**: Download from [GitHub Releases](https://github.com/microsoft/DirectXShaderCompiler/releases)
- **macOS**: `brew install shaderc` (includes dxc)

Or build from source: https://github.com/microsoft/DirectXShaderCompiler

## Build

Create build folder:

```sh
$ mkdir bin
$ cd bin
```

Configure:

```sh
$ cmake ..
```

Compile & run:

```sh
$ make
$ ./main
```

## Usage

This program initializes a Vulkan compute shader that performs element-wise multiplication (squaring) on an array of integers.

### Expected Output

```
Device Name    : <Your GPU Name>
Vulkan Version : <Vulkan Version>
Compute Queue Family Index: <Index>
Memory Type Index: <Index>
Memory Heap Size : <Size> GB

INPUT:  0 1 2 3 4 5 6 7 8 9
OUTPUT: 0 1 4 9 16 25 36 49 64 81
```

### Platform Compatibility

I tested this program primarily on Linux with DXC and HLSL shaders. DXC can produce SPIR-V binaries that are compatible with Vulkan across platforms.

## Technical Details

### Shader Implementation

The compute shader (`shaders/shader.hlsl`) is written in HLSL and compiled to SPIR-V using DXC with the following invocation:

```bash
dxc -spirv -T cs_6_0 -E main shader.hlsl -Fo shader.hlsl.spv
```

- `-spirv`: Output SPIR-V instead of DXIL
- `-T cs_6_0`: Compile for compute shader model 6.0
- `-E main`: Entry point function is `main`
- `-Fo`: Output file

The shader binds two storage buffers:
- Binding 0: Input buffer (read-only)
- Binding 1: Output buffer (read-write)

### Vulkan Pipeline

- Workgroup size: 1x1x1
- Dispatch count: 10 (for 10 input elements)
- Compute shader squares each element in parallel
