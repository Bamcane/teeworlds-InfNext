/* (c) DDNet developers. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.  */

#include <base/system.h>
#include <engine/graphics.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/gamecore.h>
#include <game/mapitems.h>
#include <engine/external/pnglite/pnglite.h>
#include "mapconverter.h"
/*
	Usage: map_convert_07 <source map filepath> <dest map filepath>
*/

CDataFileReader g_DataReader;
CDataFileWriter g_DataWriter;

// global new image data (set by ReplaceImageItem)
int g_aNewDataSize[64];
void *g_apNewData[64];

int g_Index = 0;
int g_NextDataItemID = -1;

int g_aImageIDs[64];

int CMapConverter::LoadPNG(CImageInfo *pImg, const char *pFilename)
{
	char aCompleteFilename[512];
	unsigned char *pBuffer;
	png_t Png; // ignore_convention

	// open file for reading
	png_init(0,0); // ignore_convention

	IOHANDLE File = m_pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL, aCompleteFilename, sizeof(aCompleteFilename));
	if(File)
		io_close(File);
	else
	{
		dbg_msg("game/png", "failed to open file. filename='%s'", pFilename);
		return 0;
	}

	int Error = png_open_file(&Png, aCompleteFilename); // ignore_convention
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("game/png", "failed to open file. filename='%s'", aCompleteFilename);
		if(Error != PNG_FILE_ERROR)
			png_close_file(&Png); // ignore_convention
		return 0;
	}

	if(Png.depth != 8 || (Png.color_type != PNG_TRUECOLOR && Png.color_type != PNG_TRUECOLOR_ALPHA)) // ignore_convention
	{
		dbg_msg("game/png", "invalid format. filename='%s'", aCompleteFilename);
		png_close_file(&Png); // ignore_convention
		return 0;
	}

	pBuffer = (unsigned char *)mem_alloc(Png.width * Png.height * Png.bpp, 1); // ignore_convention
	png_get_data(&Png, pBuffer); // ignore_convention
	png_close_file(&Png); // ignore_convention

	pImg->m_Width = Png.width; // ignore_convention
	pImg->m_Height = Png.height; // ignore_convention
	if(Png.color_type == PNG_TRUECOLOR) // ignore_convention
		pImg->m_Format = CImageInfo::FORMAT_RGB;
	else if(Png.color_type == PNG_TRUECOLOR_ALPHA) // ignore_convention
		pImg->m_Format = CImageInfo::FORMAT_RGBA;
	pImg->m_pData = pBuffer;
	return 1;
}

void FreePNG(CImageInfo *pImg)
{
	mem_free(pImg->m_pData);
	pImg->m_pData = nullptr;
}

bool CMapConverter::CheckImageDimensions(void *pLayerItem, int LayerType, const char *pFilename)
{
	if(LayerType != MAPITEMTYPE_LAYER)
		return true;

	CMapItemLayer *pImgLayer = (CMapItemLayer *)pLayerItem;
	if(pImgLayer->m_Type != LAYERTYPE_TILES)
		return true;

	CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pImgLayer;
	if(pTMap->m_Image == -1)
		return true;

	int Type;
	void *pItem = g_DataReader.GetItem(g_aImageIDs[pTMap->m_Image], &Type, nullptr);
	if(Type != MAPITEMTYPE_IMAGE)
		return true;

	CMapItemImage *pImgItem = (CMapItemImage *)pItem;

	if(pImgItem->m_Width % 16 == 0 && pImgItem->m_Height % 16 == 0 && pImgItem->m_Width > 0 && pImgItem->m_Height > 0)
		return true;

	char aTileLayerName[12];
	IntsToStr(pTMap->m_aName, sizeof(pTMap->m_aName) / sizeof(int), aTileLayerName);
	char *pName = (char *)g_DataReader.GetData(pImgItem->m_ImageName);
	dbg_msg("map_convert_07", "%s: Tile layer \"%s\" uses image \"%s\" with width %d, height %d, which is not divisible by 16. This is not supported in Teeworlds 0.7. Please scale the image and replace it manually.", pFilename, aTileLayerName, pName, pImgItem->m_Width, pImgItem->m_Height);
	return false;
}

void *CMapConverter::ReplaceImageItem(void *pItem, int Type, CMapItemImage *pNewImgItem)
{
	if(Type != MAPITEMTYPE_IMAGE)
		return pItem;

	CMapItemImage *pImgItem = (CMapItemImage *)pItem;

	if(!pImgItem->m_External)
		return pItem;

	char *pName = (char *)g_DataReader.GetData(pImgItem->m_ImageName);
	dbg_msg("map_convert_07", "embedding image '%s'", pName);

	CImageInfo ImgInfo;
	char aStr[64];
	str_format(aStr, sizeof(aStr), "data/mapres/%s.png", pName);
	if(!LoadPNG(&ImgInfo, aStr))
		return pItem; // keep as external if we don't have a mapres to replace

	*pNewImgItem = *pImgItem;

	pNewImgItem->m_Width = ImgInfo.m_Width;
	pNewImgItem->m_Height = ImgInfo.m_Height;
	pNewImgItem->m_External = false;
	pNewImgItem->m_ImageData = g_NextDataItemID++;

	g_apNewData[g_Index] = ImgInfo.m_pData;
	g_aNewDataSize[g_Index] = ImgInfo.m_Width * ImgInfo.m_Height * 4;
	g_Index++;

	return (void *)pNewImgItem;
}

int CMapConverter::ConvertMapTo7(const char* pFilename)
{
    // ddnet
	char pSourceFileName[IO_MAX_PATH_LENGTH];
	char aDestFileName[IO_MAX_PATH_LENGTH];

    str_format(pSourceFileName, sizeof(pSourceFileName), "clientmaps/next/%s.map", pFilename);
    str_format(aDestFileName, sizeof(aDestFileName), "clientmaps/next/%s7.map", pFilename);

	if(!g_DataReader.Open(Storage(), pSourceFileName, IStorage::TYPE_ALL))
	{
		dbg_msg("MapConverter", "failed to open source map. filename='%s'", pSourceFileName);
		return -1;
	}

	if(!g_DataWriter.Open(Storage(), aDestFileName, IStorage::TYPE_ALL))
	{
		dbg_msg("MapConverter", "failed to open destination map. filename='%s'", aDestFileName);
		return -1;
	}

	g_NextDataItemID = g_DataReader.NumData();

	int i = 0;
	for(int Index = 0; Index < g_DataReader.NumItems(); Index++)
	{
		int Type;
		g_DataReader.GetItem(Index, &Type, nullptr);
		if(Type == MAPITEMTYPE_IMAGE)
			g_aImageIDs[i++] = Index;
	}

	bool Success = true;

	if(i > 64)
		dbg_msg("MapConverter", "%s: Uses more textures than the client maximum of 64.", pSourceFileName);

	// add all items
	for(int Index = 0; Index < g_DataReader.NumItems(); Index++)
	{
		int Type, ID;
		void *pItem = g_DataReader.GetItem(Index, &Type, &ID);
		int Size = g_DataReader.GetItemSize(Index);

		// filter ITEMTYPE_EX items, they will be automatically added again
		if(Type == ITEMTYPE_EX)
		{
			continue;
		}

		Success &= CheckImageDimensions(pItem, Type, pSourceFileName);

		CMapItemImage NewImageItem;
		pItem = ReplaceImageItem(pItem, Type, &NewImageItem);
		if(!pItem)
			return -1;
		g_DataWriter.AddItem(Type, ID, Size, pItem);
	}

	// add all data
	for(int Index = 0; Index < g_DataReader.NumData(); Index++)
	{
		void *pData = g_DataReader.GetData(Index);
		int Size = g_DataReader.GetDataSize(Index);
		g_DataWriter.AddData(Size, pData);
	}

	for(int Index = 0; Index < g_Index; Index++)
	{
		g_DataWriter.AddData(g_aNewDataSize[Index], g_apNewData[Index]);
	}

	g_DataReader.Close();
	g_DataWriter.Finish();
	return Success ? 0 : -1;
}