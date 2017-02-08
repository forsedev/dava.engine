#include "PackageNodeInformation.h"
#include "ControlNodeInformation.h"
#include "Model/PackageHierarchy/ImportedPackagesNode.h"
#include "Model/PackageHierarchy/PackageControlsNode.h"

using namespace DAVA;

PackageNodeInformation::PackageNodeInformation(const PackageNode* packageNode_)
    : packageNode(packageNode_)
{
}

String PackageNodeInformation::GetPath() const
{
    return packageNode->GetPath().GetStringValue();
}

void PackageNodeInformation::VisitImportedPackages(const DAVA::Function<void(const PackageInformation*)>& visitor) const
{
    ImportedPackagesNode* importedPackagesNode = packageNode->GetImportedPackagesNode();

    for (int32 i = 0; i < importedPackagesNode->GetCount(); i++)
    {
        PackageNodeInformation importedPackageInfo(importedPackagesNode->GetImportedPackage(i));

        visitor(&importedPackageInfo);
    }
}

void PackageNodeInformation::VisitControls(const DAVA::Function<void(const ControlInformation*)>& visitor) const
{
    PackageControlsNode* controlsNode = packageNode->GetPackageControlsNode();

    for (int32 i = 0; i < controlsNode->GetCount(); i++)
    {
        ControlNodeInformation controlInfo(controlsNode->Get(i));

        visitor(&controlInfo);
    }
}

void PackageNodeInformation::VisitPrototypes(const DAVA::Function<void(const ControlInformation*)>& visitor) const
{
    PackageControlsNode* prototypesNode = packageNode->GetPrototypes();

    for (int32 i = 0; i < prototypesNode->GetCount(); i++)
    {
        ControlNodeInformation prototypeInfo(prototypesNode->Get(i));

        visitor(&prototypeInfo);
    }
}
