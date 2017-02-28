#include <numeric>
#include "HUDSystem.h"

#include "UI/UIControl.h"
#include "UI/UIEvent.h"
#include "Base/BaseTypes.h"

#include "Model/PackageHierarchy/ControlNode.h"
#include "Model/PackageHierarchy/PackageNode.h"
#include "Model/PackageHierarchy/PackageControlsNode.h"

#include "EditorSystems/HUDControls.h"
#include "EditorSystems/KeyboardProxy.h"
#include "Model/ControlProperties/RootProperty.h"
#include "Model/ControlProperties/VisibleValueProperty.h"

#include "UI/Find/ControlNodeInformation.h"
#include "UI/Find/ControlInformation.h"

using namespace DAVA;

namespace
{
const Array<HUDAreaInfo::eArea, 2> AreasToHide = { { HUDAreaInfo::PIVOT_POINT_AREA, HUDAreaInfo::ROTATE_AREA } };
}

RefPtr<ControlContainer> CreateControlContainer(HUDAreaInfo::eArea area)
{
    switch (area)
    {
    case HUDAreaInfo::PIVOT_POINT_AREA:
        return RefPtr<ControlContainer>(new PivotPointControl());
    case HUDAreaInfo::ROTATE_AREA:
        return RefPtr<ControlContainer>(new RotateControl());
    case HUDAreaInfo::TOP_LEFT_AREA:
    case HUDAreaInfo::TOP_CENTER_AREA:
    case HUDAreaInfo::TOP_RIGHT_AREA:
    case HUDAreaInfo::CENTER_LEFT_AREA:
    case HUDAreaInfo::CENTER_RIGHT_AREA:
    case HUDAreaInfo::BOTTOM_LEFT_AREA:
    case HUDAreaInfo::BOTTOM_CENTER_AREA:
    case HUDAreaInfo::BOTTOM_RIGHT_AREA:
        return RefPtr<ControlContainer>(new FrameRectControl(area));
    case HUDAreaInfo::FRAME_AREA:
        return RefPtr<ControlContainer>(new FrameControl());
    case HUDAreaInfo::HIGHLIGHT_AREA:
        return RefPtr<HighlightControl>(new HighlightControl());
    case HUDAreaInfo::TEXT_AREA:
        return RefPtr<TextControl>(new TextControl());
    default:
        DVASSERT(!"unacceptable value of area");
        return RefPtr<ControlContainer>(nullptr);
    }
}

struct HUDSystem::HUD
{
    HUD(ControlNode* node, UIControl* hudControl, const Vector<HUDAreaInfo::eArea> areas);
    ~HUD();
    ControlNode* node = nullptr;
    UIControl* control = nullptr;
    UIControl* hudControl = nullptr;
    RefPtr<HUDContainer> container;
    Map<HUDAreaInfo::eArea, RefPtr<ControlContainer>> hudControls;

    static std::unique_ptr<HUD> CreateSelectionHUD(ControlNode* node, UIControl* hudControl);
    static std::unique_ptr<HUD> CreateHighlightHUD(ControlNode* node, UIControl* hudControl);
};

HUDSystem::HUD::HUD(ControlNode* node_, UIControl* hudControl_, const Vector<HUDAreaInfo::eArea> areas)
    : node(node_)
    , control(node_->GetControl())
    , hudControl(hudControl_)
    , container(new HUDContainer(node_))
{
    container->SetName(FastName("Container for HUD controls of node " + node_->GetName()));
    for (HUDAreaInfo::eArea area : areas)
    {
        RefPtr<ControlContainer> controlContainer(CreateControlContainer(area));
        container->AddChild(controlContainer.Get());
        hudControls[area] = controlContainer;
    }
    hudControl->AddControl(container.Get());
    container->InitFromGD(control->GetGeometricData());
}

std::unique_ptr<HUDSystem::HUD> HUDSystem::HUD::CreateSelectionHUD(ControlNode* node, UIControl* hudControl)
{
    DAVA::Vector<HUDAreaInfo::eArea> areas = {
        HUDAreaInfo::TOP_LEFT_AREA,
        HUDAreaInfo::TOP_CENTER_AREA,
        HUDAreaInfo::TOP_RIGHT_AREA,
        HUDAreaInfo::CENTER_LEFT_AREA,
        HUDAreaInfo::CENTER_RIGHT_AREA,
        HUDAreaInfo::BOTTOM_LEFT_AREA,
        HUDAreaInfo::BOTTOM_CENTER_AREA,
        HUDAreaInfo::BOTTOM_RIGHT_AREA
    };

    if (node->GetParent() != nullptr && node->GetParent()->GetControl() != nullptr)
    {
        areas.push_back(HUDAreaInfo::ROTATE_AREA);
        areas.push_back(HUDAreaInfo::PIVOT_POINT_AREA);
        areas.push_back(HUDAreaInfo::FRAME_AREA);
    }

    return std::unique_ptr<HUD>(new HUD(node, hudControl, areas));
}

std::unique_ptr<HUDSystem::HUD> HUDSystem::HUD::CreateHighlightHUD(ControlNode* node, UIControl* hudControl)
{
    std::unique_ptr<HUD> hud(new HUD(node, hudControl, { HUDAreaInfo::HIGHLIGHT_AREA, HUDAreaInfo::TEXT_AREA }));

    ControlNodeInformation controlInfo(node);
    const String& path = ControlInformationHelpers::GetPathToControl(&controlInfo);

    TextControl* textControl = DynamicTypeCheck<TextControl*>(hud->hudControls[HUDAreaInfo::TEXT_AREA].Get());
    textControl->SetText(path);

    return hud;
}

HUDSystem::HUD::~HUD()
{
    hudControl->RemoveControl(container.Get());
}

class HUDControl : public UIControl
{
    void Draw(const UIGeometricData& geometricData) override
    {
        UpdateLayout();
        UIControl::Draw(geometricData);
    }
};

HUDSystem::HUDSystem(EditorSystemsManager* parent)
    : BaseEditorSystem(parent)
    , hudControl(new HUDControl())
    , sortedControlList(CompareByLCA)
{
    hudControl->SetName(FastName("hudControl"));
    systemsManager->selectionChanged.Connect(this, &HUDSystem::OnSelectionChanged);
    systemsManager->magnetLinesChanged.Connect(this, &HUDSystem::OnMagnetLinesChanged);
    systemsManager->packageChanged.Connect(this, &HUDSystem::OnPackageChanged);
    systemsManager->searchResultsChanged.Connect(this, &HUDSystem::OnSearchResultsChanged);
    systemsManager->GetRootControl()->AddControl(hudControl.Get());
}

HUDSystem::~HUDSystem()
{
    DVASSERT(systemsManager->GetDragState() != EditorSystemsManager::SelectByRect);
    systemsManager->GetRootControl()->RemoveControl(hudControl.Get());
}

void HUDSystem::OnSelectionChanged(const SelectedNodes& selected, const SelectedNodes& deselected)
{
    selectionContainer.MergeSelection(selected, deselected);
    for (auto node : deselected)
    {
        ControlNode* controlNode = dynamic_cast<ControlNode*>(node);
        if (nullptr != controlNode)
        {
            selectionHudMap.erase(controlNode);
            sortedControlList.erase(controlNode);
        }
    }

    for (auto node : selected)
    {
        ControlNode* controlNode = dynamic_cast<ControlNode*>(node);
        if (controlNode != nullptr)
        {
            if (nullptr != controlNode && nullptr != controlNode->GetControl())
            {
                selectionHudMap[controlNode] = HUD::CreateSelectionHUD(controlNode, hudControl.Get());
                sortedControlList.insert(controlNode);
            }
        }
    }

    UpdateAreasVisibility();
    ProcessCursor(hoveredPoint, SEARCH_BACKWARD);
    ControlNode* node = systemsManager->GetControlNodeAtPoint(hoveredPoint);
    OnHighlightNode(node);
}

void HUDSystem::ProcessInput(UIEvent* currentInput)
{
    bool findPivot = selectionContainer.selectedNodes.size() == 1 && IsKeyPressed(KeyboardProxy::KEY_CTRL) && IsKeyPressed(KeyboardProxy::KEY_ALT);
    eSearchOrder searchOrder = findPivot ? SEARCH_BACKWARD : SEARCH_FORWARD;
    hoveredPoint = currentInput->point;
    UIEvent::Phase phase = currentInput->phase;

    switch (phase)
    {
    case UIEvent::Phase::MOVE:
    case UIEvent::Phase::BEGAN:
    case UIEvent::Phase::ENDED:
        ProcessCursor(hoveredPoint, searchOrder);
        break;
    case UIEvent::Phase::DRAG:
        if (systemsManager->GetDragState() == EditorSystemsManager::SelectByRect)
        {
            Vector2 point(pressedPoint);
            Vector2 size(hoveredPoint - pressedPoint);
            if (size.x < 0.0f)
            {
                point.x += size.x;
                size.x *= -1.0f;
            }
            if (size.y <= 0.0f)
            {
                point.y += size.y;
                size.y *= -1.0f;
            }
            selectionRectControl->SetRect(Rect(point, size));
            systemsManager->selectionRectChanged.Emit(selectionRectControl->GetAbsoluteRect());
        }
        break;
    default:
        break;
    }
    if (phase == UIEvent::Phase::MOVE
        || phase == UIEvent::Phase::WHEEL
        || phase == UIEvent::Phase::ENDED
        )
    {
        HUDAreaInfo::eArea activeArea = activeAreaInfo.area;
        ControlNode* node = systemsManager->GetControlNodeAtPoint(hoveredPoint);
        if (activeArea == HUDAreaInfo::NO_AREA || (activeArea == HUDAreaInfo::FRAME_AREA && node != activeAreaInfo.owner))
        {
            OnHighlightNode(node);
        }
        else
        {
            OnHighlightNode(nullptr);
        }
    }
}

void HUDSystem::OnHighlightNode(const ControlNode* node)
{
    if (hoveredNodeControl != nullptr)
    {
        hudControl->RemoveControl(hoveredNodeControl.Get());
        hoveredNodeControl.Set(nullptr);
    }
    if (node != nullptr)
    {
        UIControl* targetControl = node->GetControl();
        hoveredNodeControl = CreateHUDRect(node);
        hudControl->AddControl(hoveredNodeControl.Get());
    }
}

void HUDSystem::OnSearchResultsChanged(const SelectedControls& results)
{
    searchHudMap.clear();

    for (auto node : results)
    {
        if (nullptr != node->GetControl())
        {
            searchHudMap[node] = HUD::CreateHighlightHUD(node, hudControl.Get()); //std::make_unique<HUD>(node, hudControl.Get());
        }
    }
}

void HUDSystem::OnMagnetLinesChanged(const Vector<MagnetLineInfo>& magnetLines)
{
    static const float32 axtraSizeValue = 50.0f;
    DVASSERT(magnetControls.size() == magnetTargetControls.size());

    const size_type magnetsSize = magnetControls.size();
    const size_type newMagnetsSize = magnetLines.size();
    if (newMagnetsSize < magnetsSize)
    {
        auto linesRIter = magnetControls.rbegin();
        auto rectsRIter = magnetTargetControls.rbegin();
        size_type count = magnetsSize - newMagnetsSize;
        for (size_type i = 0; i < count; ++i)
        {
            UIControl* lineControl = (*linesRIter++).Get();
            UIControl* targetRectControl = (*rectsRIter++).Get();
            hudControl->RemoveControl(lineControl);
            hudControl->RemoveControl(targetRectControl);
        }
        const auto& linesEnd = magnetControls.end();
        const auto& targetRectsEnd = magnetTargetControls.end();
        magnetControls.erase(linesEnd - count, linesEnd);
        magnetTargetControls.erase(targetRectsEnd - count, targetRectsEnd);
    }
    else if (newMagnetsSize > magnetsSize)
    {
        size_type count = newMagnetsSize - magnetsSize;

        magnetControls.reserve(count);
        magnetTargetControls.reserve(count);
        for (size_type i = 0; i < count; ++i)
        {
            RefPtr<UIControl> lineControl(new UIControl());
            lineControl->SetName(FastName("magnet line control"));
            ::SetupHUDMagnetLineControl(lineControl.Get());
            hudControl->AddControl(lineControl.Get());
            magnetControls.emplace_back(lineControl);

            RefPtr<UIControl> rectControl(new UIControl());
            rectControl->SetName(FastName("rect of target control which we magnet to"));
            ::SetupHUDMagnetRectControl(rectControl.Get());
            hudControl->AddControl(rectControl.Get());
            magnetTargetControls.emplace_back(rectControl);
        }
    }
    DVASSERT(magnetLines.size() == magnetControls.size() && magnetControls.size() == magnetTargetControls.size());
    for (size_t i = 0, size = magnetLines.size(); i < size; ++i)
    {
        const MagnetLineInfo& line = magnetLines.at(i);
        const auto& gd = line.gd;

        auto linePos = line.rect.GetPosition();
        auto lineSize = line.rect.GetSize();

        linePos = ::Rotate(linePos, gd->angle);
        linePos *= gd->scale;
        lineSize[line.axis] *= gd->scale[line.axis];
        Vector2 gdPos = gd->position - ::Rotate(gd->pivotPoint * gd->scale, gd->angle);

        UIControl* lineControl = magnetControls.at(i).Get();
        float32 angle = line.gd->angle;
        Vector2 extraSize(line.axis == Vector2::AXIS_X ? axtraSizeValue : 0.0f, line.axis == Vector2::AXIS_Y ? axtraSizeValue : 0.0f);
        Vector2 extraPos = ::Rotate(extraSize, angle) / 2.0f;
        Rect lineRect(Vector2(linePos + gdPos) - extraPos, lineSize + extraSize);
        lineControl->SetRect(lineRect);
        lineControl->SetAngle(angle);

        linePos = line.targetRect.GetPosition();
        lineSize = line.targetRect.GetSize();

        linePos = ::Rotate(linePos, gd->angle);
        linePos *= gd->scale;
        lineSize *= gd->scale;

        UIControl* rectControl = magnetTargetControls.at(i).Get();
        rectControl->SetRect(Rect(linePos + gdPos, lineSize));
        rectControl->SetAngle(line.gd->angle);
    }
}

void HUDSystem::ProcessCursor(const Vector2& pos, eSearchOrder searchOrder)
{
    SetNewArea(GetControlArea(pos, searchOrder));
}

HUDAreaInfo HUDSystem::GetControlArea(const Vector2& pos, eSearchOrder searchOrder) const
{
    uint32 end = HUDAreaInfo::AREAS_BEGIN;
    int sign = 1;
    if (searchOrder == SEARCH_BACKWARD)
    {
        if (HUDAreaInfo::AREAS_BEGIN != HUDAreaInfo::AREAS_COUNT)
        {
            end = HUDAreaInfo::AREAS_COUNT - 1;
        }
        sign = -1;
    }
    for (uint32 i = HUDAreaInfo::AREAS_BEGIN; i < HUDAreaInfo::AREAS_COUNT; ++i)
    {
        for (const auto& iter : sortedControlList)
        {
            ControlNode* node = dynamic_cast<ControlNode*>(iter);
            DVASSERT(nullptr != node);
            auto findIter = selectionHudMap.find(node);
            DVASSERT(findIter != selectionHudMap.end(), "hud map corrupted");
            const auto& hud = findIter->second;
            if (hud->container->GetVisibilityFlag())
            {
                HUDAreaInfo::eArea area = static_cast<HUDAreaInfo::eArea>(end + sign * i);
                auto hudControlsIter = hud->hudControls.find(area);
                if (hudControlsIter != hud->hudControls.end())
                {
                    const auto& controlContainer = hudControlsIter->second;
                    if (controlContainer->GetVisibilityFlag() && controlContainer->IsPointInside(pos))
                    {
                        return HUDAreaInfo(hud->node, area);
                    }
                }
            }
        }
    }
    return HUDAreaInfo();
}

void HUDSystem::SetNewArea(const HUDAreaInfo& areaInfo)
{
    if (activeAreaInfo.area != areaInfo.area || activeAreaInfo.owner != areaInfo.owner)
    {
        activeAreaInfo = areaInfo;
        systemsManager->activeAreaChanged.Emit(activeAreaInfo);
    }
}

void HUDSystem::UpdateAreasVisibility()
{
    bool showAreas = sortedControlList.size() == 1;

    for (const auto& iter : sortedControlList)
    {
        ControlNode* node = dynamic_cast<ControlNode*>(iter);
        DVASSERT(nullptr != node);
        auto findIter = selectionHudMap.find(node);
        DVASSERT(findIter != selectionHudMap.end(), "hud map corrupted");
        const auto& hud = findIter->second;
        for (HUDAreaInfo::eArea area : AreasToHide)
        {
            auto hudControlsIter = hud->hudControls.find(area);
            if (hudControlsIter != hud->hudControls.end())
            {
                const RefPtr<ControlContainer>& controlContainer = hudControlsIter->second;
                controlContainer->SetSystemVisible(showAreas);
            }
        }
    }
}

void HUDSystem::OnDragStateChanged(EditorSystemsManager::eDragState currentState, EditorSystemsManager::eDragState previousState)
{
    switch (currentState)
    {
    case EditorSystemsManager::SelectByRect:
        DVASSERT(selectionRectControl.Valid() == false);
        selectionRectControl.Set(new FrameControl());
        hudControl->AddControl(selectionRectControl.Get());
        break;
    case EditorSystemsManager::Transform:
        OnHighlightNode(nullptr);
        break;
    case EditorSystemsManager::DragScreen:
        SetHUDEnabled(false);
        break;
    default:
        break;
    }

    switch (previousState)
    {
    case EditorSystemsManager::SelectByRect:
        DVASSERT(selectionRectControl.Valid());
        hudControl->RemoveControl(selectionRectControl.Get());
        selectionRectControl.Set(nullptr);
        break;
    case EditorSystemsManager::Transform:
        ClearMagnetLines();
        break;
    case EditorSystemsManager::DragScreen:
        SetHUDEnabled(true);
        break;
    default:
        break;
    }
}

void HUDSystem::OnDisplayStateChanged(EditorSystemsManager::eDisplayState currentState, EditorSystemsManager::eDisplayState previousState)
{
    if (currentState == EditorSystemsManager::Emulation)
    {
        SetHUDEnabled(false);
    }
    else if (previousState == EditorSystemsManager::Emulation)
    {
        SetHUDEnabled(true);
    }
}

bool HUDSystem::CanProcessInput(DAVA::UIEvent* currentInput) const
{
    if (hudControl->GetParent() == nullptr)
    {
        return false;
    }
    EditorSystemsManager::eDisplayState displayState = systemsManager->GetDisplayState();
    EditorSystemsManager::eDragState dragState = systemsManager->GetDragState();
    return (displayState == EditorSystemsManager::Edit || displayState == EditorSystemsManager::Preview)
    && dragState != EditorSystemsManager::Transform;
}

EditorSystemsManager::eDragState HUDSystem::RequireNewState(DAVA::UIEvent* currentInput)
{
    EditorSystemsManager::eDragState dragState = systemsManager->GetDragState();
    if (currentInput->device == eInputDevices::MOUSE && currentInput->phase == UIEvent::Phase::DRAG
        && dragState != EditorSystemsManager::Transform && dragState != EditorSystemsManager::DragScreen)
    {
        EditorSystemsManager::eDragState dragState = systemsManager->GetDragState();
        //if we in selectByRect and still drag mouse - continue this state
        if (dragState == EditorSystemsManager::SelectByRect)
        {
            return EditorSystemsManager::SelectByRect;
        }
        //check that we can draw rect
        Vector<ControlNode*> nodesUnderPoint;
        Vector2 point = currentInput->point;
        auto predicate = [point](const ControlNode* node) -> bool {
            const auto visibleProp = node->GetRootProperty()->GetVisibleProperty();
            DVASSERT(node->GetControl() != nullptr);
            return visibleProp->GetVisibleInEditor() && node->GetControl()->IsPointInside(point);
        };
        systemsManager->CollectControlNodes(std::back_inserter(nodesUnderPoint), predicate);
        bool noHudableControls = nodesUnderPoint.empty() || (nodesUnderPoint.size() == 1 && nodesUnderPoint.front()->GetParent()->GetControl() == nullptr);
        bool noHudUnderCursor = (systemsManager->GetCurrentHUDArea().area == HUDAreaInfo::NO_AREA);
        bool hotKeyDetected = IsKeyPressed(KeyboardProxy::KEY_CTRL);

        if ((hotKeyDetected || noHudableControls) && noHudUnderCursor)
        {
            if (systemsManager->GetDragState() != EditorSystemsManager::SelectByRect)
            {
                pressedPoint = point;
            }
            return EditorSystemsManager::SelectByRect;
        }
    }
    return EditorSystemsManager::NoDrag;
}

void HUDSystem::ClearMagnetLines()
{
    static const Vector<MagnetLineInfo> emptyVector;
    OnMagnetLinesChanged(emptyVector);
}

void HUDSystem::OnPackageChanged(PackageNode* package)
{
    OnHighlightNode(nullptr);
}

void HUDSystem::SetHUDEnabled(bool enabled)
{
    if (enabled)
    {
        systemsManager->GetRootControl()->AddControl(hudControl.Get());
    }
    else
    {
        systemsManager->GetRootControl()->RemoveControl(hudControl.Get());
    }
}
