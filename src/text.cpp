#include "text.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// --- helpers ---

static uint32_t find_memory_type_text(VkPhysicalDevice physical,
                                       uint32_t allowed_types,
                                       VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((allowed_types & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    fprintf(stderr, "[text] No suitable memory type\n");
    return UINT32_MAX;
}

static uint32_t* load_spirv_text(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[text] Failed to open %s\n", path);
        return nullptr;
    }
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint32_t* buf = (uint32_t*)malloc(*size);
    fread(buf, 1, *size, f);
    fclose(f);
    return buf;
}

// --- main: create the text pipeline ---

TextPipeline create_text_pipeline(VkDevice device,
                                   VkFormat color_format,
                                   VkDescriptorSetLayout atlas_layout,
                                   GPU& gpu,
                                   uint32_t max_glyphs) {
    TextPipeline tp{};
    tp.max_glyphs = max_glyphs;

    // --- Load shaders ---
    size_t vert_size, frag_size;
    uint32_t* vert_code = load_spirv_text("shaders/text.vert.spv", &vert_size);
    uint32_t* frag_code = load_spirv_text("shaders/text.frag.spv", &frag_size);
    if (!vert_code || !frag_code) {
        free(vert_code); free(frag_code);
        return tp;
    }

    VkShaderModuleCreateInfo vert_module_info{};
    vert_module_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vert_module_info.codeSize = vert_size;
    vert_module_info.pCode    = vert_code;

    VkShaderModuleCreateInfo frag_module_info{};
    frag_module_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    frag_module_info.codeSize = frag_size;
    frag_module_info.pCode    = frag_code;

    VkShaderModule vert_module, frag_module;
    vkCreateShaderModule(device, &vert_module_info, nullptr, &vert_module);
    vkCreateShaderModule(device, &frag_module_info, nullptr, &frag_module);
    free(vert_code);
    free(frag_code);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert_module;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag_module;
    stages[1].pName  = "main";

    // --- Vertex input: TextVertex = pos(vec2) + uv(vec2) + color(vec4) ---
    VkVertexInputBindingDescription binding{};
    binding.binding   = 0;
    binding.stride    = sizeof(TextVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[3]{};
    attrs[0].binding = 0; attrs[0].location = 0;
    attrs[0].format  = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset  = offsetof(TextVertex, pos);
    attrs[1].binding = 0; attrs[1].location = 1;
    attrs[1].format  = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset  = offsetof(TextVertex, uv);
    attrs[2].binding = 0; attrs[2].location = 2;
    attrs[2].format  = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[2].offset  = offsetof(TextVertex, color);

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount   = 1;
    vertex_input.pVertexBindingDescriptions      = &binding;
    vertex_input.vertexAttributeDescriptionCount = 3;
    vertex_input.pVertexAttributeDescriptions    = attrs;

    // --- Input assembly, viewport, raster, multisample (all standard) ---
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth   = 1.0f;
    raster.cullMode    = VK_CULL_MODE_NONE;
    raster.frontFace   = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // --- Color blending: alpha blend on, premultiplied alpha source ---
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable         = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp        = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend.alphaBlendOp        = VK_BLEND_OP_ADD;
    blend.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend{};
    color_blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments    = &blend;

    // --- Dynamic viewport + scissor ---
    VkDynamicState dynamics[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates    = dynamics;

    // --- Pipeline layout: descriptor set 0 (atlas) + push constants ---
    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc_range.offset     = 0;
    pc_range.size       = sizeof(TextPushConstants);

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount         = 1;
    layout_info.pSetLayouts            = &atlas_layout;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges    = &pc_range;

    if (vkCreatePipelineLayout(device, &layout_info, nullptr, &tp.layout) != VK_SUCCESS) {
        fprintf(stderr, "[text] vkCreatePipelineLayout failed\n");
        vkDestroyShaderModule(device, vert_module, nullptr);
        vkDestroyShaderModule(device, frag_module, nullptr);
        return tp;
    }

    // --- Dynamic rendering format ---
    VkPipelineRenderingCreateInfo rendering_info{};
    rendering_info.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering_info.colorAttachmentCount    = 1;
    rendering_info.pColorAttachmentFormats = &color_format;

    // --- Pipeline ---
    VkGraphicsPipelineCreateInfo pipe_info{};
    pipe_info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe_info.pNext               = &rendering_info;
    pipe_info.stageCount          = 2;
    pipe_info.pStages             = stages;
    pipe_info.pVertexInputState   = &vertex_input;
    pipe_info.pInputAssemblyState = &input_assembly;
    pipe_info.pViewportState      = &viewport_state;
    pipe_info.pRasterizationState = &raster;
    pipe_info.pMultisampleState   = &multisample;
    pipe_info.pColorBlendState    = &color_blend;
    pipe_info.pDynamicState       = &dynamic_state;
    pipe_info.layout              = tp.layout;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe_info,
                                   nullptr, &tp.handle) != VK_SUCCESS) {
        fprintf(stderr, "[text] vkCreateGraphicsPipelines failed\n");
        vkDestroyPipelineLayout(device, tp.layout, nullptr);
        tp.layout = VK_NULL_HANDLE;
        vkDestroyShaderModule(device, vert_module, nullptr);
        vkDestroyShaderModule(device, frag_module, nullptr);
        return tp;
    }

    vkDestroyShaderModule(device, vert_module, nullptr);
    vkDestroyShaderModule(device, frag_module, nullptr);

    // --- Vertex buffer: persistent host-visible mapping ---
    VkDeviceSize buffer_size = (VkDeviceSize)max_glyphs * 6 * sizeof(TextVertex);

    VkBufferCreateInfo buf_info{};
    buf_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size        = buffer_size;
    buf_info.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &buf_info, nullptr, &tp.vertex_buffer) != VK_SUCCESS) {
        fprintf(stderr, "[text] vkCreateBuffer failed\n");
        vkDestroyPipeline(device, tp.handle, nullptr);
        vkDestroyPipelineLayout(device, tp.layout, nullptr);
        tp.handle = VK_NULL_HANDLE;
        tp.layout = VK_NULL_HANDLE;
        return tp;
    }

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device, tp.vertex_buffer, &mem_reqs);

    uint32_t mem_type = find_memory_type_text(
        gpu.device, mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (mem_type == UINT32_MAX) {
        vkDestroyBuffer(device, tp.vertex_buffer, nullptr);
        vkDestroyPipeline(device, tp.handle, nullptr);
        vkDestroyPipelineLayout(device, tp.layout, nullptr);
        tp.vertex_buffer = VK_NULL_HANDLE;
        tp.handle = VK_NULL_HANDLE;
        tp.layout = VK_NULL_HANDLE;
        return tp;
    }

    VkMemoryAllocateInfo mem_alloc{};
    mem_alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_alloc.allocationSize  = mem_reqs.size;
    mem_alloc.memoryTypeIndex = mem_type;

    if (vkAllocateMemory(device, &mem_alloc, nullptr, &tp.vertex_memory) != VK_SUCCESS) {
        fprintf(stderr, "[text] vkAllocateMemory failed\n");
        vkDestroyBuffer(device, tp.vertex_buffer, nullptr);
        vkDestroyPipeline(device, tp.handle, nullptr);
        vkDestroyPipelineLayout(device, tp.layout, nullptr);
        tp.vertex_buffer = VK_NULL_HANDLE;
        tp.handle = VK_NULL_HANDLE;
        tp.layout = VK_NULL_HANDLE;
        return tp;
    }

    vkBindBufferMemory(device, tp.vertex_buffer, tp.vertex_memory, 0);

    // Persistent mapping — keep the pointer for the lifetime of the buffer
    if (vkMapMemory(device, tp.vertex_memory, 0, buffer_size, 0,
                    &tp.vertex_mapped) != VK_SUCCESS) {
        fprintf(stderr, "[text] vkMapMemory failed\n");
        vkFreeMemory(device, tp.vertex_memory, nullptr);
        vkDestroyBuffer(device, tp.vertex_buffer, nullptr);
        vkDestroyPipeline(device, tp.handle, nullptr);
        vkDestroyPipelineLayout(device, tp.layout, nullptr);
        return tp;
    }

    printf("[text] Pipeline created (max %u glyphs, vertex buffer %zu KB)\n",
           max_glyphs, (size_t)(buffer_size / 1024));
    return tp;
}

uint32_t build_text_vertices(TextPipeline& tp,
                              const AxylFont& font,
                              const char* utf8_text,
                              float origin_x_px,
                              float origin_y_px,
                              float pixel_size,
                              float r, float g, float b, float a) {
    if (!tp.vertex_mapped) {
        return 0;
    }

    TextVertex* verts = (TextVertex*)tp.vertex_mapped;
    uint32_t glyph_index = 0;

    float cursor_x = origin_x_px;
    float baseline_y = origin_y_px;

    // Atlas dimensions in pixels — for normalizing atlas UVs to [0, 1]
    float atlas_w = (float)font.atlas_width;
    float atlas_h = (float)font.atlas_height;

    for (const char* p = utf8_text; *p; p++) {
        // ASCII-only for now (we'll handle multibyte UTF-8 later)
        uint32_t codepoint = (uint8_t)*p;

        const Glyph* g_meta = font_get_glyph(font, codepoint);
        if (!g_meta) {
            // Unknown glyph — skip silently
            continue;
        }

        // Whitespace and glyphs with zero quad: just advance cursor
        bool has_quad = (g_meta->plane_right > g_meta->plane_left) &&
                        (g_meta->plane_top   > g_meta->plane_bottom);

        if (has_quad && glyph_index < tp.max_glyphs) {
            // Compute screen-space quad corners
            float x0 = cursor_x + g_meta->plane_left   * pixel_size;
            float x1 = cursor_x + g_meta->plane_right  * pixel_size;
            float y0 = baseline_y - g_meta->plane_top    * pixel_size;
            float y1 = baseline_y - g_meta->plane_bottom * pixel_size;

            // Atlas UVs (msdf-atlas-gen uses bottom-up Y for atlas coords)
            float u0 = g_meta->atlas_left   / atlas_w;
            float u1 = g_meta->atlas_right  / atlas_w;
            float v0 = 1.0f - g_meta->atlas_top    / atlas_h;
            float v1 = 1.0f - g_meta->atlas_bottom / atlas_h;

            TextVertex* v = &verts[glyph_index * 6];

            // Triangle 1: top-left, top-right, bottom-left
            v[0].pos[0] = x0; v[0].pos[1] = y0;
            v[0].uv[0]  = u0; v[0].uv[1]  = v0;
            v[0].color[0] = r; v[0].color[1] = g; v[0].color[2] = b; v[0].color[3] = a;

            v[1].pos[0] = x1; v[1].pos[1] = y0;
            v[1].uv[0]  = u1; v[1].uv[1]  = v0;
            v[1].color[0] = r; v[1].color[1] = g; v[1].color[2] = b; v[1].color[3] = a;

            v[2].pos[0] = x0; v[2].pos[1] = y1;
            v[2].uv[0]  = u0; v[2].uv[1]  = v1;
            v[2].color[0] = r; v[2].color[1] = g; v[2].color[2] = b; v[2].color[3] = a;

            // Triangle 2: top-right, bottom-right, bottom-left
            v[3].pos[0] = x1; v[3].pos[1] = y0;
            v[3].uv[0]  = u1; v[3].uv[1]  = v0;
            v[3].color[0] = r; v[3].color[1] = g; v[3].color[2] = b; v[3].color[3] = a;

            v[4].pos[0] = x1; v[4].pos[1] = y1;
            v[4].uv[0]  = u1; v[4].uv[1]  = v1;
            v[4].color[0] = r; v[4].color[1] = g; v[4].color[2] = b; v[4].color[3] = a;

            v[5].pos[0] = x0; v[5].pos[1] = y1;
            v[5].uv[0]  = u0; v[5].uv[1]  = v1;
            v[5].color[0] = r; v[5].color[1] = g; v[5].color[2] = b; v[5].color[3] = a;

            glyph_index++;
        }

        // Advance cursor regardless (whitespace still advances)
        cursor_x += g_meta->advance * pixel_size;
    }

    tp.glyph_count = glyph_index;
    return glyph_index;
}

void render_text(VkCommandBuffer cmd,
                 TextPipeline& tp,
                 Atlas& atlas,
                 uint32_t screen_width,
                 uint32_t screen_height,
                 float pixel_range) {
    if (tp.glyph_count == 0 || tp.handle == VK_NULL_HANDLE) {
        return;
    }

    // Bind the text pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tp.handle);

    // Bind the atlas descriptor set at set 0
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            tp.layout,
                            0, 1, &atlas.set,
                            0, nullptr);

    // Push constants: screen size + pxrange
    TextPushConstants pc{};
    pc.screen_size[0] = (float)screen_width;
    pc.screen_size[1] = (float)screen_height;
    pc.pixel_range    = pixel_range;
    pc._padding       = 0.0f;
    vkCmdPushConstants(cmd,
                       tp.layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);

    // Bind the vertex buffer
    VkBuffer buffers[] = { tp.vertex_buffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);

    // Set viewport and scissor to match screen
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = (float)screen_width;
    viewport.height   = (float)screen_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent.width  = screen_width;
    scissor.extent.height = screen_height;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Draw: 6 vertices per glyph
    vkCmdDraw(cmd, tp.glyph_count * 6, 1, 0, 0);
}
void destroy_text_pipeline(VkDevice device, TextPipeline& tp) {
    if (tp.vertex_mapped) {
        if (tp.vertex_memory != VK_NULL_HANDLE) {
            vkUnmapMemory(device, tp.vertex_memory);
        }
        tp.vertex_mapped = nullptr;
    }
    if (tp.vertex_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, tp.vertex_buffer, nullptr);
        tp.vertex_buffer = VK_NULL_HANDLE;
    }
    if (tp.vertex_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, tp.vertex_memory, nullptr);
        tp.vertex_memory = VK_NULL_HANDLE;
    }
    if (tp.handle != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, tp.handle, nullptr);
        tp.handle = VK_NULL_HANDLE;
    }
    if (tp.layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, tp.layout, nullptr);
        tp.layout = VK_NULL_HANDLE;
    }
}