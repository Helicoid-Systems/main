#include "computer_preview_metrics.h"

#include <cstdlib>
#include <iostream>

using helicoid::manager::ComputerPreviewMetrics;
using helicoid::manager::computerPreviewMetrics;

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool matches(const ComputerPreviewMetrics& metrics,
             int cellWidth, int cellHeight,
             int tileWidth, int tileHeight,
             int previewHeight, int detailsHeight)
{
    return metrics.gridCell.width == cellWidth && metrics.gridCell.height == cellHeight &&
        metrics.tile.width == tileWidth && metrics.tile.height == tileHeight &&
        metrics.previewHeight == previewHeight && metrics.detailsHeight == detailsHeight;
}

void presetsMatchPersistedSizeContract()
{
    constexpr ComputerPreviewMetrics compact = computerPreviewMetrics(0);
    constexpr ComputerPreviewMetrics standard = computerPreviewMetrics(1);
    constexpr ComputerPreviewMetrics large = computerPreviewMetrics(2);
    constexpr ComputerPreviewMetrics extraLarge = computerPreviewMetrics(3);

    require(matches(compact, 208, 222, 192, 210, 108, 42),
            "compact preset dimensions must remain stable");
    require(matches(standard, 272, 258, 256, 246, 144, 42),
            "standard preset dimensions must remain stable");
    require(matches(large, 336, 294, 320, 282, 180, 42),
            "large preset dimensions must remain stable");
    require(matches(extraLarge, 464, 366, 448, 354, 252, 42),
            "extra large preset dimensions must remain stable");
    require(compact.gridCell.width > compact.tile.width &&
                standard.gridCell.width > standard.tile.width &&
                large.gridCell.width > large.tile.width &&
                extraLarge.gridCell.width > extraLarge.tile.width,
            "grid cells must preserve horizontal layout padding");
    require(compact.gridCell.height > compact.tile.height &&
                standard.gridCell.height > standard.tile.height &&
                large.gridCell.height > large.tile.height &&
                extraLarge.gridCell.height > extraLarge.tile.height,
            "grid cells must preserve vertical layout padding");
}

void unknownIndicesUseStandardPreset()
{
    constexpr ComputerPreviewMetrics fallback = computerPreviewMetrics(-1);
    constexpr ComputerPreviewMetrics standard = computerPreviewMetrics(1);
    require(fallback.tile.width == standard.tile.width &&
                fallback.tile.height == standard.tile.height &&
                fallback.previewHeight == standard.previewHeight,
            "unknown preset indices must fall back to standard dimensions");
}

} // namespace

int main()
{
    presetsMatchPersistedSizeContract();
    unknownIndicesUseStandardPreset();
    std::cout << "computer preview metrics tests passed\n";
    return EXIT_SUCCESS;
}
