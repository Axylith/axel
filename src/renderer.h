#pragma once
#include <vulkan/vulkan.h>
#include "swapchain.h"
#include "vulkan_init.h"

struct Renderer{
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkSemaphore image_available;
    VkSemaphore render_complete;
    VkFence in_flight;
};

Renderer create_renderer(VulkanDevice& vkdev, GPU& gpu);
void render_frame(Renderer& Renderer, VulkanDevice& vkdev, Swapchain& swapchain);