# ZRenderGraph
Personal customized render graph targeting on different gfx APIs

# Usage
...

# Knowledge
...

## Architecture

### Denpendency Graph

> A high level representation and controller of render passes and resources.

#### Structure in *FrostBite*

##### Setup Phase

General Purpose
1. Define render / compute passes
2. Define **inputs** and **outputs** resources for each pass
3. Code flow is similar to immediate mode rendering

Details
1. Render passes must declare all resources usage
    - Read
    - Write
    - Create
2. External permanent resources are **imported** to Frame Graph
    - History buffer for TAA
    - Backbuffer
    - etc.

Example for creation
```C++
RenderPass::RenderPasss(FrameGraphBuilder& builder)
{
    FrameGraphTextureDesc desc;
    desc.width = 1280;
    desc.height = 720;
    desc.format = RenderFormat_D32_FLOAT;
    desc.initialState = FrameGraphTextureDesc::Clear;
    m_renderTarget = builder.createTexure(desc);
}
```

Example for Read/Write
```C++
RenderPass::RenderPasss(
    FrameGraphBuilder& builder
    FrameGraphResource input, 
    FrameGraphMutableResource renderTarget
    )
{
    // Declare resource denpendencies
    m_input = builder.read(input, readFlags);
    m_renderTarget = builder.write(renderTarget, writeFlags);
}
```


### Resource System

> A general module respond for resource allocation and memory aliasing.