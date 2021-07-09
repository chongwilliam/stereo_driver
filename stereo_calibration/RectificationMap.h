#pragma once
#include <vector>
#ifdef _WIN32
#include <cstdint>
#else
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#endif
#include <string>

// 08/26/201


/// <summary>
/// Class for defining rectification map
/// </summary>
class RectificationMap
{

public:

	RectificationMap();
	~RectificationMap();

	/// Clear stored data
	void Clear();

	/// Save map to file
	bool Save(std::string filename);

	/// Load map from file
	bool Load(std::string filename);

	/// Print map info
	void Print();

	/// Set width of the stacked image
	void SetMosaicWidth(int imageWidth);

	/// Get width of the stacked image
	int GetMosaicWidth();

	/// Set height of the stacked image
	void SetMosaicHeight(int imageHeight);

	/// Get height of the stacked image
	int GetMosaicHeight();

	/// Set number of active imagers
	void SetNumberOfCameras(int count);

	/// Get number of active imagers
	int GetNumberOfCameras();

	/// Get pixel mapping for x-coordiante
	std::vector<float> GetMapX();

	/// Get pixel mapping for y-coordinate
	std::vector<float> GetMapY();

	/// Set pixel mapping for x-coordinate
	void SetMapX(std::vector<float> &map);

	/// Set pixel mapping for y-coordinate
	void SetMapY(std::vector<float> &map);

private:
	enum Stacking { VERTICAL = 0 };

	/// Rectification map file header
	struct Header
	{
		float version;					///< Format version
		char calibrationDate[64];		///< Date of calibration
		uint8_t stackingType;			///< Type of image stacking (0 - vertical from top-left to top-right).
		uint8_t bitsPerElement;			///< Number of bits representing the maping value
		uint32_t arraySerialNumber;		///< Camera serial number
		char arrayName[255];			///< Camera notes
		uint16_t cameraCount;			///< Total number of cameras
		uint16_t mosaicWidth;			///< Imager width in pixels
		uint16_t mosaicHeight;			///< Imager height in pixels
		uint32_t mapXSizeBytes;			///< Size in bytes for X map
		uint32_t mapYSizeBytes;			///< Size in bytes for Y map
	};
	Header m_header;


	/// Maps
	std::vector<float> m_mapX;			///< mapping for x coordiante
	std::vector<float> m_mapY;			///< mapping for y coordinate

};

