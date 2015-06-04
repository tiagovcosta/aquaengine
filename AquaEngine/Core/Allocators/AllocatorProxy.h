#pragma once

#if !_DEBUG
	#define BlockAllocatorProxy BlockAllocator
	//#define ProxyAllocator Allocator
#else
	namespace aqua
	{
		class BlockAllocatorProxy;
		class ProxyAllocator;
	};
#endif