- [ ] Window 和 Swapchain如何组织？Window resize之后如何通知swapchain?考虑多线程情况下的问题，实践表面当前设计中，渲染循环在子线程的话resize会出明显bug
- [ ] 尝试headless surface，应该有助于设计抽象的render target
- [ ] 给出支持单个DescriptorSet分配回收的DescriptorManager用于GlobalDescriptorSetManager，
        带有FREE_DESCRIPTOR_SET_BIT的flag，alloc出UniqueDescriptorSet