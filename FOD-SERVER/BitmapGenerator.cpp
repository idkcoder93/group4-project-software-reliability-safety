#include "BitmapGenerator.h"
#include <cstring>
#include <cctype>

namespace FODServer
{
    static constexpr int GRID_ROWS = 6;
    static constexpr int GRID_COLS = 6;
    static constexpr int CELL_SIZE = 100;

    void BitmapGenerator::zoneToGrid(const std::string& zone, int& row, int& col)
    {
        row = -1;
        col = -1;

        if (zone.size() >= 2U)
        {
            const char letter = static_cast<char>(
                std::toupper(static_cast<unsigned char>(zone[0])));
            if ((letter >= 'A') && (letter <= 'F'))
            {
                row = static_cast<int>(letter - 'A');
            }

            const char digit = zone[1];
            if ((digit >= '1') && (digit <= '6'))
            {
                col = static_cast<int>(digit - '1');
            }
        }
    }

    std::vector<char> BitmapGenerator::generateRunwayBitmap(const std::string& affectedZone)
    {
        const int rowBytes = BMP_WIDTH * 3;
        const int paddingLen = (4 - (rowBytes % 4)) % 4;
        const int stride = rowBytes + paddingLen;
        const int pixelBytes = stride * BMP_HEIGHT;
        const int fileSize = HEADER_SIZE + pixelBytes;

        std::vector<char> bmp(static_cast<size_t>(fileSize), '\0');

        //BMP File Header (14 bytes)
        bmp[0] = 'B';
        bmp[1] = 'M';
        (void)memcpy(&bmp[2], &fileSize, 4);
        const int dataOffset = HEADER_SIZE;
        (void)memcpy(&bmp[10], &dataOffset, 4);

        //DIB Header BITMAPINFOHEADER (40 bytes)
        const int dibSize = 40;
        (void)memcpy(&bmp[14], &dibSize, 4);
        (void)memcpy(&bmp[18], &BMP_WIDTH, 4);
        (void)memcpy(&bmp[22], &BMP_HEIGHT, 4);
        const short planes = 1;
        const short bpp = 24;
        (void)memcpy(&bmp[26], &planes, 2);
        (void)memcpy(&bmp[28], &bpp, 2);

        //determine highlighted zone
        int hazRow = -1;
        int hazCol = -1;
        zoneToGrid(affectedZone, hazRow, hazCol);

        //write pixel data (bottom-up row order per BMP spec)
        for (int y = 0; y < BMP_HEIGHT; ++y)
        {
            const int gridRow = (BMP_HEIGHT - 1 - y) / CELL_SIZE;
            for (int x = 0; x < BMP_WIDTH; ++x)
            {
                const int gridCol = x / CELL_SIZE;
                const size_t offset = static_cast<size_t>(HEADER_SIZE) +
                    (static_cast<size_t>(y) * static_cast<size_t>(stride)) +
                    (static_cast<size_t>(x) * 3U);

                unsigned char b = 0U;
                unsigned char g = 0U;
                unsigned char r = 0U;

                const bool onGridLine = ((x % CELL_SIZE) == 0) || ((y % CELL_SIZE) == 0);

                if (onGridLine)
                {
                    r = 60U;  g = 60U;  b = 60U;
                }
                else if ((gridRow == hazRow) && (gridCol == hazCol))
                {
                    r = 200U; g = 40U;  b = 40U;
                }
                else
                {
                    r = 45U;  g = 120U; b = 45U;
                }

                bmp[offset] = static_cast<char>(b);
                bmp[offset + 1U] = static_cast<char>(g);
                bmp[offset + 2U] = static_cast<char>(r);
            }
        }

        return bmp;
    }
}