#include <cstdint>
#include <emscripten.h>
#include <webgpu/webgpu.h>

static bool WaitFor(WGPUInstance instance, WGPUFuture future) {
    WGPUFutureWaitInfo wait = WGPU_FUTURE_WAIT_INFO_INIT;
    wait.future = future;
    return wgpuInstanceWaitAny(instance, 1, &wait, UINT64_MAX) == WGPUWaitStatus_Success;
}

extern "C" EMSCRIPTEN_KEEPALIVE int drawDawnTriangle(uint32_t width, uint32_t height) {
    const WGPUInstanceFeatureName feature = WGPUInstanceFeatureName_TimedWaitAny;
    WGPUInstanceDescriptor instanceDesc = WGPU_INSTANCE_DESCRIPTOR_INIT;
    instanceDesc.requiredFeatureCount = 1;
    instanceDesc.requiredFeatures = &feature;
    const WGPUInstance instance = wgpuCreateInstance(&instanceDesc);
    if (!instance) return 1;

    WGPUAdapter adapter = nullptr;
    WGPURequestAdapterCallbackInfo adapterCallback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    adapterCallback.mode = WGPUCallbackMode_WaitAnyOnly;
    adapterCallback.userdata1 = &adapter;
    adapterCallback.callback = [](WGPURequestAdapterStatus status, WGPUAdapter value, WGPUStringView, void* out, void*) {
        if (status == WGPURequestAdapterStatus_Success) *static_cast<WGPUAdapter*>(out) = value;
    };
    if (!WaitFor(instance, wgpuInstanceRequestAdapter(instance, nullptr, adapterCallback)) || !adapter) return 2;

    WGPUDevice device = nullptr;
    WGPURequestDeviceCallbackInfo deviceCallback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    deviceCallback.mode = WGPUCallbackMode_WaitAnyOnly;
    deviceCallback.userdata1 = &device;
    deviceCallback.callback = [](WGPURequestDeviceStatus status, WGPUDevice value, WGPUStringView, void* out, void*) {
        if (status == WGPURequestDeviceStatus_Success) *static_cast<WGPUDevice*>(out) = value;
    };
    if (!WaitFor(instance, wgpuAdapterRequestDevice(adapter, nullptr, deviceCallback)) || !device) return 3;

    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector source = WGPU_EMSCRIPTEN_SURFACE_SOURCE_CANVAS_HTML_SELECTOR_INIT;
    source.selector = {"#canvas", WGPU_STRLEN};
    WGPUSurfaceDescriptor surfaceDesc = WGPU_SURFACE_DESCRIPTOR_INIT;
    surfaceDesc.nextInChain = &source.chain;
    const WGPUSurface surface = wgpuInstanceCreateSurface(instance, &surfaceDesc);
    if (!surface) return 4;

    WGPUSurfaceCapabilities caps = WGPU_SURFACE_CAPABILITIES_INIT;
    if (wgpuSurfaceGetCapabilities(surface, adapter, &caps) != WGPUStatus_Success || caps.formatCount == 0) return 5;
    const WGPUTextureFormat format = caps.formats[0];
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    WGPUSurfaceConfiguration config = WGPU_SURFACE_CONFIGURATION_INIT;
    config.device = device;
    config.format = format;
    config.width = width;
    config.height = height;
    wgpuSurfaceConfigure(surface, &config);

    constexpr char shaderSource[] = R"(
        @vertex fn vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
            let vertices = array(vec2f(0.0, 0.6), vec2f(-0.65, -0.5), vec2f(0.65, -0.5));
            return vec4f(vertices[i], 0.0, 1.0);
        }
        @fragment fn fs() -> @location(0) vec4f { return vec4f(0.1, 0.85, 0.75, 1.0); }
    )";
    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code = {shaderSource, sizeof(shaderSource) - 1};
    WGPUShaderModuleDescriptor shaderDesc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    shaderDesc.nextInChain = &wgsl.chain;
    const WGPUShaderModule shader = wgpuDeviceCreateShaderModule(device, &shaderDesc);
    WGPUColorTargetState color = WGPU_COLOR_TARGET_STATE_INIT;
    color.format = format;
    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
    fragment.module = shader;
    fragment.entryPoint = {"fs", WGPU_STRLEN};
    fragment.targetCount = 1;
    fragment.targets = &color;
    WGPURenderPipelineDescriptor pipelineDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pipelineDesc.vertex.module = shader;
    pipelineDesc.vertex.entryPoint = {"vs", WGPU_STRLEN};
    pipelineDesc.fragment = &fragment;
    const WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

    WGPUSurfaceTexture target = WGPU_SURFACE_TEXTURE_INIT;
    wgpuSurfaceGetCurrentTexture(surface, &target);
    if (!target.texture) return 6;
    const WGPUTextureView view = wgpuTextureCreateView(target.texture, nullptr);
    WGPURenderPassColorAttachment attachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    attachment.view = view;
    attachment.loadOp = WGPULoadOp_Clear;
    attachment.storeOp = WGPUStoreOp_Store;
    attachment.clearValue = {0.025, 0.05, 0.08, 1.0};
    WGPURenderPassDescriptor passDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &attachment;
    const WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    const WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    const WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, nullptr);
    const WGPUQueue queue = wgpuDeviceGetQueue(device);
    wgpuQueueSubmit(queue, 1, &commands);

    bool workDone = false;
    WGPUQueueWorkDoneCallbackInfo completed = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
    completed.mode = WGPUCallbackMode_WaitAnyOnly;
    completed.userdata1 = &workDone;
    completed.callback = [](WGPUQueueWorkDoneStatus status, WGPUStringView, void* out, void*) {
        *static_cast<bool*>(out) = status == WGPUQueueWorkDoneStatus_Success;
    };
    if (!WaitFor(instance, wgpuQueueOnSubmittedWorkDone(queue, completed)) || !workDone) return 7;

    wgpuCommandBufferRelease(commands);
    wgpuRenderPassEncoderRelease(pass);
    wgpuCommandEncoderRelease(encoder);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(target.texture);
    wgpuRenderPipelineRelease(pipeline);
    wgpuShaderModuleRelease(shader);
    wgpuQueueRelease(queue);
    wgpuAdapterRelease(adapter);
    return 42;
}
