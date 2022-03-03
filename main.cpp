#include <MetalLibraryLoader.hpp>
#include <numeric>
#include <vector>

inline void run_example(MTL::Device* device, MetalLibraryLoader& libs, size_t size) {
    auto in_buf = std::vector<float>(size);
    std::iota(in_buf.begin(), in_buf.end(), 0);   // buf == 0, 1, 2, ..., size-1
    auto byte_size = size * sizeof(float);
    auto check_result = [&](const float* res) {
        for (int i = 0; i < size; ++i) {
            if (std::abs(res[i] - std::sqrt((float) i)) < 0.01) continue;
            std::cerr << "Mismatch pos: " << i << ", got: " << res[i] << ", but expected: " << std::sqrt(i) << std::endl;
        }
    };

    auto fun = libs.get_function("Shaders/base.metal", "sqrtf");
    auto input = device->newBuffer(in_buf.data(), byte_size, MTL::ResourceCPUCacheModeDefaultCache);
    auto output = device->newBuffer(byte_size, MTL::ResourceCPUCacheModeDefaultCache);
    {
        auto queue = device->newCommandQueue();
        NS::Error* error = nullptr;
        auto pipeline = device->newComputePipelineState(fun, &error);
        if (error) { report_error(error); }
        auto sub_group_size = pipeline->threadExecutionWidth();
        auto command_buffer = queue->commandBuffer();
        {
            auto encoder = command_buffer->computeCommandEncoder();
            encoder->setComputePipelineState(pipeline);
            encoder->setBuffer(input, 0, 0);
            encoder->setBuffer(output, 0, 1);
            encoder->dispatchThreads(MTL::Size(size, 1, 1), MTL::Size(sub_group_size, 1, 1));
            encoder->endEncoding();
            encoder->release();
        }
        command_buffer->commit();
        command_buffer->waitUntilCompleted();
        command_buffer->release();
        //std::cout << "Pipeline: " << pipeline->description()->cString(NS::StringEncoding::ASCIIStringEncoding) << std::endl;
        pipeline->release();
        queue->release();
    }
    auto out = (float*) output->contents();
    check_result(out);
    input->release();
    output->release();
}


int main() {
    auto device = MTL::CreateSystemDefaultDevice();
    std::cout << "Running on: " << device->name()->cString(NS::StringEncoding::ASCIIStringEncoding) << std::endl;
    auto libs = MetalLibraryLoader(device);
    for (auto i = 1; i < 10000000; i += 1000) {
        run_example(device, libs, i);
        std::cout << i << std::endl;
    }
    device->release();
    return 0;
}
