#pragma once

//generates a runway zone bitmap image (>= 1 MB) highlighting the affected zone

#include <vector>
#include <string>

namespace FODServer
{
    class BitmapGenerator
    {
    public:
        //generate a BMP byte buffer (>= 1 MB) with the given zone highlighted
        static std::vector<char> generateRunwayBitmap(const std::string& affectedZone);

        static void zoneToGrid(const std::string& zone, int& row, int& col);
    private:
        static constexpr int BMP_WIDTH = 600;
        static constexpr int BMP_HEIGHT = 600;
        static constexpr int HEADER_SIZE = 54;

        //map a zone string to a grid cell index (row, col)
        //static void zoneToGrid(const std::string& zone, int& row, int& col);
    };
}