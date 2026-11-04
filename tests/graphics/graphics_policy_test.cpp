#include <pgp/graphics/graphics_policy.hpp>

#include <iostream>

int main() {
    using namespace pgp::graphics;
    const bool valid =
        select(Platform::Mac, true, false).api == QSGRendererInterface::Metal &&
        select(Platform::Linux, true, false).api == QSGRendererInterface::OpenGL &&
        select(Platform::Windows, true, false).api == QSGRendererInterface::OpenGL &&
        select(Platform::Windows, false, false).api == QSGRendererInterface::Direct3D11 &&
        select(Platform::Mac, true, true).api == QSGRendererInterface::Software;
    if (!valid) {
        std::cerr << "Graphics selection violated platform/recovery policy\n";
        return 1;
    }
    return 0;
}
