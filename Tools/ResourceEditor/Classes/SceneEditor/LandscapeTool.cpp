#include "LandscapeTool.h"

LandscapeTool::LandscapeTool(eBrushType _type, const String & _spriteName, const String & _imageName)
{
    brushType = _type;
    spriteName = _spriteName;
    sprite = Sprite::Create(spriteName);
    
    imageName = _imageName;
    image = Image::CreateFromFile(imageName);
    
    intension = (IntensionMax() + IntensionMin()) / 2.0f;
    zoom = (ZoomMax() + ZoomMin()) / 2.0f;
    
    maxSize = DefaultSize();
    size = DefaultSize() / 2.f;
    maxStrength = DefaultStrength();
    strength = 1.f;
    
    relativeDrawing = true;
    averageDrawing = false;
}

LandscapeTool::~LandscapeTool()
{
    SafeRelease(image);
    SafeRelease(sprite);
}

float32 LandscapeTool::ZoomMin()
{
    return 0.2f;
}

float32 LandscapeTool::ZoomMax()
{
    return 2.0f;
}

float32 LandscapeTool::IntensionMin()
{
    return 0.0f;
}

float32 LandscapeTool::IntensionMax()
{
    return 0.50f;
}

float32 LandscapeTool::DefaultStrength()
{
    return 3.0f;
}

float32 LandscapeTool::DefaultSize()
{
    return 60.0f;
}
