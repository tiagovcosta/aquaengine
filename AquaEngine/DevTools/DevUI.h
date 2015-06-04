#include <AntTweakBar.h>

#include "..\AquaTypes.h"

namespace aqua
{
	class DevUI
	{
	public:

		void init(void* device, u32 window_width, u32 window_height)
		{
			TwInit(TW_DIRECT3D11, device);
			TwWindowSize(window_width, window_height);
		}

	private:

	};
}