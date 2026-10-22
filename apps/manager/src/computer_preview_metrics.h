#pragma once

namespace helicoid::manager {

struct PreviewDimension final {
    int width = 0;
    int height = 0;
};

struct ComputerPreviewMetrics final {
    PreviewDimension gridCell;
    PreviewDimension tile;
    int previewHeight = 0;
    int detailsHeight = 0;
};

// Preset order is persisted in Manager preferences and must remain stable.
constexpr ComputerPreviewMetrics computerPreviewMetrics(int index) noexcept
{
    switch (index) {
    case 0:
        return {{208, 222}, {192, 210}, 108, 42};
    case 2:
        return {{336, 294}, {320, 282}, 180, 42};
    case 3:
        return {{464, 366}, {448, 354}, 252, 42};
    default:
        return {{272, 258}, {256, 246}, 144, 42};
    }
}

} // namespace helicoid::manager
