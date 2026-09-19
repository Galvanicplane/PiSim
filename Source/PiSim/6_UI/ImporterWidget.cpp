// ImporterWidget.cpp

#include "ImporterWidget.h"
#include "Vehicle.h"
#include "ModelFbxImporter.h"
#include "MotorConfigurator.h"

// @state: WIP - İthalat sürecini çalıştırır ve kemik isimlerine göre otomatik motor atamalarını tetikler
bool UPiSimModularImporterWidget::ExecuteImport(APiSimVehicle* TargetVehicle, USkeletalMesh* InMesh, float ScaleMultiplier)
{
    if (!TargetVehicle || !InMesh)
    {
        return false;
    }

    bool bSuccess = UPiSimModelFbxImporter::ApplySkeletalMeshToVehicle(TargetVehicle, InMesh, ScaleMultiplier);
    if (bSuccess)
    {
        UPiSimMotorConfigurator::AutoAssignMotorsByBoneNames(TargetVehicle);
    }

    return bSuccess;
}
