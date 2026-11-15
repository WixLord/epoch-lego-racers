#include "renderer_api.h"
#include <windows.h>
#include <cstdio>
int main() {
    HWND window=CreateWindowExA(0,"STATIC","Renderer unload test",WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
    if(!window)return 1;
    for(unsigned iteration=0;iteration<3;++iteration) {
        HMODULE module=LoadLibraryA("RebornRenderer.dll");if(!module)return 2;
        auto get=reinterpret_cast<RBGetApiFn>(GetProcAddress(module,"RBGetApi"));
        const RBApi* api=get?get(8):nullptr;if(!api)return 3;
        void* context=api->create(window,320,240);if(!context)return 4;
        uint16_t glyphs[]={L'A',3,3,24,24};
        if(!api->uploadFont(context,1,64,32,glyphs,1))return 5;
        api->destroy(context);
        if(!FreeLibrary(module))return 6;
        printf("Custom font renderer unload/reload %u PASS\n",iteration+1);
    }
    DestroyWindow(window);return 0;
}
