#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>

#include "deletion_queue.h"
#include "utils.h"
#include "utils/cast.h"

namespace tr::renderer {

class DescriptorAllocator {
  VkDescriptorPool pool;

 public:
  static auto init(VkDevice device, uint32_t max_sets, std::span<const VkDescriptorPoolSize> pool_sizes)
      -> DescriptorAllocator;
  void defer_deletion(DeviceDeletionStack&);

  auto allocate(VkDevice device, VkDescriptorSetLayout layout) -> VkDescriptorSet;
  void reset(VkDevice device);
};

struct DescriptorSetLayoutBindingBuilder : VkBuilder<DescriptorSetLayoutBindingBuilder, VkDescriptorSetLayoutBinding> {
  constexpr DescriptorSetLayoutBindingBuilder()
      : VkBuilder({
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 0,
            .stageFlags = 0,
            .pImmutableSamplers = nullptr,
        }) {}

  constexpr auto binding_(uint32_t binding_) -> DescriptorSetLayoutBindingBuilder& {
    binding = binding_;
    return *this;
  }
  constexpr auto descriptor_type(VkDescriptorType type) -> DescriptorSetLayoutBindingBuilder& {
    descriptorType = type;
    return *this;
  }
  constexpr auto descriptor_count(uint32_t count) -> DescriptorSetLayoutBindingBuilder& {
    descriptorCount = count;
    return *this;
  }
  constexpr auto stages(VkShaderStageFlags shader_stages) -> DescriptorSetLayoutBindingBuilder& {
    stageFlags = shader_stages;
    return *this;
  }
  constexpr auto immutable_samplers(std::span<const VkSampler> samplers) -> DescriptorSetLayoutBindingBuilder& {
    descriptorCount = utils::narrow_cast<uint32_t>(samplers.size());
    pImmutableSamplers = samplers.data();
    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkDescriptorSetLayoutBinding { return inner(); }
};

struct DescriptorSetLayoutBuilder : VkBuilder<DescriptorSetLayoutBuilder, VkDescriptorSetLayoutCreateInfo> {
  constexpr DescriptorSetLayoutBuilder()
      : VkBuilder({
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = 0,
            .pBindings = nullptr,
        }) {}

  auto bindings(std::span<const VkDescriptorSetLayoutBinding> bindings) -> DescriptorSetLayoutBuilder& {
    bindingCount = utils::narrow_cast<uint32_t>(bindings.size());
    pBindings = bindings.data();
    return *this;
  }

  [[nodiscard]] auto build(VkDevice device) const -> VkDescriptorSetLayout {
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;

    VK_UNWRAP(vkCreateDescriptorSetLayout, device, inner_ptr(), nullptr, &layout);
    return layout;
  }
};

struct DescriptorUpdater : VkBuilder<DescriptorUpdater, VkWriteDescriptorSet> {
  constexpr DescriptorUpdater(VkDescriptorSet set, uint32_t binding)
      : VkBuilder({
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = set,
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        }) {}

  constexpr auto image_info(std::span<const VkDescriptorImageInfo> image_info) -> DescriptorUpdater& {
    descriptorCount = utils::narrow_cast<uint32_t>(image_info.size());
    pImageInfo = image_info.data();
    return *this;
  }
  constexpr auto buffer_info(std::span<const VkDescriptorBufferInfo> buffer_info) -> DescriptorUpdater& {
    descriptorCount = utils::narrow_cast<uint32_t>(buffer_info.size());
    pBufferInfo = buffer_info.data();
    return *this;
  }
  constexpr auto type(VkDescriptorType type) -> DescriptorUpdater& {
    descriptorType = type;
    return *this;
  }

  void write(VkDevice device) const { vkUpdateDescriptorSets(device, 1, inner_ptr(), 0, nullptr); }
};

}  // namespace tr::renderer
