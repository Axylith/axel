#include "renderer.h"
#include <cstdio>

Renderer create_renderer(VulkanDevice& vkdev, GPU& gpu) {
    Renderer r{};

    //Command Pool -- allocates command buffers from this family
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = gpu.graphics_queue_family;

    vkCreateCommandPool(vkdev.device, &pool_info, nullptr, &r.command_pool);

    VkCommandBufferAllocateInfo alloc_info;
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = r.command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    vkAllocateCommandBuffers(vkdev.device, &alloc_info, &r.command_buffer);

    //Sync
    VkSemaphoreCreateInfo sem_info{};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO_INTEL;
    
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vkCreateSemaphore(vkdev.device, &sem_info, nullptr, &r.image_available);
    vkCreateSemaphore(vkdev.device, &sem_info, nullptr, &r.render_complete);
    vkCreateFence(vkdev.device, &fence_info, nullptr, &r.in_flight);

    printf("[vulkan] Renderer created (command pool + sync objects)\n");
    return r;
}


void render_frame(Renderer& r, VulkanDevice& vkdev, Swapchain& sc) {
    // 1. Wait for previous frame to finish
    vkWaitForFences(vkdev.device, 1, &r.in_flight, VK_TRUE, UINT64_MAX);
    vkResetFences(vkdev.device, 1, &r.in_flight);

    // 2. Get next swapchain image
    uint32_t image_index;
    vkAcquireNextImageKHR(vkdev.device, sc.handle, UINT64_MAX,
                          r.image_available, VK_NULL_HANDLE, &image_index);

    // 3. Record commands
    vkResetCommandBuffer(r.command_buffer, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(r.command_buffer, &begin_info);

    // Transition image from undefined to color attachment
    VkImageMemoryBarrier2 barrier_to_render{};
    barrier_to_render.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier_to_render.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    barrier_to_render.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier_to_render.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier_to_render.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier_to_render.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier_to_render.image = sc.images[image_index];
    barrier_to_render.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier_to_render.subresourceRange.levelCount = 1;
    barrier_to_render.subresourceRange.layerCount = 1;

    VkDependencyInfo dep_to_render{};
    dep_to_render.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_to_render.imageMemoryBarrierCount = 1;
    dep_to_render.pImageMemoryBarriers = &barrier_to_render;
    vkCmdPipelineBarrier2(r.command_buffer, &dep_to_render);

    // Dynamic rendering — clear to dark background
    VkRenderingAttachmentInfo color_attachment{};
    color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachment.imageView = sc.views[image_index];
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.clearValue.color = {{0.2f, 0.05f, 0.07f, 1.0f}};

    VkRenderingInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea.extent = sc.extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;

    vkCmdBeginRendering(r.command_buffer, &rendering_info);
    // Nothing to draw yet — just clearing
    vkCmdEndRendering(r.command_buffer);

    // Transition image from color attachment to presentable
    VkImageMemoryBarrier2 barrier_to_present{};
    barrier_to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier_to_present.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier_to_present.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier_to_present.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    barrier_to_present.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier_to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier_to_present.image = sc.images[image_index];
    barrier_to_present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier_to_present.subresourceRange.levelCount = 1;
    barrier_to_present.subresourceRange.layerCount = 1;

    VkDependencyInfo dep_to_present{};
    dep_to_present.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_to_present.imageMemoryBarrierCount = 1;
    dep_to_present.pImageMemoryBarriers = &barrier_to_present;
    vkCmdPipelineBarrier2(r.command_buffer, &dep_to_present);

    vkEndCommandBuffer(r.command_buffer);

    VkSemaphore wait_semaphores[] = {r.image_available};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signal_semaphores[] = {r.render_complete};

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &r.command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    vkQueueSubmit(vkdev.graphics_queue, 1, &submit_info, r.in_flight);

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &sc.handle;
    present_info.pImageIndices = &image_index;

    vkQueuePresentKHR(vkdev.present_queue, &present_info);
}