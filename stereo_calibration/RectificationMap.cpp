#include "RectificationMap.h"
#include <fstream>
#include <iostream>
#ifndef _WIN32
#include <string>
int strcpy_s(char *dst, size_t n, const char *src) 
{
    if (strlen(src) >= n)
        return -1;
    strcpy(dst, src);
    return 0;
}
#endif

RectificationMap::RectificationMap()
{

	// Initialize header data
	m_header.version = 0.1f;
	strcpy_s(m_header.calibrationDate, 64, "");
	m_header.stackingType = static_cast<uint8_t>(Stacking::VERTICAL);
	m_header.bitsPerElement = 32;
	m_header.arraySerialNumber = 0;
	strcpy_s(m_header.arrayName, 255, "");
	m_header.cameraCount = 0;
	m_header.mosaicHeight = 0;
	m_header.mosaicWidth = 0;
	m_header.arraySerialNumber = 0;
	m_header.mapXSizeBytes = 0;
	m_header.mapYSizeBytes = 0;
}


RectificationMap::~RectificationMap()
{
	Clear();
}


void RectificationMap::SetMosaicWidth(int imageWidth) 
{ 
	m_header.mosaicWidth = static_cast<uint16_t>(imageWidth); 
}


int RectificationMap::GetMosaicWidth()
{ 
	return static_cast<int>(m_header.mosaicWidth); 
}


void RectificationMap::SetMosaicHeight(int imageHeight) 
{ 
	m_header.mosaicHeight = static_cast<uint16_t>(imageHeight); 
}


int RectificationMap::GetMosaicHeight() 
{ 
	return static_cast<int>(m_header.mosaicHeight); 
}


void RectificationMap::SetNumberOfCameras(int count) 
{ 
	m_header.cameraCount = static_cast<uint16_t>(count); 
}


int RectificationMap::GetNumberOfCameras() 
{ 
	return static_cast<int>(m_header.cameraCount); 
}


std::vector<float> RectificationMap::GetMapX() 
{ 
	return m_mapX; 
}


std::vector<float> RectificationMap::GetMapY() 
{ 
	return m_mapY; 
}


void RectificationMap::SetMapX(std::vector<float> &map) 
{ 
	m_mapX = map; 
	m_header.mapXSizeBytes = static_cast<uint32_t>(m_mapX.size())*m_header.bitsPerElement / 8; 
}


void RectificationMap::SetMapY(std::vector<float> &map) 
{ 
	m_mapY = map; 
	m_header.mapYSizeBytes = static_cast<uint32_t>(m_mapY.size())*m_header.bitsPerElement / 8; 
}



/// <summary>
/// Clears this instance.
/// </summary>
void RectificationMap::Clear()
{
	m_mapX.clear();
	m_mapY.clear();

	m_header.mapXSizeBytes = 0;
	m_header.mapYSizeBytes = 0;
	m_header.cameraCount = 0;
	m_header.mosaicHeight = 0;
	m_header.mosaicWidth = 0;
	m_header.arraySerialNumber = 0;
}



/// <summary>
/// Saves rectification map to a file
/// </summary>
/// <param name="filename">The output filename.</param>
/// <returns></returns>
bool RectificationMap::Save(std::string filename)
{
	// open file for writing
	std::ofstream fs(filename, std::iostream::binary);

	if (!fs.is_open())
	{
		throw new std::runtime_error("Error: RectificationMap::Save(): Cannot open file for writing.");
	}

	// write header
	fs.write(reinterpret_cast<const char *>(&m_header.version), sizeof(m_header.version));
	fs.write(reinterpret_cast<const char *>(&m_header.calibrationDate), sizeof(m_header.calibrationDate));
	fs.write(reinterpret_cast<const char *>(&m_header.stackingType), sizeof(m_header.stackingType));
	fs.write(reinterpret_cast<const char *>(&m_header.bitsPerElement), sizeof(m_header.bitsPerElement));
	fs.write(reinterpret_cast<const char *>(&m_header.arraySerialNumber), sizeof(m_header.arraySerialNumber));
	fs.write(reinterpret_cast<const char *>(&m_header.arrayName), sizeof(m_header.arrayName));
	fs.write(reinterpret_cast<const char *>(&m_header.cameraCount), sizeof(m_header.cameraCount));
	fs.write(reinterpret_cast<const char *>(&m_header.mosaicWidth), sizeof(m_header.mosaicWidth));
	fs.write(reinterpret_cast<const char *>(&m_header.mosaicHeight), sizeof(m_header.mosaicHeight));
	fs.write(reinterpret_cast<const char *>(&m_header.mapXSizeBytes), sizeof(m_header.mapXSizeBytes));
	fs.write(reinterpret_cast<const char *>(&m_header.mapYSizeBytes), sizeof(m_header.mapYSizeBytes));

	// write maps
	fs.write(reinterpret_cast<const char*>(&m_mapX[0]), m_mapX.size() * sizeof(m_mapX[0]));
	fs.write(reinterpret_cast<const char*>(&m_mapY[0]), m_mapY.size() * sizeof(m_mapY[0]));

	// close
	fs.close();
	return true;
}


/// <summary>
/// Loads rectification map from a file.
/// </summary>
/// <param name="filename">The filename.</param>
/// <returns></returns>
bool RectificationMap::Load(std::string filename)
{
	// open file for reading
	std::ifstream fs(filename, std::iostream::binary);

	if (!fs)
	{
		throw new std::runtime_error("Error: RectificationMap::Load(): Cannot open file for reading.");
	}

	// read header
	fs.read(reinterpret_cast<char *>(&m_header.version), sizeof(m_header.version));
	fs.read(reinterpret_cast<char *>(&m_header.calibrationDate), sizeof(m_header.calibrationDate));
	fs.read(reinterpret_cast<char *>(&m_header.stackingType), sizeof(m_header.stackingType));
	fs.read(reinterpret_cast<char *>(&m_header.bitsPerElement), sizeof(m_header.bitsPerElement));
	fs.read(reinterpret_cast<char *>(&m_header.arraySerialNumber), sizeof(m_header.arraySerialNumber));
	fs.read(reinterpret_cast<char *>(&m_header.arrayName), sizeof(m_header.arrayName));
	fs.read(reinterpret_cast<char *>(&m_header.cameraCount), sizeof(m_header.cameraCount));
	fs.read(reinterpret_cast<char *>(&m_header.mosaicWidth), sizeof(m_header.mosaicWidth));
	fs.read(reinterpret_cast<char *>(&m_header.mosaicHeight), sizeof(m_header.mosaicHeight));
	fs.read(reinterpret_cast<char *>(&m_header.mapXSizeBytes), sizeof(m_header.mapXSizeBytes));
	fs.read(reinterpret_cast<char *>(&m_header.mapYSizeBytes), sizeof(m_header.mapYSizeBytes));

	// resize maps
	m_mapX.resize(m_header.mosaicWidth * m_header.mosaicHeight);
	m_mapY.resize(m_header.mosaicWidth * m_header.mosaicHeight);

	// read maps
	fs.read(reinterpret_cast<char*>(&m_mapX[0]), m_mapX.size() * sizeof(m_mapX[0]));
	fs.read(reinterpret_cast<char*>(&m_mapY[0]), m_mapY.size() * sizeof(m_mapY[0]));

	// close
	fs.close();
	return true;
}

void RectificationMap::Print()
{
	std::cout << "-----------------------------" << std::endl;
	std::cout << "Rectification Map info: " << std::endl;
	std::cout << "Version: " << m_header.version << std::endl;
	std::cout << "Calibration Date: " << m_header.calibrationDate << std::endl;
	std::cout << "Stacking type: " << m_header.stackingType << std::endl;
	std::cout << "Bits per element: " << m_header.bitsPerElement << std::endl;
	std::cout << "Array serial number: " << m_header.arraySerialNumber << std::endl;
	std::cout << "Array Name: " << m_header.arrayName << std::endl;
	std::cout << "Camera count: " << m_header.cameraCount << std::endl;
	std::cout << "Mosaic Width (px): " << m_header.mosaicWidth << std::endl;
	std::cout << "Mosaic Height (px): " << m_header.mosaicHeight << std::endl;
	std::cout << "Map X Size (Bytes): " << m_header.mapXSizeBytes << std::endl;
	std::cout << "Map Y Size (Bytes): " << m_header.mapYSizeBytes << std::endl;
	std::cout << "-----------------------------" << std::endl;

}

