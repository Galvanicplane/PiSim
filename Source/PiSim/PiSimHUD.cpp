// PiSimHUD.cpp
#include "PiSimHUD.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Kismet/GameplayStatics.h"
#include "PiSimGarageRobot.h"
#include "PiSimModelImporter.h"
#include "ProceduralMeshComponent.h"

APiSimHUD::APiSimHUD()
{
}

void APiSimHUD::BeginPlay()
{
    Super::BeginPlay();

    // Dynamically instantiate UPiSimGarageWidget if not assigned in Blueprint
    TSubclassOf<UPiSimGarageWidget> ClassToUse = GarageWidgetClass ? GarageWidgetClass : TSubclassOf<UPiSimGarageWidget>(UPiSimGarageWidget::StaticClass());
    
    if (GetWorld())
    {
        ActiveGarageWidget = CreateWidget<UPiSimGarageWidget>(GetWorld(), ClassToUse);
        if (ActiveGarageWidget)
        {
            ActiveGarageWidget->AddToViewport(10);
            
            APlayerController* PC = GetOwningPlayerController() ? GetOwningPlayerController() : GetWorld()->GetFirstPlayerController();
            if (PC)
            {
                PC->bShowMouseCursor = true;
                FInputModeGameAndUI InputMode;
                InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
                PC->SetInputMode(InputMode);
            }
        }
    }
}

void APiSimHUD::DrawHUD()
{
    Super::DrawHUD();

    if (!Canvas)
    {
        return;
    }

    APlayerController* PC = GetOwningPlayerController() ? GetOwningPlayerController() : GetWorld()->GetFirstPlayerController();

    float MouseX = 0.0f;
    float MouseY = 0.0f;
    bool bLeftClickJustPressed = false;

    if (PC)
    {
        PC->GetMousePosition(MouseX, MouseY);
        bLeftClickJustPressed = PC->WasInputKeyJustPressed(EKeys::LeftMouseButton);
    }

    // =========================================================================
    // DEDICATED HUD FOR PiSimModelImporter
    // =========================================================================
    AActor* FoundImporter = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimModelImporter::StaticClass());
    APiSimModelImporter* Importer = Cast<APiSimModelImporter>(FoundImporter);

    if (Importer)
    {
        // 1. TOP HEADER BAR
        DrawRect(FLinearColor(0.03f, 0.05f, 0.09f, 0.95f), 0, 0, Canvas->SizeX, 68);
        DrawRect(FLinearColor(0.0f, 0.94f, 1.0f, 0.85f), 0, 66, Canvas->SizeX, 2);

        FCanvasTextItem HeaderText(FVector2D(24, 18), FText::FromString(TEXT("🚀 PiSim MODEL IMPORTER | DEDICATED FBX & UCX TEST SYSTEM")), GEngine->GetMediumFont(), FLinearColor(0.0f, 0.94f, 1.0f, 1.0f));
        Canvas->DrawItem(HeaderText);

        // Scale Buttons: 0.1X, 1.0X, 10.0X
        float ScaleBtnLabels[3] = { 0.1f, 1.0f, 10.0f };
        const TCHAR* ScaleBtnTexts[3] = { TEXT("🔍 0.1X"), TEXT("📐 1.0X (Default)"), TEXT("🔬 10.0X") };

        for (int32 s = 0; s < 3; ++s)
        {
            float BtnX = 480.0f + (s * 135.0f);
            float BtnY = 16.0f;
            float BtnW = 125.0f;
            float BtnH = 34.0f;

            bool bHover = (MouseX >= BtnX && MouseX <= (BtnX + BtnW) && MouseY >= BtnY && MouseY <= (BtnY + BtnH));
            bool bIsActive = FMath::IsNearlyEqual(Importer->ImportScaleMultiplier, ScaleBtnLabels[s], 0.01f);

            if (bHover && bLeftClickJustPressed)
            {
                if (s == 0) Importer->SetScale_0_1X();
                else if (s == 1) Importer->SetScale_1_0X();
                else if (s == 2) Importer->SetScale_10_0X();
            }

            FLinearColor Bg = bIsActive ? FLinearColor(0.0f, 0.94f, 1.0f, 0.95f) : (bHover ? FLinearColor(0.2f, 0.45f, 0.65f, 0.9f) : FLinearColor(0.12f, 0.18f, 0.28f, 0.85f));
            FLinearColor TxtCol = bIsActive ? FLinearColor::Black : FLinearColor::White;
            DrawRect(Bg, BtnX, BtnY, BtnW, BtnH);

            FCanvasTextItem BtnTxt(FVector2D(BtnX + 10, BtnY + 9), FText::FromString(ScaleBtnTexts[s]), GEngine->GetSmallFont(), TxtCol);
            Canvas->DrawItem(BtnTxt);
        }

        // Reimport Button
        float ReimpBtnX = 480.0f + (3 * 135.0f);
        float ReimpBtnY = 16.0f;
        float ReimpBtnW = 145.0f;
        float ReimpBtnH = 34.0f;
        bool bReimpHover = (MouseX >= ReimpBtnX && MouseX <= (ReimpBtnX + ReimpBtnW) && MouseY >= ReimpBtnY && MouseY <= (ReimpBtnY + ReimpBtnH));
        if (bReimpHover && bLeftClickJustPressed)
        {
            Importer->ImportAndSpawnRobot();
        }
        FLinearColor ReimpBg = bReimpHover ? FLinearColor(0.0f, 1.0f, 0.5f, 0.95f) : FLinearColor(0.1f, 0.5f, 0.25f, 0.85f);
        DrawRect(ReimpBg, ReimpBtnX, ReimpBtnY, ReimpBtnW, ReimpBtnH);
        FCanvasTextItem ReimpTxt(FVector2D(ReimpBtnX + 10, ReimpBtnY + 9), FText::FromString(TEXT("🔄 REIMPORT FBX")), GEngine->GetSmallFont(), FLinearColor::White);
        Canvas->DrawItem(ReimpTxt);

        // Physics Simulation Toggle Button
        float PhysBtnX = ReimpBtnX + ReimpBtnW + 15.0f;
        float PhysBtnY = 16.0f;
        float PhysBtnW = 200.0f;
        float PhysBtnH = 34.0f;
        bool bPhysHover = (MouseX >= PhysBtnX && MouseX <= (PhysBtnX + PhysBtnW) && MouseY >= PhysBtnY && MouseY <= (PhysBtnY + PhysBtnH));
        if (bPhysHover && bLeftClickJustPressed)
        {
            Importer->TogglePhysicsSimulation();
        }
        FLinearColor PhysBg = Importer->bIsPhysicsSimulating ? FLinearColor(0.95f, 0.4f, 0.0f, 0.95f) : (bPhysHover ? FLinearColor(0.3f, 0.5f, 0.7f, 0.9f) : FLinearColor(0.15f, 0.25f, 0.4f, 0.85f));
        DrawRect(PhysBg, PhysBtnX, PhysBtnY, PhysBtnW, PhysBtnH);
        FString PhysStr = Importer->bIsPhysicsSimulating ? TEXT("⚡ FİZİK: AKTİF (AÇIK)") : TEXT("⚡ FİZİĞİ SİMÜLE ET");
        FCanvasTextItem PhysTxt(FVector2D(PhysBtnX + 12, PhysBtnY + 9), FText::FromString(PhysStr), GEngine->GetSmallFont(), FLinearColor::White);
        Canvas->DrawItem(PhysTxt);

        // Left Telemetry Info Card
        float CardX = 24.0f;
        float CardY = 88.0f;
        float CardW = 350.0f;
        float CardH = 320.0f;
        DrawRect(FLinearColor(0.04f, 0.07f, 0.12f, 0.92f), CardX, CardY, CardW, CardH);
        DrawRect(FLinearColor(0.0f, 0.94f, 1.0f, 0.8f), CardX, CardY, CardW, 2);

        FCanvasTextItem CardTitle(FVector2D(CardX + 16, CardY + 14), FText::FromString(TEXT("📊 MODEL & TEST BİLGİSİ")), GEngine->GetSmallFont(), FLinearColor(0.0f, 0.94f, 1.0f, 1.0f));
        Canvas->DrawItem(CardTitle);

        FString InfoLines[] = {
            FString::Printf(TEXT("📁 Dosya: Saved/Robots/Cache/robot_import_test.fbx")),
            FString::Printf(TEXT("🎨 Görsel Parçalar: %d Adet (Render Açık)"), Importer->VisualMeshComponents.Num()),
            FString::Printf(TEXT("🛡️ UCX Çarpışma: %d Adet (Chaos Collision)"), Importer->CollisionMeshComponents.Num()),
            FString::Printf(TEXT("📐 Geçerli Ölçek: %.2fX"), Importer->ImportScaleMultiplier),
            FString::Printf(TEXT("⚡ Fizik Simülasyonu: %s"), Importer->bIsPhysicsSimulating ? TEXT("AÇIK (Yerçekimi Aktif)") : TEXT("KAPALI (Statik Havada)")),
            FString(TEXT("----------------------------------------")),
            FString(TEXT("🎮 KAMERA KONTROLLERİ:")),
            FString(TEXT("  • Sol Tık + Sürükle: 360° Orbit Döndür")),
            FString(TEXT("  • Sağ Tık + Sürükle: Kamerayı Kaydır (Pan)")),
            FString(TEXT("  • Fare Tekerleği: Yaklaş / Uzaklaş (Zoom)"))
        };

        for (int32 line = 0; line < 10; ++line)
        {
            FLinearColor LineColor = (line < 5) ? FLinearColor::White : FLinearColor(0.7f, 0.85f, 1.0f, 1.0f);
            if (line == 4) LineColor = Importer->bIsPhysicsSimulating ? FLinearColor(0.2f, 1.0f, 0.4f, 1.0f) : FLinearColor(1.0f, 0.6f, 0.2f, 1.0f);
            FCanvasTextItem LineTxt(FVector2D(CardX + 16, CardY + 44 + (line * 24)), FText::FromString(InfoLines[line]), GEngine->GetSmallFont(), LineColor);
            Canvas->DrawItem(LineTxt);
        }

        return; // Done rendering Model Importer HUD
    }

    AActor* FoundActor = UGameplayStatics::GetActorOfClass(GetWorld(), APiSimGarageRobot::StaticClass());
    APiSimGarageRobot* Robot = Cast<APiSimGarageRobot>(FoundActor);

    int32 SelectedIdx = (ActiveGarageWidget) ? ActiveGarageWidget->SelectedJointIndex : 0;
    float CurrAngle = (Robot && Robot->JointLimitsList.IsValidIndex(SelectedIdx)) ? Robot->JointLimitsList[SelectedIdx].CurrentAngle : 0.0f;
    float MinAngle = (Robot && Robot->JointLimitsList.IsValidIndex(SelectedIdx)) ? Robot->JointLimitsList[SelectedIdx].MinAngle : -90.0f;
    float MaxAngle = (Robot && Robot->JointLimitsList.IsValidIndex(SelectedIdx)) ? Robot->JointLimitsList[SelectedIdx].MaxAngle : 90.0f;
    FString ActiveAxisName = (Robot && Robot->JointLimitsList.IsValidIndex(SelectedIdx)) ? Robot->JointLimitsList[SelectedIdx].RotationAxisName : TEXT("Y");
    bool bInvertAxis = (Robot && Robot->JointLimitsList.IsValidIndex(SelectedIdx)) ? Robot->JointLimitsList[SelectedIdx].bInvertAxis : false;

    // Find assigned PWM port & driver profile for selected joint
    int32 AssignedPwmPin = -1;
    FString DriverProfileName = TEXT("Standard Servo (1000 - 2000 us)");
    if (Robot)
    {
        FString SelectedName = Robot->SubMeshNames.IsValidIndex(SelectedIdx) ? Robot->SubMeshNames[SelectedIdx] : TEXT("");
        for (const FPiSimVirtualPort& Port : Robot->RobotConfig.VirtualPorts)
        {
            if (Port.TargetComponent == SelectedName && Port.PortType == EPiSimVirtualPortType::PWM)
            {
                AssignedPwmPin = Port.PinOrAddress;
                DriverProfileName = Port.HardwareControllerType;
                break;
            }
        }
    }

    // ---------------------------------------------------------
    // 1. TOP HEADER BAR - SPACEX VEHICLE CONFIGURATOR
    // ---------------------------------------------------------
    DrawRect(FLinearColor(0.03f, 0.05f, 0.09f, 0.94f), 0, 0, Canvas->SizeX, 64);
    DrawRect(FLinearColor(0.0f, 0.94f, 1.0f, 0.8f), 0, 62, Canvas->SizeX, 2); // Neon Cyan Line

    FCanvasTextItem HeaderText(FVector2D(24, 18), FText::FromString(TEXT("🛸 PiSim GARAGE CONFIGURATOR | SPACEX VEHICLE TELEMETRY")), GEngine->GetMediumFont(), FLinearColor(0.0f, 0.94f, 1.0f, 1.0f));
    HeaderText.Scale = FVector2D(1.15f, 1.15f);
    Canvas->DrawItem(HeaderText);

    FString CadFormatText = Robot ? Robot->LoadedModelFormatName : TEXT("YÜKLENMEDİ");
    FString NetStatus = FString::Printf(TEXT("📦 CAD FORMATI: %s  |  📡 ROS2: ONLINE"), *CadFormatText);
    FCanvasTextItem StatusText(FVector2D(Canvas->SizeX - 680, 22), FText::FromString(NetStatus), GEngine->GetSmallFont(), FLinearColor(0.0f, 1.0f, 0.5f, 1.0f));
    Canvas->DrawItem(StatusText);


    // Top Bar 3-Way Mode Switch (Visual, Structural, Sensor) + Re-Import + Physics Test Button
    const TCHAR* ViewBtnLabels[3] = { TEXT("🎨 VISUAL"), TEXT("🦴 STRUCTURAL"), TEXT("📡 SENSOR") };
    const EGarageViewMode ViewModes[3] = { EGarageViewMode::Visual, EGarageViewMode::Structural, EGarageViewMode::Sensor };

    for (int32 vm = 0; vm < 3; ++vm)
    {
        float VmBtnX = 420.0f + (vm * 115.0f);
        float VmBtnY = 16.0f;
        float VmBtnW = 108.0f;
        float VmBtnH = 30.0f;

        bool bVmHover = (MouseX >= VmBtnX && MouseX <= (VmBtnX + VmBtnW) && MouseY >= VmBtnY && MouseY <= (VmBtnY + VmBtnH));
        bool bIsActiveMode = (Robot && Robot->CurrentViewMode == ViewModes[vm]);

        if (bVmHover && bLeftClickJustPressed && ActiveGarageWidget)
        {
            ActiveGarageWidget->SwitchViewMode(ViewModes[vm]);
        }

        FLinearColor VmBg = bIsActiveMode ? FLinearColor(0.0f, 0.94f, 1.0f, 0.95f) : (bVmHover ? FLinearColor(0.2f, 0.4f, 0.6f, 0.8f) : FLinearColor(0.12f, 0.18f, 0.28f, 0.85f));
        FLinearColor VmTxtCol = bIsActiveMode ? FLinearColor::Black : FLinearColor::White;
        DrawRect(VmBg, VmBtnX, VmBtnY, VmBtnW, VmBtnH);

        FCanvasTextItem VmTxt(FVector2D(VmBtnX + 10, VmBtnY + 7), FText::FromString(ViewBtnLabels[vm]), GEngine->GetSmallFont(), VmTxtCol);
        Canvas->DrawItem(VmTxt);
    }

    // Re-Import Button
    float ReimpBtnX = 775.0f;
    float ReimpBtnY = 16.0f;
    float ReimpBtnW = 115.0f;
    float ReimpBtnH = 30.0f;
    bool bReimpHover = (MouseX >= ReimpBtnX && MouseX <= (ReimpBtnX + ReimpBtnW) && MouseY >= ReimpBtnY && MouseY <= (ReimpBtnY + ReimpBtnH));
    if (bReimpHover && bLeftClickJustPressed && ActiveGarageWidget)
    {
        ActiveGarageWidget->ReimportModel();
    }
    FLinearColor ReimpBg = bReimpHover ? FLinearColor(0.0f, 1.0f, 0.5f, 0.9f) : FLinearColor(0.1f, 0.5f, 0.25f, 0.85f);
    DrawRect(ReimpBg, ReimpBtnX, ReimpBtnY, ReimpBtnW, ReimpBtnH);
    FCanvasTextItem ReimpTxt(FVector2D(ReimpBtnX + 10, ReimpBtnY + 7), FText::FromString(TEXT("🔄 RE-IMPORT")), GEngine->GetSmallFont(), FLinearColor::White);
    Canvas->DrawItem(ReimpTxt);

    // ---------------------------------------------------------
    // CAD SCALE MULTIPLIER BUTTONS IN TOP BAR (0.1X, 1X, 10X, 100X)
    // ---------------------------------------------------------
    const float ScaleMults[4] = { 0.1f, 1.0f, 10.0f, 100.0f };
    const TCHAR* ScaleLabels[4] = { TEXT("0.1X"), TEXT("1X"), TEXT("10X"), TEXT("100X") };

    FCanvasTextItem ScaleTitle(FVector2D(905.0f, 22.0f), FText::FromString(TEXT("🔍 ÖLÇEK:")), GEngine->GetSmallFont(), FLinearColor(0.0f, 0.94f, 1.0f, 1.0f));
    Canvas->DrawItem(ScaleTitle);

    for (int32 sm = 0; sm < 4; ++sm)
    {
        float ScaleBtnX = 965.0f + (sm * 50.0f);
        float ScaleBtnY = 16.0f;
        float ScaleBtnW = 46.0f;
        float ScaleBtnH = 30.0f;

        bool bScaleHover = (MouseX >= ScaleBtnX && MouseX <= (ScaleBtnX + ScaleBtnW) && MouseY >= ScaleBtnY && MouseY <= (ScaleBtnY + ScaleBtnH));
        bool bIsActiveScale = (Robot && FMath::IsNearlyEqual(Robot->CadUnitScaleMultiplier, ScaleMults[sm], 0.05f));

        if (bScaleHover && bLeftClickJustPressed && ActiveGarageWidget)
        {
            ActiveGarageWidget->SetCadScaleMultiplierFromUI(ScaleMults[sm]);
        }

        FLinearColor ScaleBg = bIsActiveScale ? FLinearColor(0.0f, 0.94f, 1.0f, 0.95f) : (bScaleHover ? FLinearColor(0.3f, 0.5f, 0.7f, 0.85f) : FLinearColor(0.15f, 0.22f, 0.35f, 0.85f));
        FLinearColor ScaleTxtCol = bIsActiveScale ? FLinearColor::Black : FLinearColor::White;
        DrawRect(ScaleBg, ScaleBtnX, ScaleBtnY, ScaleBtnW, ScaleBtnH);

        FCanvasTextItem ScaleTxt(FVector2D(ScaleBtnX + 6, ScaleBtnY + 7), FText::FromString(ScaleLabels[sm]), GEngine->GetSmallFont(), ScaleTxtCol);
        Canvas->DrawItem(ScaleTxt);
    }

    // Physics Test Button
    float TestBtnX = 1180.0f;
    float TestBtnY = 16.0f;
    float TestBtnW = 115.0f;
    float TestBtnH = 30.0f;
    bool bTestHover = (MouseX >= TestBtnX && MouseX <= (TestBtnX + TestBtnW) && MouseY >= TestBtnY && MouseY <= (TestBtnY + TestBtnH));
    if (bTestHover && bLeftClickJustPressed && ActiveGarageWidget)
    {
        ActiveGarageWidget->ToggleTestPanel();
    }
    FLinearColor TestBg = bTestHover ? FLinearColor(1.0f, 0.8f, 0.0f, 0.9f) : FLinearColor(0.7f, 0.5f, 0.0f, 0.85f);
    DrawRect(TestBg, TestBtnX, TestBtnY, TestBtnW, TestBtnH);
    FCanvasTextItem TestBtnTxt(FVector2D(TestBtnX + 12, TestBtnY + 7), FText::FromString(TEXT("🧪 FİZİK TEST")), GEngine->GetSmallFont(), FLinearColor::Black);
    Canvas->DrawItem(TestBtnTxt);


    // ---------------------------------------------------------
    // 2. LEFT PANEL - CAD SUB-MESH HIERARCHY TREE & SAVE BUTTON
    // ---------------------------------------------------------
    float PanelWidth = 360.0f;
    float PanelHeight = Canvas->SizeY - 140.0f;
    DrawRect(FLinearColor(0.04f, 0.06f, 0.10f, 0.88f), 20, 80, PanelWidth, PanelHeight);
    DrawRect(FLinearColor(0.0f, 0.94f, 1.0f, 0.4f), 20, 80, PanelWidth, 32); // Header Bar

    FCanvasTextItem HierarchyTitle(FVector2D(32, 88), FText::FromString(TEXT("📦 CAD SUB-MESH HIERARCHY TREE (CLICK TO SELECT)")), GEngine->GetSmallFont(), FLinearColor::White);
    Canvas->DrawItem(HierarchyTitle);

    if (Robot && Robot->SubMeshNames.Num() > 0)
    {
        int32 TotalCount = Robot->SubMeshNames.Num();
        float CurrentRowY = 118.0f;

        FString ModeTitle = TEXT("▼ 🎨 GÖRSEL PARÇA HİYERARŞİSİ (VISUAL MESH TREE)");
        if (Robot->CurrentViewMode == EGarageViewMode::Structural) ModeTitle = TEXT("▼ 🦴 YAPISAL & ÇARPIŞMA MESH HİYERARŞİSİ (STRUCTURAL CM_ MESHES)");
        else if (Robot->CurrentViewMode == EGarageViewMode::Sensor) ModeTitle = TEXT("▼ 📡 SENSÖR MESH HİYERARŞİSİ (SENSOR S_ MESHES)");

        // Draw Mode-Specific Blender/CAD Parent-Child Hierarchy Tree
        DrawRect(FLinearColor(0.0f, 0.45f, 0.25f, 0.8f), 24, CurrentRowY, PanelWidth - 8, 22);
        FCanvasTextItem GrpHeader(FVector2D(30, CurrentRowY + 3), FText::FromString(ModeTitle), GEngine->GetSmallFont(), FLinearColor::Yellow);
        Canvas->DrawItem(GrpHeader);
        CurrentRowY += 24.0f;

        TArray<int32> ValidIndices;
        for (int32 i = 0; i < TotalCount; ++i)
        {
            EMeshCategoryType Cat = Robot->MeshCategories.IsValidIndex(i) ? Robot->MeshCategories[i] : EMeshCategoryType::Visual;
            if (Robot->CurrentViewMode == EGarageViewMode::Visual && Cat != EMeshCategoryType::Visual) continue;
            if (Robot->CurrentViewMode == EGarageViewMode::Structural && Cat != EMeshCategoryType::Structural) continue;
            if (Robot->CurrentViewMode == EGarageViewMode::Sensor && Cat != EMeshCategoryType::Sensor) continue;
            ValidIndices.Add(i);
        }
        if (ValidIndices.Num() == 0)
        {
            for (int32 i = 0; i < TotalCount; ++i) ValidIndices.Add(i);
        }

        TArray<int32> TreeOrder;
        TSet<int32> ProcessedNodes;

        auto AddNodeAndChildren = [&](auto& Self, int32 NodeIdx, bool bParentCollapsed) -> void
        {
            if (ProcessedNodes.Contains(NodeIdx)) return;
            ProcessedNodes.Add(NodeIdx);

            if (!bParentCollapsed)
            {
                TreeOrder.Add(NodeIdx);
            }

            bool bIsThisCollapsed = bParentCollapsed || (ActiveGarageWidget && ActiveGarageWidget->CollapsedParentIndices.Contains(NodeIdx));

            for (int32 ChildIdx : ValidIndices)
            {
                if (ChildIdx != NodeIdx && !ProcessedNodes.Contains(ChildIdx) && Robot->ParentJointIndices.IsValidIndex(ChildIdx) && Robot->ParentJointIndices[ChildIdx] == NodeIdx)
                {
                    Self(Self, ChildIdx, bIsThisCollapsed);
                }
            }
        };


        for (int32 RootIdx : ValidIndices)
        {
            int32 ParentIdx = Robot->ParentJointIndices.IsValidIndex(RootIdx) ? Robot->ParentJointIndices[RootIdx] : -1;
            bool bHasValidParent = (ParentIdx >= 0 && ParentIdx != RootIdx && ValidIndices.Contains(ParentIdx));
            if (!bHasValidParent && !ProcessedNodes.Contains(RootIdx))
            {
                AddNodeAndChildren(AddNodeAndChildren, RootIdx, false);
            }
        }
        for (int32 Idx : ValidIndices)
        {
            if (!ProcessedNodes.Contains(Idx))
            {
                TreeOrder.Add(Idx);
            }
        }


        for (int32 i : TreeOrder)
        {
            if (CurrentRowY > (80.0f + PanelHeight - 28.0f)) break;

            bool bIsSelected = (i == SelectedIdx);
            bool bVisHovered = (MouseX >= 28.0f && MouseX <= 48.0f && MouseY >= CurrentRowY && MouseY <= (CurrentRowY + 22.0f));

            bool bIsVisible = (Robot->SubMeshComponents.IsValidIndex(i) && Robot->SubMeshComponents[i]) ? Robot->SubMeshComponents[i]->IsVisible() : true;
            if (bVisHovered && bLeftClickJustPressed && Robot->SubMeshComponents.IsValidIndex(i) && Robot->SubMeshComponents[i])
            {
                Robot->SubMeshComponents[i]->SetVisibility(!bIsVisible, false);
                bIsVisible = !bIsVisible;
            }

            int32 TreeDepth = 0;
            int32 CurrAnc = Robot->ParentJointIndices.IsValidIndex(i) ? Robot->ParentJointIndices[i] : -1;
            while (CurrAnc >= 0 && CurrAnc != i && ValidIndices.Contains(CurrAnc) && TreeDepth < 10)
            {
                TreeDepth++;
                CurrAnc = Robot->ParentJointIndices.IsValidIndex(CurrAnc) ? Robot->ParentJointIndices[CurrAnc] : -1;
            }

            bool bHasChildren = false;
            for (int32 ChildIdx : ValidIndices)
            {
                if (ChildIdx != i && Robot->ParentJointIndices.IsValidIndex(ChildIdx) && Robot->ParentJointIndices[ChildIdx] == i)
                {
                    bHasChildren = true;
                    break;
                }
            }

            float IndentX = TreeDepth * 18.0f;
            float TriBtnX = 52.0f + IndentX;
            float RowTextX = bHasChildren ? (TriBtnX + 18.0f) : TriBtnX;
            bool bTriHovered = (bHasChildren && MouseX >= TriBtnX && MouseX <= (TriBtnX + 16.0f) && MouseY >= CurrentRowY && MouseY <= (CurrentRowY + 22.0f));
            bool bRowHovered = (MouseX >= 50.0f && MouseX <= (28.0f + PanelWidth - 16.0f) && MouseY >= CurrentRowY && MouseY <= (CurrentRowY + 22.0f) && !bVisHovered && !bTriHovered);

            if (bTriHovered && bLeftClickJustPressed && ActiveGarageWidget)
            {
                if (ActiveGarageWidget->CollapsedParentIndices.Contains(i))
                    ActiveGarageWidget->CollapsedParentIndices.Remove(i);
                else
                    ActiveGarageWidget->CollapsedParentIndices.Add(i);
            }

            if (bRowHovered && bLeftClickJustPressed && ActiveGarageWidget)
            {
                ActiveGarageWidget->SelectJoint(i);
            }

            bool bIsChassis = Robot->StructuralPropsList.IsValidIndex(i) ? Robot->StructuralPropsList[i].bIsChassisGroup : false;
            FString GroupTag = bIsChassis ? TEXT(" [🔗 CHASSIS]") : TEXT("");

            DrawRect(bIsVisible ? FLinearColor(0.0f, 0.75f, 0.4f, 0.9f) : FLinearColor(0.35f, 0.1f, 0.1f, 0.85f), 30, CurrentRowY + 3, 16, 16);
            FCanvasTextItem VisTxt(FVector2D(34, CurrentRowY + 3), FText::FromString(bIsVisible ? TEXT("✔") : TEXT("×")), GEngine->GetSmallFont(), FLinearColor::White);
            Canvas->DrawItem(VisTxt);

            FLinearColor RowBg = bIsSelected ? FLinearColor(0.0f, 0.6f, 0.9f, 0.9f) : (bRowHovered ? FLinearColor(0.1f, 0.3f, 0.45f, 0.7f) : FLinearColor(0.05f, 0.12f, 0.2f, 0.5f));
            FLinearColor TextColor = bIsSelected ? FLinearColor::Black : (bRowHovered ? FLinearColor::Yellow : FLinearColor::White);
            DrawRect(RowBg, 52, CurrentRowY, PanelWidth - 40, 22);

            if (bHasChildren)
            {
                bool bIsCollapsed = (ActiveGarageWidget && ActiveGarageWidget->CollapsedParentIndices.Contains(i));
                FString TriStr = bIsCollapsed ? TEXT("►") : TEXT("▼");
                FCanvasTextItem TriTxt(FVector2D(TriBtnX + 2, CurrentRowY + 3), FText::FromString(TriStr), GEngine->GetSmallFont(), bTriHovered ? FLinearColor::Yellow : FLinearColor::White);
                Canvas->DrawItem(TriTxt);
            }

            FString NodeText = FString::Printf(TEXT("[%d] %s%s"), i, *Robot->SubMeshNames[i], *GroupTag);
            FCanvasTextItem RowText(FVector2D(RowTextX, CurrentRowY + 3), FText::FromString(NodeText), GEngine->GetSmallFont(), TextColor);
            Canvas->DrawItem(RowText);

            CurrentRowY += 23.0f;
        }
    }


    // --- FLIP MESH NORMALS TOGGLE BUTTON ---
    float FlipBtnX = 28.0f;
    float FlipBtnY = 80.0f + PanelHeight - 92.0f;
    float FlipBtnW = PanelWidth - 16.0f;
    float FlipBtnH = 32.0f;
    bool bFlipHovered = (MouseX >= FlipBtnX && MouseX <= (FlipBtnX + FlipBtnW) && MouseY >= FlipBtnY && MouseY <= (FlipBtnY + FlipBtnH));

    if (bFlipHovered && bLeftClickJustPressed && Robot)
    {
        Robot->ToggleFlipMeshNormals();
    }

    bool bIsFlipped = (Robot) ? Robot->bFlipMeshNormals : false;
    FLinearColor FlipBg = bIsFlipped ? FLinearColor(0.9f, 0.5f, 0.0f, 0.9f) : (bFlipHovered ? FLinearColor(0.2f, 0.35f, 0.5f, 0.8f) : FLinearColor(0.1f, 0.15f, 0.22f, 0.8f));
    DrawRect(FlipBg, FlipBtnX, FlipBtnY, FlipBtnW, FlipBtnH);

    FString FlipLabelStr = FString::Printf(TEXT("🔄 FLIP MESH NORMALS: [ %s ]"), bIsFlipped ? TEXT("ON (Ters Yön)") : TEXT("OFF (Normal Yön)"));
    FCanvasTextItem FlipLabel(FVector2D(FlipBtnX + 24.0f, FlipBtnY + 8.0f), FText::FromString(FlipLabelStr), GEngine->GetSmallFont(), FLinearColor::White);
    Canvas->DrawItem(FlipLabel);

    // --- SAVE & BROADCAST BUTTON AT BOTTOM OF LEFT PANEL ---
    float SaveBtnX = 28.0f;
    float SaveBtnY = 80.0f + PanelHeight - 52.0f;
    float SaveBtnW = PanelWidth - 16.0f;
    float SaveBtnH = 42.0f;
    bool bSaveHovered = (MouseX >= SaveBtnX && MouseX <= (SaveBtnX + SaveBtnW) && MouseY >= SaveBtnY && MouseY <= (SaveBtnY + SaveBtnH));

    if (bSaveHovered && bLeftClickJustPressed && ActiveGarageWidget)
    {
        ActiveGarageWidget->SaveAndBroadcastConfig();
    }

    FLinearColor SaveBg = bSaveHovered ? FLinearColor(0.0f, 1.0f, 0.5f, 1.0f) : FLinearColor(0.0f, 0.7f, 0.35f, 0.9f);
    DrawRect(SaveBg, SaveBtnX, SaveBtnY, SaveBtnW, SaveBtnH);

    FCanvasTextItem SaveLabel(FVector2D(SaveBtnX + 22.0f, SaveBtnY + 12.0f), FText::FromString(TEXT("💾 SAVE CONFIG & BROADCAST TO PI 5 [S]")), GEngine->GetSmallFont(), FLinearColor::Black);
    SaveLabel.Scale = FVector2D(1.1f, 1.1f);
    Canvas->DrawItem(SaveLabel);

    // ---------------------------------------------------------
    // 3. RIGHT PANEL - KINEMATIC JOINT, AXIS & LIMITS CONFIGURATOR
    // ---------------------------------------------------------
    float RightPanelX = Canvas->SizeX - 430.0f;
    DrawRect(FLinearColor(0.04f, 0.06f, 0.10f, 0.88f), RightPanelX, 80, 410, 620);

    if (!Robot || Robot->CurrentViewMode == EGarageViewMode::Visual)
    {
        DrawRect(FLinearColor(0.0f, 1.0f, 0.5f, 0.5f), RightPanelX, 80, 410, 28);
        FCanvasTextItem ConfigTitle(FVector2D(RightPanelX + 12, 86), FText::FromString(TEXT("⚙️ PARÇA KİNEMATİK VE LİMİT AYARLARI (MESH KINEMATICS)")), GEngine->GetSmallFont(), FLinearColor::Black);
        Canvas->DrawItem(ConfigTitle);

        FString ActiveMeshName = (Robot && Robot->SubMeshNames.IsValidIndex(SelectedIdx)) ? Robot->SubMeshNames[SelectedIdx] : TEXT("None");
        FString TargetInfo = FString::Printf(TEXT("HEDEF PARÇA (MESH): [%d] %s"), SelectedIdx, *ActiveMeshName);
        FCanvasTextItem TargetText(FVector2D(RightPanelX + 16, 115), FText::FromString(TargetInfo), GEngine->GetSmallFont(), FLinearColor(0.0f, 0.94f, 1.0f, 1.0f));
        Canvas->DrawItem(TargetText);

        bool bIsGrp = Robot->StructuralPropsList.IsValidIndex(SelectedIdx) ? Robot->StructuralPropsList[SelectedIdx].bIsChassisGroup : false;
        float GrpBtnX = RightPanelX + 14; float GrpBtnY = 148; float GrpBtnW = 380; float GrpBtnH = 32;
        bool bGrpHover = (MouseX >= GrpBtnX && MouseX <= (GrpBtnX+GrpBtnW) && MouseY >= GrpBtnY && MouseY <= (GrpBtnY+GrpBtnH));

        if (bGrpHover && bLeftClickJustPressed) Robot->ToggleChassisGroup(SelectedIdx);
        FLinearColor GrpBg = bIsGrp ? FLinearColor(0.0f, 0.85f, 0.4f, 0.95f) : FLinearColor(0.2f, 0.25f, 0.38f, 0.85f);
        DrawRect(GrpBg, GrpBtnX, GrpBtnY, GrpBtnW, GrpBtnH);
        FCanvasTextItem GrpTxt(FVector2D(GrpBtnX+24, GrpBtnY+6), FText::FromString(bIsGrp ? TEXT("🔗 ANA GÖVDE (CHASSIS) GRUBUNDAN ÇIKAR") : TEXT("🔗 ANA GÖVDE (CHASSIS) GRUBUNA BİRLEŞTİR")), GEngine->GetSmallFont(), bIsGrp ? FLinearColor::Black : FLinearColor::White);
        Canvas->DrawItem(GrpTxt);
    }
    else if (Robot->CurrentViewMode == EGarageViewMode::Structural)
    {
        DrawRect(FLinearColor(0.9f, 0.5f, 0.0f, 0.7f), RightPanelX, 80, 410, 28);
        FCanvasTextItem StructTitle(FVector2D(RightPanelX + 12, 86), FText::FromString(TEXT("🦴 PARÇA FİZİK VE AERODİNAMİK DETAYLARI (STRUCTURAL)")), GEngine->GetSmallFont(), FLinearColor::Black);
        Canvas->DrawItem(StructTitle);

        FString ActiveMeshName = (Robot && Robot->SubMeshNames.IsValidIndex(SelectedIdx)) ? Robot->SubMeshNames[SelectedIdx] : TEXT("None");
        FString TargetInfo = FString::Printf(TEXT("SEÇİLEN PARÇA: [%d] %s"), SelectedIdx, *ActiveMeshName);
        FCanvasTextItem TargetText(FVector2D(RightPanelX + 16, 114), FText::FromString(TargetInfo), GEngine->GetSmallFont(), FLinearColor(0.0f, 0.94f, 1.0f, 1.0f));
        Canvas->DrawItem(TargetText);

        float MassVal = Robot->StructuralPropsList.IsValidIndex(SelectedIdx) ? Robot->StructuralPropsList[SelectedIdx].MassKg : 1.0f;
        float MassGrams = MassVal * 1000.0f;
        FString MassStr = FString::Printf(TEXT("KÜTLE (MASS - GRAM / KG): %.3f kg (%.0f gr)"), MassVal, MassGrams);
        FCanvasTextItem MassLbl(FVector2D(RightPanelX + 16, 138), FText::FromString(MassStr), GEngine->GetSmallFont(), FLinearColor::Yellow);
        Canvas->DrawItem(MassLbl);

        DrawInteractiveNumberBox(TEXT("AĞIRLIK"), TEXT("gr"), MassGrams, 1.0f, 100000.0f, 5.0f, RightPanelX + 14, 160, 380, 26, 100, MouseX, MouseY, bLeftClickJustPressed, PC);
        if (Robot->StructuralPropsList.IsValidIndex(SelectedIdx))
        {
            Robot->StructuralPropsList[SelectedIdx].MassKg = MassGrams / 1000.0f;
        }

        // Air Drag & Ground Friction with interactive text/drag boxes
        float AirDrag = Robot->StructuralPropsList.IsValidIndex(SelectedIdx) ? Robot->StructuralPropsList[SelectedIdx].AirDragCoeff : 0.3f;
        float GroundFric = Robot->StructuralPropsList.IsValidIndex(SelectedIdx) ? Robot->StructuralPropsList[SelectedIdx].GroundFriction : 0.8f;

        DrawInteractiveNumberBox(TEXT("HAVA SÜRTÜNME"), TEXT(""), AirDrag, 0.0f, 10.0f, 0.05f, RightPanelX + 14, 192, 185, 26, 101, MouseX, MouseY, bLeftClickJustPressed, PC);
        DrawInteractiveNumberBox(TEXT("KARA SÜRTÜNME"), TEXT(""), GroundFric, 0.0f, 10.0f, 0.05f, RightPanelX + 205, 192, 189, 26, 102, MouseX, MouseY, bLeftClickJustPressed, PC);

        if (Robot->StructuralPropsList.IsValidIndex(SelectedIdx))
        {
            Robot->StructuralPropsList[SelectedIdx].AirDragCoeff = AirDrag;
            Robot->StructuralPropsList[SelectedIdx].GroundFriction = GroundFric;
        }

        // Aerodynamic Lift Formula display & interactive boxes (F_lift = 0.5 * rho * V^2 * S * C_L)
        float LiftCoeff = Robot->StructuralPropsList.IsValidIndex(SelectedIdx) ? Robot->StructuralPropsList[SelectedIdx].LiftCoefficient : 0.0f;
        float WingSqM = Robot->StructuralPropsList.IsValidIndex(SelectedIdx) ? Robot->StructuralPropsList[SelectedIdx].WingAreaSqMeters : 0.25f;

        FString LiftFormulaStr = TEXT("LİFT FORMÜLÜ: F_lift = 0.5 * ρ * V² * S * C_L  (ρ: 1.225 kg/m³)");
        FCanvasTextItem LiftTxt(FVector2D(RightPanelX + 16, 224), FText::FromString(LiftFormulaStr), GEngine->GetSmallFont(), FLinearColor(0.8f, 0.95f, 1.0f, 1.0f));
        Canvas->DrawItem(LiftTxt);

        DrawInteractiveNumberBox(TEXT("C_L (Katsayı)"), TEXT(""), LiftCoeff, -10.0f, 10.0f, 0.05f, RightPanelX + 14, 244, 185, 26, 103, MouseX, MouseY, bLeftClickJustPressed, PC);
        DrawInteractiveNumberBox(TEXT("S (Kanat Alanı)"), TEXT("m²"), WingSqM, 0.0f, 100.0f, 0.1f, RightPanelX + 205, 244, 189, 26, 104, MouseX, MouseY, bLeftClickJustPressed, PC);

        if (Robot->StructuralPropsList.IsValidIndex(SelectedIdx))
        {
            Robot->StructuralPropsList[SelectedIdx].LiftCoefficient = LiftCoeff;
            Robot->StructuralPropsList[SelectedIdx].WingAreaSqMeters = WingSqM;
        }


        bool bIsGrp = Robot->StructuralPropsList.IsValidIndex(SelectedIdx) ? Robot->StructuralPropsList[SelectedIdx].bIsChassisGroup : false;
        float GrpBtnX = RightPanelX + 14; float GrpBtnY = 276; float GrpBtnW = 380; float GrpBtnH = 28;
        bool bGrpHover = (MouseX >= GrpBtnX && MouseX <= (GrpBtnX+GrpBtnW) && MouseY >= GrpBtnY && MouseY <= (GrpBtnY+GrpBtnH));

        if (bGrpHover && bLeftClickJustPressed) Robot->ToggleChassisGroup(SelectedIdx);
        FLinearColor GrpBg = bIsGrp ? FLinearColor(0.0f, 0.85f, 0.4f, 0.95f) : FLinearColor(0.2f, 0.25f, 0.38f, 0.85f);
        DrawRect(GrpBg, GrpBtnX, GrpBtnY, GrpBtnW, GrpBtnH);
        FCanvasTextItem GrpTxt(FVector2D(GrpBtnX+24, GrpBtnY+6), FText::FromString(bIsGrp ? TEXT("🔗 ANA GÖVDE (CHASSIS) GRUBUNDAN ÇIKAR") : TEXT("🔗 ANA GÖVDE (CHASSIS) GRUBUNA BİRLEŞTİR")), GEngine->GetSmallFont(), bIsGrp ? FLinearColor::Black : FLinearColor::White);
        Canvas->DrawItem(GrpTxt);
    }
    else if (Robot->CurrentViewMode == EGarageViewMode::Sensor)
    {
        DrawRect(FLinearColor(0.0f, 0.94f, 1.0f, 0.5f), RightPanelX, 80, 410, 28);
        FCanvasTextItem SensorTitle(FVector2D(RightPanelX + 12, 86), FText::FromString(TEXT("📡 SENSÖR PLACEHOLDER CANLI VERİ & LINE TRACE")), GEngine->GetSmallFont(), FLinearColor::Black);
        Canvas->DrawItem(SensorTitle);

        int32 SensIdx = ActiveGarageWidget ? ActiveGarageWidget->SelectedSensorIndex : 0;
        if (Robot->SensorsList.IsValidIndex(SensIdx))
        {
            FPiSimSensorDetail& Sens = Robot->SensorsList[SensIdx];

            FString SensHead = FString::Printf(TEXT("SENSÖR: [%d] %s  |  TİP: %s"), SensIdx, *Sens.SensorName, *Sens.SensorType);
            FCanvasTextItem SensHeadTxt(FVector2D(RightPanelX + 16, 120), FText::FromString(SensHead), GEngine->GetSmallFont(), FLinearColor::Yellow);
            Canvas->DrawItem(SensHeadTxt);

            float OnBtnX = RightPanelX + 16; float OnBtnY = 150; float OnBtnW = 378; float OnBtnH = 32;
            bool bOnHover = (MouseX >= OnBtnX && MouseX <= (OnBtnX+OnBtnW) && MouseY >= OnBtnY && MouseY <= (OnBtnY+OnBtnH));
            if (bOnHover && bLeftClickJustPressed && ActiveGarageWidget)
            {
                ActiveGarageWidget->ToggleSensorActive(SensIdx);
            }

            FLinearColor OnBg = Sens.bSensorActive ? FLinearColor(0.0f, 0.85f, 0.4f, 0.95f) : FLinearColor(0.85f, 0.15f, 0.15f, 0.95f);
            DrawRect(OnBg, OnBtnX, OnBtnY, OnBtnW, OnBtnH);

            FString OnLabel = FString::Printf(TEXT("DURUM: %s"), Sens.bSensorActive ? TEXT("🟢 SENSÖR AKTİF (ON)") : TEXT("🔴 SENSÖR KAPALI (OFF)"));
            FCanvasTextItem OnTxt(FVector2D(OnBtnX + 24, OnBtnY + 8), FText::FromString(OnLabel), GEngine->GetSmallFont(), FLinearColor::White);
            Canvas->DrawItem(OnTxt);

            FCanvasTextItem LiveDataHeadTxt(FVector2D(RightPanelX + 16, 195), FText::FromString(TEXT("📊 CANLI VERİ AKIŞI (LIVE TELEMETRY MONITOR):")), GEngine->GetSmallFont(), FLinearColor(0.0f, 0.94f, 1.0f, 1.0f));
            Canvas->DrawItem(LiveDataHeadTxt);

            DrawRect(FLinearColor(0.02f, 0.05f, 0.1f, 0.9f), RightPanelX + 16, 215, 378, 48);
            FCanvasTextItem DataTxt(FVector2D(RightPanelX + 24, 226), FText::FromString(Sens.bSensorActive ? Sens.LiveDataStream : TEXT("SENSÖR KAPALI (OFFLINE - VERİ YOK)")), GEngine->GetSmallFont(), Sens.bSensorActive ? FLinearColor::Green : FLinearColor::Red);
            Canvas->DrawItem(DataTxt);

            FString RayDistStr = FString::Printf(TEXT("• MESAFE (RAYCAST HIT DISTANCE): %.2f m"), Sens.RaycastHitDistance > 0 ? Sens.RaycastHitDistance / 100.0f : 0.0f);
            FString RayActStr = FString::Printf(TEXT("• ÇARPAN NESNE: %s"), *Sens.RaycastHitActorName);
            FCanvasTextItem RayDistTxt(FVector2D(RightPanelX + 16, 274), FText::FromString(RayDistStr), GEngine->GetSmallFont(), FLinearColor::White);
            Canvas->DrawItem(RayDistTxt);
            FCanvasTextItem RayActTxt(FVector2D(RightPanelX + 16, 296), FText::FromString(RayActStr), GEngine->GetSmallFont(), FLinearColor::White);
            Canvas->DrawItem(RayActTxt);
        }
        else
        {
            FCanvasTextItem EmptySensTxt(FVector2D(RightPanelX + 16, 130), FText::FromString(TEXT("S_ Öneki ile başlayan sensör placeholder meshi seçin.")), GEngine->GetSmallFont(), FLinearColor::White);
            Canvas->DrawItem(EmptySensTxt);
        }
    }

    // ---------------------------------------------------------
    // 3B. RIGHT PANEL - BOTTOM HALF: BAĞIMSIZ MOTORLAR VE SÜRÜCÜ AYARLARI
    // ---------------------------------------------------------
    if (Robot && Robot->CurrentViewMode != EGarageViewMode::Sensor)
    {
        float BottomSectionY = 346.0f;
        DrawRect(FLinearColor(0.03f, 0.05f, 0.09f, 0.93f), RightPanelX, BottomSectionY, 410, 355);
        DrawRect(FLinearColor(0.0f, 0.8f, 1.0f, 0.7f), RightPanelX, BottomSectionY, 410, 26);

        FCanvasTextItem MotorHeaderTxt(FVector2D(RightPanelX + 12, BottomSectionY + 5), FText::FromString(TEXT("⚡ MOTORLAR VE SÜRÜCÜ AYARLARI (PARÇADAN BAĞIMSIZ)")), GEngine->GetSmallFont(), FLinearColor::Black);
        Canvas->DrawItem(MotorHeaderTxt);

        int32 ActIdx = ActiveGarageWidget ? ActiveGarageWidget->SelectedActuatorIndex : 0;
        int32 ActCount = Robot ? Robot->ActuatorsList.Num() : 0;

        // [+ YENİ MOTOR EKLE] button in top-right of header
        float NewMotX = RightPanelX + 296;
        float NewMotY = BottomSectionY + 2;
        bool bNewMotHover = (MouseX >= NewMotX && MouseX <= (NewMotX+104) && MouseY >= NewMotY && MouseY <= (NewMotY+22));
        if (bNewMotHover && bLeftClickJustPressed && ActiveGarageWidget)
        {
            ActiveGarageWidget->AddNewMotor();
        }
        DrawRect(bNewMotHover ? FLinearColor(0.0f, 0.9f, 0.3f, 0.95f) : FLinearColor(0.0f, 0.65f, 0.25f, 0.9f), NewMotX, NewMotY, 104, 22);
        FCanvasTextItem NewMotTxt(FVector2D(NewMotX + 8, NewMotY + 4), FText::FromString(TEXT("+ YENİ MOTOR")), GEngine->GetSmallFont(), FLinearColor::Black);
        Canvas->DrawItem(NewMotTxt);

        // 16 Motor Select Buttons (M0 .. M15 in 2 rows of 8) - Inactive slots dimmed
        for (int32 a = 0; a < 16; ++a)
        {
            int32 Row = a / 8;
            int32 Col = a % 8;
            float ActBtnX = RightPanelX + 14 + (Col * 48);
            float ActBtnY = BottomSectionY + 32 + (Row * 24);
            bool bActHover = (MouseX >= ActBtnX && MouseX <= (ActBtnX+44) && MouseY >= ActBtnY && MouseY <= (ActBtnY+20));

            bool bIsSlotActive = (a < ActCount);
            if (bActHover && bLeftClickJustPressed && ActiveGarageWidget)
            {
                while (Robot->ActuatorsList.Num() <= a)
                {
                    Robot->AddNewMotorActuator();
                }
                ActiveGarageWidget->SelectedActuatorIndex = a;
            }

            bool bActSel = (a == ActIdx);
            FLinearColor BgColor = bActSel ? FLinearColor(0.0f, 0.94f, 1.0f, 0.9f) : (bIsSlotActive ? (bActHover ? FLinearColor(0.2f, 0.4f, 0.6f, 0.8f) : FLinearColor(0.12f, 0.18f, 0.28f, 0.9f)) : FLinearColor(0.06f, 0.08f, 0.11f, 0.45f));
            DrawRect(BgColor, ActBtnX, ActBtnY, 44, 20);

            FString ActLabel = FString::Printf(TEXT("M%d"), a);
            FLinearColor TxtColor = bActSel ? FLinearColor::Black : (bIsSlotActive ? FLinearColor::White : FLinearColor(0.45f, 0.5f, 0.55f, 0.6f));
            FCanvasTextItem ActBtnTxt(FVector2D(ActBtnX+10, ActBtnY+3), FText::FromString(ActLabel), GEngine->GetSmallFont(), TxtColor);
            Canvas->DrawItem(ActBtnTxt);
        }

        if (Robot && Robot->ActuatorsList.IsValidIndex(ActIdx))
        {
            FPiSimMotorActuator& Act = Robot->ActuatorsList[ActIdx];
            FString MotInfoStr = FString::Printf(TEXT("SEÇİLİ MOTOR: [%d] %s  |  HEDEF PARÇA: %s"), ActIdx, *Act.MotorName, *Act.TargetMeshName);
            FCanvasTextItem MotInfoTxt(FVector2D(RightPanelX + 16, BottomSectionY + 84), FText::FromString(MotInfoStr), GEngine->GetSmallFont(), FLinearColor::Yellow);
            Canvas->DrawItem(MotInfoTxt);

            // PWM KANAL AYARI (P0 .. P15)
            FCanvasTextItem PwmHeaderTxt(FVector2D(RightPanelX + 16, BottomSectionY + 106), FText::FromString(TEXT("🔌 PWM SANAL PORT KANALI ATAMA (P0 .. P15):")), GEngine->GetSmallFont(), FLinearColor::White);
            Canvas->DrawItem(PwmHeaderTxt);

            for (int32 p = 0; p < 16; ++p)
            {
                int32 Row = p / 8;
                int32 Col = p % 8;
                float PwmBtnX = RightPanelX + 14 + (Col * 48);
                float PwmBtnY = BottomSectionY + 128 + (Row * 24);
                bool bPwmHover = (MouseX >= PwmBtnX && MouseX <= (PwmBtnX + 44) && MouseY >= PwmBtnY && MouseY <= (PwmBtnY + 20));
                if (bPwmHover && bLeftClickJustPressed && ActiveGarageWidget)
                {
                    ActiveGarageWidget->SetMotorPwmChannel(p);
                }

                bool bIsPwmSel = (Act.AssignedPwmChannel == p);
                DrawRect(bIsPwmSel ? FLinearColor(0.0f, 0.94f, 0.4f, 0.9f) : (bPwmHover ? FLinearColor(0.2f, 0.4f, 0.6f, 0.8f) : FLinearColor(0.12f, 0.18f, 0.28f, 0.8f)), PwmBtnX, PwmBtnY, 44, 20);
                FString PwmStr = FString::Printf(TEXT("P%d"), p);
                FCanvasTextItem PwmTxt(FVector2D(PwmBtnX + 10, PwmBtnY + 3), FText::FromString(PwmStr), GEngine->GetSmallFont(), bIsPwmSel ? FLinearColor::Black : FLinearColor::White);
                Canvas->DrawItem(PwmTxt);
            }

            // BIND SELECTED MOTOR TO SELECTED MESH BUTTON -> OPENS MIDDLE-BOTTOM SIMULATION MODAL
            float BindBtnX = RightPanelX + 14;
            float BindBtnY = BottomSectionY + 190;
            float BindBtnW = 380;
            float BindBtnH = 34;
            bool bBindHover = (MouseX >= BindBtnX && MouseX <= (BindBtnX + BindBtnW) && MouseY >= BindBtnY && MouseY <= (BindBtnY + BindBtnH));
            if (bBindHover && bLeftClickJustPressed && ActiveGarageWidget)
            {
                ActiveGarageWidget->BindMotorToSelectedMesh();
            }

            DrawRect(bBindHover ? FLinearColor(0.9f, 0.7f, 0.0f, 0.95f) : FLinearColor(0.85f, 0.55f, 0.0f, 0.9f), BindBtnX, BindBtnY, BindBtnW, BindBtnH);
            FCanvasTextItem BindTxt(FVector2D(BindBtnX + 16, BindBtnY + 9),
                FText::FromString(TEXT("⚡ SEÇİLİ MOTORU SEÇİLİ MESHE BAĞLA VE ETKİSİNİ AYARLA")),
                GEngine->GetSmallFont(), FLinearColor::Black);
            Canvas->DrawItem(BindTxt);

        }
    }


    // ---------------------------------------------------------
    // 4. BOTTOM BAR - CAMERA CONTROLS & SHORTCUT HELP
    // ---------------------------------------------------------
    DrawRect(FLinearColor(0.03f, 0.05f, 0.09f, 0.94f), 0, Canvas->SizeY - 48, Canvas->SizeX, 48);

    FString CamHelp = TEXT("📷 ORBIT CAM: [Right Mouse Drag] 360° Rotate  |  [Mouse Wheel] Zoom In/Out  |  [S] Save Config");
    FCanvasTextItem CamHelpText(FVector2D(24, Canvas->SizeY - 32), FText::FromString(CamHelp), GEngine->GetSmallFont(), FLinearColor(0.8f, 0.9f, 1.0f, 1.0f));
    Canvas->DrawItem(CamHelpText);

    // ---------------------------------------------------------
    // 5. PHYSICS TEST MODE MODAL PANEL (IF ACTIVE / OPEN)
    // ---------------------------------------------------------
    if (ActiveGarageWidget && (ActiveGarageWidget->bIsTestPanelOpen || (Robot && Robot->CurrentPhysicsTestMode != EPhysicsTestMode::None)))
    {
        float ModalW = 540.0f;
        float ModalH = 96.0f;
        float ModalX = (Canvas->SizeX - ModalW) * 0.5f;
        float ModalY = Canvas->SizeY - 155.0f;

        DrawRect(FLinearColor(0.02f, 0.08f, 0.15f, 0.95f), ModalX, ModalY, ModalW, ModalH);
        DrawRect(FLinearColor(0.0f, 0.94f, 1.0f, 1.0f), ModalX, ModalY, ModalW, 4.0f);

        FCanvasTextItem TestTitle(FVector2D(ModalX + 16, ModalY + 12),
            FText::FromString(TEXT("🧪 FİZİKSEL ROBOT TEST VE SİMÜLASYON PANELİ (PHYSICS TEST MODE)")),
            GEngine->GetSmallFont(), FLinearColor::Yellow);
        Canvas->DrawItem(TestTitle);

        const TCHAR* TestBtnLabels[4] = {
            TEXT("✈️ HAVADA SABİT TUT"),
            TEXT("🔒 ROTASYONU SABİT TUT"),
            TEXT("🌊 HEPSİNİ SERBEST BIRAK"),
            TEXT("🔄 RESET ROBOT")
        };

        const EPhysicsTestMode ModeEnums[3] = {
            EPhysicsTestMode::HoldInAir,
            EPhysicsTestMode::LockRotation,
            EPhysicsTestMode::FreeSim
        };

        for (int32 tb = 0; tb < 4; ++tb)
        {
            float BtnX = ModalX + 16.0f + (tb * 128.0f);
            float BtnY = ModalY + 44.0f;
            float BtnW = 122.0f;
            float BtnH = 38.0f;

            bool bTbHover = (MouseX >= BtnX && MouseX <= (BtnX + BtnW) && MouseY >= BtnY && MouseY <= (BtnY + BtnH));

            if (bTbHover && bLeftClickJustPressed && ActiveGarageWidget)
            {
                if (tb < 3)
                {
                    ActiveGarageWidget->SelectPhysicsTestMode(ModeEnums[tb]);
                }
                else
                {
                    ActiveGarageWidget->ResetTestPose();
                }
            }

            bool bIsActiveMode = (tb < 3 && Robot && Robot->CurrentPhysicsTestMode == ModeEnums[tb]);
            FLinearColor BtnBg = bIsActiveMode ? FLinearColor(0.0f, 0.94f, 0.3f, 0.95f) : (bTbHover ? FLinearColor(0.2f, 0.45f, 0.7f, 0.9f) : FLinearColor(0.12f, 0.22f, 0.35f, 0.85f));
            FLinearColor TxtCol = bIsActiveMode ? FLinearColor::Black : FLinearColor::White;

            DrawRect(BtnBg, BtnX, BtnY, BtnW, BtnH);
            FCanvasTextItem BtnTxt(FVector2D(BtnX + 8, BtnY + 10), FText::FromString(TestBtnLabels[tb]), GEngine->GetSmallFont(), TxtCol);
            Canvas->DrawItem(BtnTxt);
        }
    }

    // ---------------------------------------------------------
    // 5.5 DEVKIT NUMPAD TEST CONTROL TELEMETRY PANEL
    // ---------------------------------------------------------
    if (Robot && ActiveGarageWidget)
    {
        float DevBoxX = 28.0f;
        float DevBoxY = Canvas->SizeY - 260.0f;
        float DevBoxW = 480.0f;
        float DevBoxH = 220.0f;

        DrawRect(FLinearColor(0.02f, 0.04f, 0.08f, 0.95f), DevBoxX, DevBoxY, DevBoxW, DevBoxH);
        DrawRect(FLinearColor(0.0f, 0.94f, 1.0f, 0.9f), DevBoxX, DevBoxY, DevBoxW, 26.0f);

        FCanvasTextItem DevTitle(FVector2D(DevBoxX + 10.0f, DevBoxY + 5.0f), FText::FromString(TEXT("🎮 DEVKIT NUMPAD TEST KONTROL PANELİ")), GEngine->GetSmallFont(), FLinearColor::Black);
        Canvas->DrawItem(DevTitle);

        const TCHAR* ServoAxisNames[3] = { TEXT("X-EKSENİ"), TEXT("Y-EKSENİ"), TEXT("Z-EKSENİ") };
        FString ServoAxisStr = ServoAxisNames[FMath::Clamp(Robot->DevKitServoAxis, 0, 2)];
        int32 DevSelectedIdx = ActiveGarageWidget->SelectedJointIndex;
        FString SelectedMeshStr = Robot->SubMeshNames.IsValidIndex(DevSelectedIdx) ? Robot->SubMeshNames[DevSelectedIdx] : TEXT("YOK");

        FString Line1 = FString::Printf(TEXT("🎯 HEDEF MESH: [%d] %s"), DevSelectedIdx, *SelectedMeshStr);
        FString Line2 = FString::Printf(TEXT("🔘 [Numpad 0] SERVO EKSENİ: %s"), *ServoAxisStr);
        FString Line3 = FString::Printf(TEXT("⚙️ [Numpad 1,2,3] SERVO AÇISI: [1]: MIN  [2]: ZERO  [3]: MAX"));
        FString Line4 = FString::Printf(TEXT("🔄 [Numpad 4,5,6] DÖNDÜRME RPM (X,Y,Z +10): (X:%.0f | Y:%.0f | Z:%.0f RPM)"), Robot->DevKitAppliedRpm.X, Robot->DevKitAppliedRpm.Y, Robot->DevKitAppliedRpm.Z);
        FString Line5 = FString::Printf(TEXT("🚀 [Numpad 7,8,9] KUVVET (X,Y,Z +10N): (X:%.0f | Y:%.0f | Z:%.0f N)"), Robot->DevKitAppliedForceN.X, Robot->DevKitAppliedForceN.Y, Robot->DevKitAppliedForceN.Z);

        FCanvasTextItem T1(FVector2D(DevBoxX + 12.0f, DevBoxY + 34.0f), FText::FromString(Line1), GEngine->GetSmallFont(), FLinearColor::Yellow);
        FCanvasTextItem T2(FVector2D(DevBoxX + 12.0f, DevBoxY + 56.0f), FText::FromString(Line2), GEngine->GetSmallFont(), FLinearColor(0.0f, 0.94f, 1.0f, 1.0f));
        FCanvasTextItem T3(FVector2D(DevBoxX + 12.0f, DevBoxY + 78.0f), FText::FromString(Line3), GEngine->GetSmallFont(), FLinearColor::White);
        FCanvasTextItem T4(FVector2D(DevBoxX + 12.0f, DevBoxY + 100.0f), FText::FromString(Line4), GEngine->GetSmallFont(), FLinearColor(0.0f, 1.0f, 0.5f, 1.0f));
        FCanvasTextItem T5(FVector2D(DevBoxX + 12.0f, DevBoxY + 122.0f), FText::FromString(Line5), GEngine->GetSmallFont(), FLinearColor(1.0f, 0.5f, 0.0f, 1.0f));

        Canvas->DrawItem(T1); Canvas->DrawItem(T2); Canvas->DrawItem(T3); Canvas->DrawItem(T4); Canvas->DrawItem(T5);

        // Reset Values Button
        float DevResetX = DevBoxX + 12.0f;
        float DevResetY = DevBoxY + 160.0f;
        float DevResetW = 456.0f;
        float DevResetH = 32.0f;
        bool bDevResetHover = (MouseX >= DevResetX && MouseX <= (DevResetX + DevResetW) && MouseY >= DevResetY && MouseY <= (DevResetY + DevResetH));
        if (bDevResetHover && bLeftClickJustPressed)
        {
            Robot->ResetDevKitTestValues();
        }
        FLinearColor DevResetBg = bDevResetHover ? FLinearColor(1.0f, 0.2f, 0.2f, 0.95f) : FLinearColor(0.6f, 0.1f, 0.1f, 0.85f);
        DrawRect(DevResetBg, DevResetX, DevResetY, DevResetW, DevResetH);
        FCanvasTextItem DevResetTxt(FVector2D(DevResetX + 75.0f, DevResetY + 8.0f), FText::FromString(TEXT("🧹 DEVKIT KUVVET VE RPM TEST DEĞERLERİNİ SIFIRLA")), GEngine->GetSmallFont(), FLinearColor::White);
        Canvas->DrawItem(DevResetTxt);
    }

    // ---------------------------------------------------------
    // 6. MOTOR ETKİ VE SİMÜLASYON MODAL PANELİ (MIDDLE-BOTTOM)
    // ---------------------------------------------------------
    // ---------------------------------------------------------
    // 6. MOTOR ETKİ VE SİMÜLASYON MODAL PANELİ (MIDDLE-BOTTOM)
    // ---------------------------------------------------------
    if (ActiveGarageWidget && ActiveGarageWidget->bIsMotorEffectModalOpen && Robot)
    {
        int32 ModalActIdx = ActiveGarageWidget->SelectedActuatorIndex;
        if (Robot->ActuatorsList.IsValidIndex(ModalActIdx))
        {
            FPiSimMotorActuator& Act = Robot->ActuatorsList[ModalActIdx];
            float ModalW = 760.0f;
            float ModalH = 320.0f;
            float ModalX = (Canvas->SizeX - ModalW) * 0.5f;
            float ModalY = Canvas->SizeY - 340.0f;

            DrawRect(FLinearColor(0.02f, 0.07f, 0.14f, 0.96f), ModalX, ModalY, ModalW, ModalH);
            DrawRect(FLinearColor(0.9f, 0.7f, 0.0f, 1.0f), ModalX, ModalY, ModalW, 4.0f);

            FString TitleStr = FString::Printf(TEXT("⚡ MOTOR VE FİZİKSEL TEST PANELİ — [%s] ---> HEDEF PARÇA: [%s]"), *Act.MotorName, *Act.TargetMeshName);
            FCanvasTextItem ModalTitle(FVector2D(ModalX + 16, ModalY + 12), FText::FromString(TitleStr), GEngine->GetSmallFont(), FLinearColor::Yellow);
            Canvas->DrawItem(ModalTitle);

            // Close button [ ✖ KAPAT ]
            float CloseBtnX = ModalX + ModalW - 90.0f;
            float CloseBtnY = ModalY + 8.0f;
            bool bCloseHover = (MouseX >= CloseBtnX && MouseX <= (CloseBtnX + 80) && MouseY >= CloseBtnY && MouseY <= (CloseBtnY + 24));
            if (bCloseHover && bLeftClickJustPressed)
            {
                ActiveGarageWidget->bIsMotorEffectModalOpen = false;
            }
            DrawRect(bCloseHover ? FLinearColor(0.9f, 0.2f, 0.2f, 0.9f) : FLinearColor(0.6f, 0.1f, 0.1f, 0.8f), CloseBtnX, CloseBtnY, 80, 24);
            FCanvasTextItem CloseTxt(FVector2D(CloseBtnX + 10, CloseBtnY + 4), FText::FromString(TEXT("✖ KAPAT")), GEngine->GetSmallFont(), FLinearColor::White);
            Canvas->DrawItem(CloseTxt);

            // 1) TOP SECTION: 3 MOTOR TYPES
            FCanvasTextItem TypeLbl(FVector2D(ModalX + 24, ModalY + 38), FText::FromString(TEXT("⚙️ MOTOR TİPİ SEÇİN (ROS PAKETİ İLE DOĞRUDAN SÜRÜLÜR):")), GEngine->GetSmallFont(), FLinearColor::Yellow);
            Canvas->DrawItem(TypeLbl);

            const TCHAR* MotorTypeNames[3] = {
                TEXT("⚙️ SERVO (Açısal Pozisyon)"),
                TEXT("🔄 SÜREKLİ DÖNEN (RPM Hız)"),
                TEXT("🚀 BLDC THRUST (Newton İtki)")
            };
            const EMotorBehaviorType MotorTypeCodes[3] = {
                EMotorBehaviorType::Servo,
                EMotorBehaviorType::ContinuousSpin,
                EMotorBehaviorType::Thruster
            };

            for (int32 mt = 0; mt < 3; ++mt)
            {
                float MtBtnX = ModalX + 24 + (mt * 240);
                float MtBtnY = ModalY + 56;
                bool bMtHover = (MouseX >= MtBtnX && MouseX <= (MtBtnX + 230) && MouseY >= MtBtnY && MouseY <= (MtBtnY + 26));

                if (bMtHover && bLeftClickJustPressed)
                {
                    Act.MotorType = MotorTypeCodes[mt];
                    if (Act.MotorType == EMotorBehaviorType::Servo || Act.MotorType == EMotorBehaviorType::ContinuousSpin)
                    {
                        Act.bAffectsRotation = true;
                        Act.bAffectsLiftForce = false;
                    }
                    else
                    {
                        Act.bAffectsRotation = false;
                        Act.bAffectsLiftForce = true;
                    }
                }

                bool bMtSel = (Act.MotorType == MotorTypeCodes[mt]);
                DrawRect(bMtSel ? FLinearColor(0.0f, 0.94f, 0.4f, 0.95f) : (bMtHover ? FLinearColor(0.2f, 0.45f, 0.7f, 0.85f) : FLinearColor(0.12f, 0.18f, 0.28f, 0.85f)), MtBtnX, MtBtnY, 230, 26);
                FCanvasTextItem MtTxt(FVector2D(MtBtnX + 10, MtBtnY + 5), FText::FromString(MotorTypeNames[mt]), GEngine->GetSmallFont(), bMtSel ? FLinearColor::Black : FLinearColor::White);
                Canvas->DrawItem(MtTxt);
            }

            // 2) SECOND SECTION: DÜZENLENECEK DURUM SEÇİMİ (-1 / 0 / +1) — SADECE KUTU HEDEFLERİNİ DEĞİŞTİRİR
            FCanvasTextItem StateLbl(FVector2D(ModalX + 24, ModalY + 94), FText::FromString(TEXT("🎯 DÜZENLENECEK DURUMU SEÇİN (-1 / 0 / +1):")), GEngine->GetSmallFont(), FLinearColor::Yellow);
            Canvas->DrawItem(StateLbl);

            const TCHAR* TestStateLabels[3] = { TEXT("[-1 DÜZENLE] (MIN)"), TEXT("[0 DÜZENLE] (MERKEZ)"), TEXT("[+1 DÜZENLE] (MAX)") };
            for (int32 ts = 0; ts < 3; ++ts)
            {
                float TsBtnX = ModalX + 24 + (ts * 240);
                float TsBtnY = ModalY + 114;
                bool bTsHover = (MouseX >= TsBtnX && MouseX <= (TsBtnX + 230) && MouseY >= TsBtnY && MouseY <= (TsBtnY + 26));
                int32 StateCode = (ts == 0) ? -1 : ((ts == 1) ? 0 : 1);

                if (bTsHover && bLeftClickJustPressed && ActiveGarageWidget)
                {
                    ActiveGarageWidget->ActiveMotorTestState = StateCode;
                }

                bool bTsSel = (ActiveGarageWidget->ActiveMotorTestState == StateCode);
                DrawRect(bTsSel ? FLinearColor(0.0f, 0.94f, 0.4f, 0.95f) : (bTsHover ? FLinearColor(0.2f, 0.45f, 0.7f, 0.85f) : FLinearColor(0.12f, 0.18f, 0.28f, 0.85f)), TsBtnX, TsBtnY, 230, 26);
                FCanvasTextItem TsTxt(FVector2D(TsBtnX + 16, TsBtnY + 5), FText::FromString(TestStateLabels[ts]), GEngine->GetSmallFont(), bTsSel ? FLinearColor::Black : FLinearColor::White);
                Canvas->DrawItem(TsTxt);
            }

            int32 TargetIdx = Act.TargetMeshIndex;
            int32 SelState = (ActiveGarageWidget) ? ActiveGarageWidget->ActiveMotorTestState : 0;
            FString StateStrName = (SelState == -1) ? TEXT("-1 (MIN)") : ((SelState == 1) ? TEXT("+1 (MAX)") : TEXT("0 (MERKEZ)"));

            // 3) THIRD SECTION: PARAMETER INPUT BOXES (SUPER CLEAN 3-COLUMN GRID)
            // ==========================================
            // PROTOKOL 1: SERVO (3 EKSENLİ AÇI & 3 EKSENLİ KUVVET/TORK)
            // ==========================================
            if (Act.MotorType == EMotorBehaviorType::Servo && Robot->JointLimitsList.IsValidIndex(TargetIdx))
            {
                FPiSimJointLimits& JL = Robot->JointLimitsList[TargetIdx];

                FString SecInfo = FString::Printf(TEXT("📐 SERVO [%s] DURUMU İÇİN 3 EKSENLİ AÇI (deg) VE 3 EKSENLİ TORK (Nm):"), *StateStrName);
                FCanvasTextItem Sec1(FVector2D(ModalX + 24, ModalY + 150), FText::FromString(SecInfo), GEngine->GetSmallFont(), FLinearColor(0.0f, 0.94f, 1.0f, 1.0f));
                Canvas->DrawItem(Sec1);

                FVector& EditEuler = (SelState == -1) ? JL.MinAngles3D : ((SelState == 1) ? JL.MaxAngles3D : JL.ZeroAngles3D);
                DrawInteractiveNumberBox(TEXT("AÇI X"), TEXT("deg"), EditEuler.X, -360.0f, 360.0f, 0.5f, ModalX + 24, ModalY + 170, 230, 26, 210, MouseX, MouseY, bLeftClickJustPressed, PC);
                DrawInteractiveNumberBox(TEXT("AÇI Y"), TEXT("deg"), EditEuler.Y, -360.0f, 360.0f, 0.5f, ModalX + 264, ModalY + 170, 230, 26, 211, MouseX, MouseY, bLeftClickJustPressed, PC);
                DrawInteractiveNumberBox(TEXT("AÇI Z"), TEXT("deg"), EditEuler.Z, -360.0f, 360.0f, 0.5f, ModalX + 504, ModalY + 170, 230, 26, 212, MouseX, MouseY, bLeftClickJustPressed, PC);

                if (!ActiveGarageWidget->bIsSweepingMotor)
                {
                    Robot->SetJointEulerAngles(TargetIdx, EditEuler);
                }

                FVector& EditForce = (SelState == -1) ? Act.AppliedForceAtMin : ((SelState == 1) ? Act.AppliedForceAtMax : Act.AppliedForceAtZero);
                DrawInteractiveNumberBox(TEXT("TORK X"), TEXT("Nm"), EditForce.X, -1000.0f, 1000.0f, 1.0f, ModalX + 24, ModalY + 202, 230, 26, 213, MouseX, MouseY, bLeftClickJustPressed, PC);
                DrawInteractiveNumberBox(TEXT("TORK Y"), TEXT("Nm"), EditForce.Y, -1000.0f, 1000.0f, 1.0f, ModalX + 264, ModalY + 202, 230, 26, 214, MouseX, MouseY, bLeftClickJustPressed, PC);
                DrawInteractiveNumberBox(TEXT("TORK Z"), TEXT("Nm"), EditForce.Z, -1000.0f, 1000.0f, 1.0f, ModalX + 504, ModalY + 202, 230, 26, 215, MouseX, MouseY, bLeftClickJustPressed, PC);
            }
            // ==========================================
            // PROTOKOL 2: SÜREKLİ DÖNEN (EKSEN X/Y/Z & HEDEF RPM)
            // ==========================================
            else if (Act.MotorType == EMotorBehaviorType::ContinuousSpin)
            {
                FString SecInfo = FString::Printf(TEXT("🔄 SÜREKLİ DÖNEN MOTOR [%s] DURUMU İÇİN DÖNÜŞ EKSENİ VE HEDEF RPM:"), *StateStrName);
                FCanvasTextItem Sec1(FVector2D(ModalX + 24, ModalY + 150), FText::FromString(SecInfo), GEngine->GetSmallFont(), FLinearColor(0.0f, 0.94f, 1.0f, 1.0f));
                Canvas->DrawItem(Sec1);

                DrawInteractiveNumberBox(TEXT("EKSEN X"), TEXT(""), Act.AppliedTorqueAxis.X, -1.0f, 1.0f, 0.05f, ModalX + 24, ModalY + 170, 230, 26, 221, MouseX, MouseY, bLeftClickJustPressed, PC);
                DrawInteractiveNumberBox(TEXT("EKSEN Y"), TEXT(""), Act.AppliedTorqueAxis.Y, -1.0f, 1.0f, 0.05f, ModalX + 264, ModalY + 170, 230, 26, 222, MouseX, MouseY, bLeftClickJustPressed, PC);
                DrawInteractiveNumberBox(TEXT("EKSEN Z"), TEXT(""), Act.AppliedTorqueAxis.Z, -1.0f, 1.0f, 0.05f, ModalX + 504, ModalY + 170, 230, 26, 223, MouseX, MouseY, bLeftClickJustPressed, PC);

                float RPMVal = Act.TestSliderValue * 1000.0f;
                DrawInteractiveNumberBox(TEXT("HEDEF RPM HIZI"), TEXT("RPM"), RPMVal, -10000.0f, 10000.0f, 10.0f, ModalX + 24, ModalY + 202, 360, 26, 224, MouseX, MouseY, bLeftClickJustPressed, PC);
                Act.TestSliderValue = FMath::Clamp(RPMVal / 1000.0f, -1.0f, 1.0f);
            }
            // ==========================================
            // PROTOKOL 3: BLDC THRUST (3 EKSENLİ İTKİ KUVVETİ NEWTON)
            // ==========================================
            else
            {
                FString SecInfo = FString::Printf(TEXT("🚀 BLDC İTKİ MOTORU [%s] DURUMU İÇİN 3 EKSENLİ İTKİ KUVVETİ (NEWTON):"), *StateStrName);
                FCanvasTextItem Sec1(FVector2D(ModalX + 24, ModalY + 150), FText::FromString(SecInfo), GEngine->GetSmallFont(), FLinearColor(0.0f, 0.94f, 1.0f, 1.0f));
                Canvas->DrawItem(Sec1);

                FVector& EditVec = (SelState == -1) ? Act.AppliedForceAtMin : ((SelState == 1) ? Act.AppliedForceAtMax : Act.AppliedForceAtZero);
                DrawInteractiveNumberBox(TEXT("KUVVET X"), TEXT("N"), EditVec.X, -10000.0f, 10000.0f, 1.0f, ModalX + 24, ModalY + 170, 230, 26, 231, MouseX, MouseY, bLeftClickJustPressed, PC);
                DrawInteractiveNumberBox(TEXT("KUVVET Y"), TEXT("N"), EditVec.Y, -10000.0f, 10000.0f, 1.0f, ModalX + 264, ModalY + 170, 230, 26, 232, MouseX, MouseY, bLeftClickJustPressed, PC);
                DrawInteractiveNumberBox(TEXT("KUVVET Z"), TEXT("N"), EditVec.Z, -10000.0f, 10000.0f, 1.0f, ModalX + 504, ModalY + 170, 230, 26, 233, MouseX, MouseY, bLeftClickJustPressed, PC);
            }

            bool bSwpActive = (ActiveGarageWidget && ActiveGarageWidget->bIsSweepingMotor);

            // 4) FOURTH SECTION: PHYSICAL TEST BUTTONS (-1, 0, +1 TEST SEÇENEKLERİ)
            if (Act.MotorType == EMotorBehaviorType::ContinuousSpin || Act.MotorType == EMotorBehaviorType::Thruster)
            {
                FCanvasTextItem TestLbl(FVector2D(ModalX + 24, ModalY + 236), FText::FromString(TEXT("🧪 FİZİKSEL TEST SEÇENEKLERİ (-1 / 0 / +1):")), GEngine->GetSmallFont(), FLinearColor::Yellow);
                Canvas->DrawItem(TestLbl);

                const float TestVals[3] = { -1.0f, 0.0f, +1.0f };
                const TCHAR* SpinTestLabels[3] = { TEXT("🧪 -1 TESTİ (TERS DÖNÜŞ)"), TEXT("🛑 0 TESTİ (BAŞLANGIÇA DÖN)"), TEXT("🧪 +1 TESTİ (İLERİ DÖNÜŞ)") };
                const TCHAR* ThrustTestLabels[3] = { TEXT("🧪 -1 TESTİ (TERS İTKİ)"), TEXT("🛑 0 TESTİ (SIFIR İTKİ)"), TEXT("🧪 +1 TESTİ (TAM İTKİ)") };
                const TCHAR** ActiveTestLabels = (Act.MotorType == EMotorBehaviorType::ContinuousSpin) ? SpinTestLabels : ThrustTestLabels;

                for (int32 tb = 0; tb < 3; ++tb)
                {
                    float TstBtnX = ModalX + 24 + (tb * 240);
                    float TstBtnY = ModalY + 256;
                    float TstBtnW = 230.0f;
                    float TstBtnH = 34.0f;
                    bool bTbHov = (MouseX >= TstBtnX && MouseX <= (TstBtnX + TstBtnW) && MouseY >= TstBtnY && MouseY <= (TstBtnY + TstBtnH));
                    int32 StateCode = (tb == 0) ? -1 : ((tb == 1) ? 0 : 1);

                    if (bTbHov && bLeftClickJustPressed && ActiveGarageWidget)
                    {
                        ActiveGarageWidget->bIsSweepingMotor = false;
                        Act.TestSliderValue = TestVals[tb];
                        if (StateCode == 0 && Act.MotorType == EMotorBehaviorType::ContinuousSpin && Robot && Robot->SubMeshComponents.IsValidIndex(TargetIdx))
                        {
                            if (Robot->SubMeshComponents[TargetIdx])
                            {
                                Robot->SubMeshComponents[TargetIdx]->SetRelativeRotation(FRotator::ZeroRotator);
                            }
                        }
                    }

                    bool bTbActive = (Act.TestSliderValue == TestVals[tb] && !ActiveGarageWidget->bIsSweepingMotor);
                    FLinearColor BtnCol = bTbActive ? FLinearColor(0.0f, 0.94f, 0.2f, 0.95f) : (bTbHov ? FLinearColor(0.2f, 0.45f, 0.7f, 0.9f) : FLinearColor(0.12f, 0.18f, 0.28f, 0.85f));
                    DrawRect(BtnCol, TstBtnX, TstBtnY, TstBtnW, TstBtnH);

                    FCanvasTextItem TbTxt(FVector2D(TstBtnX + 16, TstBtnY + 9), FText::FromString(ActiveTestLabels[tb]), GEngine->GetSmallFont(), bTbActive ? FLinearColor::Black : FLinearColor::White);
                    Canvas->DrawItem(TbTxt);
                }
            }

            else
            {
                // SERVO: FULL RANGE GİT-GEL SWEEP TESTİ + -1 / 0 / +1 AÇI TESTİ
                FCanvasTextItem TestLbl(FVector2D(ModalX + 24, ModalY + 236), FText::FromString(TEXT("🧪 SERVO FİZİKSEL TEST SEÇENEKLERİ (-1 / 0 / +1):")), GEngine->GetSmallFont(), FLinearColor::Yellow);
                Canvas->DrawItem(TestLbl);

                const float TestVals[3] = { -1.0f, 0.0f, +1.0f };
                const TCHAR* ServoTestLabels[3] = { TEXT("🧪 -1 AÇI TESTİ"), TEXT("🛑 0 MERKEZ TESTİ"), TEXT("🧪 +1 AÇI TESTİ") };
                for (int32 tb = 0; tb < 3; ++tb)
                {
                    float TstBtnX = ModalX + 24 + (tb * 170);
                    float TstBtnY = ModalY + 256;
                    float TstBtnW = 160.0f;
                    float TstBtnH = 34.0f;
                    bool bTbHov = (MouseX >= TstBtnX && MouseX <= (TstBtnX + TstBtnW) && MouseY >= TstBtnY && MouseY <= (TstBtnY + TstBtnH));
                    int32 StateCode = (tb == 0) ? -1 : ((tb == 1) ? 0 : 1);

                    if (bTbHov && bLeftClickJustPressed && ActiveGarageWidget)
                    {
                        ActiveGarageWidget->bIsSweepingMotor = false;
                        Act.TestSliderValue = TestVals[tb];
                        if (Robot && Robot->JointLimitsList.IsValidIndex(TargetIdx))
                        {
                            FPiSimJointLimits& JL = Robot->JointLimitsList[TargetIdx];
                            FVector TargetEuler = (StateCode == -1) ? JL.MinAngles3D : ((StateCode == 1) ? JL.MaxAngles3D : JL.ZeroAngles3D);
                            Robot->SetJointEulerAngles(TargetIdx, TargetEuler);
                        }
                    }

                    bool bTbActive = (Act.TestSliderValue == TestVals[tb] && !ActiveGarageWidget->bIsSweepingMotor);
                    FLinearColor BtnCol = bTbActive ? FLinearColor(0.0f, 0.94f, 0.2f, 0.95f) : (bTbHov ? FLinearColor(0.2f, 0.45f, 0.7f, 0.9f) : FLinearColor(0.12f, 0.18f, 0.28f, 0.85f));
                    DrawRect(BtnCol, TstBtnX, TstBtnY, TstBtnW, TstBtnH);

                    FCanvasTextItem TbTxt(FVector2D(TstBtnX + 14, TstBtnY + 9), FText::FromString(ServoTestLabels[tb]), GEngine->GetSmallFont(), bTbActive ? FLinearColor::Black : FLinearColor::White);
                    Canvas->DrawItem(TbTxt);
                }

                float SweepBtnX = ModalX + 534.0f;
                float SweepBtnY = ModalY + 256.0f;
                float SweepBtnW = 200.0f;
                float SweepBtnH = 34.0f;
                bool bSwpHov = (MouseX >= SweepBtnX && MouseX <= (SweepBtnX + SweepBtnW) && MouseY >= SweepBtnY && MouseY <= (SweepBtnY + SweepBtnH));

                if (bSwpHov && bLeftClickJustPressed && ActiveGarageWidget)
                {
                    ActiveGarageWidget->bIsSweepingMotor = !ActiveGarageWidget->bIsSweepingMotor;
                    ActiveGarageWidget->SweepMotorTimer = 0.0f;
                }

                DrawRect(bSwpActive ? FLinearColor(0.0f, 0.94f, 0.2f, 0.95f) : (bSwpHov ? FLinearColor(0.2f, 0.45f, 0.7f, 0.9f) : FLinearColor(0.15f, 0.22f, 0.35f, 0.9f)), SweepBtnX, SweepBtnY, SweepBtnW, SweepBtnH);
                FCanvasTextItem SwpTxt(FVector2D(SweepBtnX + 16, SweepBtnY + 9), FText::FromString(bSwpActive ? TEXT("🛑 GİT-GEL DURDUR") : TEXT("🔄 FULL RANGE TEST")), GEngine->GetSmallFont(), bSwpActive ? FLinearColor::Black : FLinearColor::Yellow);
                Canvas->DrawItem(SwpTxt);
            }

            // 5) RUN LIVE PHYSICAL SIMULATION & REAL-TIME 3D DEBUG ARROWS EVERY FRAME
            if (Robot)
            {
                // A) Full-range sweep animation between -1 .. 0 .. +1
                if (bSwpActive && ActiveGarageWidget)
                {
                    ActiveGarageWidget->SweepMotorTimer += GetWorld()->GetDeltaSeconds();
                    float SineVal = FMath::Sin(ActiveGarageWidget->SweepMotorTimer * 2.5f); // Smooth oscillation

                    if (Act.MotorType == EMotorBehaviorType::Servo && Robot->JointLimitsList.IsValidIndex(TargetIdx))
                    {
                        FPiSimJointLimits& JL = Robot->JointLimitsList[TargetIdx];
                        FVector SweepEuler = (SineVal < 0.0f) ? FMath::Lerp(JL.ZeroAngles3D, JL.MinAngles3D, -SineVal) : FMath::Lerp(JL.ZeroAngles3D, JL.MaxAngles3D, SineVal);
                        Robot->SetJointEulerAngles(TargetIdx, SweepEuler);
                    }
                    else
                    {
                        Act.TestSliderValue = SineVal;
                    }
                }

                // B) Continuous Spin: Physically spin component matrix in real time on GPU!
                if (Act.MotorType == EMotorBehaviorType::ContinuousSpin && Robot->SubMeshComponents.IsValidIndex(TargetIdx))
                {
                    UProceduralMeshComponent* SpinComp = Robot->SubMeshComponents[TargetIdx];
                    if (SpinComp)
                    {
                        float RPMVal = Act.TestSliderValue * 1000.0f;
                        if (FMath::Abs(RPMVal) > 0.1f)
                        {
                            float DegPerSec = RPMVal * 6.0f; // 1 RPM = 360 deg / 60s = 6 deg/s
                            float RotDelta = DegPerSec * GetWorld()->GetDeltaSeconds();
                            FVector Axis = Act.AppliedTorqueAxis.GetSafeNormal();
                            if (Axis.IsNearlyZero()) Axis = FVector(0, 1, 0);
                            FQuat DeltaQuat(Axis, FMath::DegreesToRadians(RotDelta));
                            SpinComp->AddLocalRotation(DeltaQuat);
                        }
                        else if (Act.TestSliderValue == 0.0f)
                        {
                            SpinComp->SetRelativeRotation(FRotator::ZeroRotator);
                        }
                    }
                }


                // C) BLDC Thruster: Draw 3D debug force arrow & apply physical thrust movement in real time!
                if (Act.MotorType == EMotorBehaviorType::Thruster && Robot->SubMeshComponents.IsValidIndex(TargetIdx))
                {
                    UProceduralMeshComponent* ThrComp = Robot->SubMeshComponents[TargetIdx];
                    if (ThrComp)
                    {
                        FVector StartPos = ThrComp->GetComponentLocation();
                        FVector ActiveForceVec = (Act.TestSliderValue < -0.1f) ? Act.AppliedForceAtMin : ((Act.TestSliderValue > 0.1f) ? Act.AppliedForceAtMax : Act.AppliedForceAtZero);
                        if (bSwpActive)
                        {
                            float SineVal = FMath::Sin(ActiveGarageWidget->SweepMotorTimer * 2.5f);
                            ActiveForceVec = (SineVal < 0.0f) ? FMath::Lerp(Act.AppliedForceAtZero, Act.AppliedForceAtMin, -SineVal) : FMath::Lerp(Act.AppliedForceAtZero, Act.AppliedForceAtMax, SineVal);
                        }
                        FVector EndPos = StartPos + ActiveForceVec * 3.0f;
                        DrawDebugLine(GetWorld(), StartPos, EndPos, FColor::Orange, false, -1.0f, 0, 4.0f);
                        DrawDebugSphere(GetWorld(), EndPos, 12.0f, 12, FColor::Yellow, false, -1.0f);

                        if (ActiveForceVec.SizeSquared() > 0.01f)
                        {
                            if (ThrComp->IsSimulatingPhysics())
                            {
                                ThrComp->AddForce(ActiveForceVec * 1000.0f, NAME_None, true);
                            }
                            else
                            {
                                // Kinematic Garage Mode: Physically push/translate the robot in 3D space along the thrust vector!
                                FVector DeltaLoc = ActiveForceVec * GetWorld()->GetDeltaSeconds() * 2.0f;
                                Robot->AddActorWorldOffset(DeltaLoc, true);
                            }
                        }
                    }
                }

            }

        }
    }
}


void APiSimHUD::DrawInteractiveNumberBox(const FString& LabelText, const FString& UnitsText, float& Value, float MinVal, float MaxVal, float DragSensitivity, float BoxX, float BoxY, float BoxW, float BoxH, int32 BoxID, float MouseX, float MouseY, bool bJustPressed, APlayerController* PC)
{
    if (!Canvas || !GEngine) return;

    bool bHover = (MouseX >= BoxX && MouseX <= (BoxX + BoxW) && MouseY >= BoxY && MouseY <= (BoxY + BoxH));
    bool bIsThisActive = (ActiveInputBoxID == BoxID);

    if (bHover && bJustPressed)
    {
        ActiveInputBoxID = BoxID;
        TypedInputString = FString::SanitizeFloat(Value);
        bIsThisActive = true;
    }

    if (bIsThisActive && PC)
    {
        bool bChangedText = false;
        const FKey NumKeys[10] = { EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
        const FKey NumPadKeys[10] = { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
        for (int32 d = 0; d < 10; ++d)
        {
            if (PC->WasInputKeyJustPressed(NumKeys[d]) || PC->WasInputKeyJustPressed(NumPadKeys[d]))
            {
                TypedInputString.AppendChar((TCHAR)('0' + d));
                bChangedText = true;
            }
        }
        if (PC->WasInputKeyJustPressed(EKeys::Period) || PC->WasInputKeyJustPressed(EKeys::Decimal))
        {
            if (!TypedInputString.Contains(TEXT("."))) { TypedInputString.AppendChar('.'); bChangedText = true; }
        }
        if (PC->WasInputKeyJustPressed(EKeys::Hyphen) || PC->WasInputKeyJustPressed(EKeys::Subtract))
        {
            if (TypedInputString.StartsWith(TEXT("-"))) TypedInputString.RemoveFromStart(TEXT("-"));
            else TypedInputString = TEXT("-") + TypedInputString;
            bChangedText = true;
        }
        if (PC->WasInputKeyJustPressed(EKeys::BackSpace))
        {
            if (TypedInputString.Len() > 0)
            {
                TypedInputString.RemoveAt(TypedInputString.Len() - 1);
                bChangedText = true;
            }
        }
        if (PC->WasInputKeyJustPressed(EKeys::Enter) || PC->WasInputKeyJustPressed(EKeys::Escape) || (bJustPressed && !bHover))
        {
            ActiveInputBoxID = -1;
            bIsThisActive = false;
        }
        if (bChangedText && TypedInputString.Len() > 0)
        {
            Value = FMath::Clamp(FCString::Atof(*TypedInputString), MinVal, MaxVal);
        }
    }

    FLinearColor BgColor = bIsThisActive ? FLinearColor(0.1f, 0.45f, 0.75f, 0.95f) : (bHover ? FLinearColor(0.18f, 0.28f, 0.4f, 0.9f) : FLinearColor(0.11f, 0.16f, 0.24f, 0.85f));
    DrawRect(BgColor, BoxX, BoxY, BoxW, BoxH);
    if (bIsThisActive)
    {
        DrawRect(FLinearColor(0.0f, 0.94f, 1.0f, 1.0f), BoxX, BoxY, BoxW, 2.0f);
        DrawRect(FLinearColor(0.0f, 0.94f, 1.0f, 1.0f), BoxX, BoxY + BoxH - 2.0f, BoxW, 2.0f);
    }

    FString DisplayStr = TEXT("");
    FString CleanUnits = (UnitsText.Len() > 0) ? FString::Printf(TEXT(" %s"), *UnitsText) : TEXT("");
    if (bIsThisActive)
    {
        DisplayStr = FString::Printf(TEXT("%s:  %s_%s"), *LabelText, *TypedInputString, *CleanUnits);
    }
    else
    {
        DisplayStr = FString::Printf(TEXT("%s:  %.2f%s"), *LabelText, Value, *CleanUnits);
    }


    FCanvasTextItem TxtItem(FVector2D(BoxX + 10.0f, BoxY + (BoxH * 0.5f) - 7.0f), FText::FromString(DisplayStr), GEngine->GetSmallFont(), bIsThisActive ? FLinearColor::Yellow : FLinearColor::White);
    Canvas->DrawItem(TxtItem);
}

void APiSimHUD::DrawInteractiveNumberBox(const FString& LabelText, const FString& UnitsText, double& Value, float MinVal, float MaxVal, float DragSensitivity, float BoxX, float BoxY, float BoxW, float BoxH, int32 BoxID, float MouseX, float MouseY, bool bJustPressed, APlayerController* PC)
{
    float TempVal = (float)Value;
    DrawInteractiveNumberBox(LabelText, UnitsText, TempVal, MinVal, MaxVal, DragSensitivity, BoxX, BoxY, BoxW, BoxH, BoxID, MouseX, MouseY, bJustPressed, PC);
    Value = (double)TempVal;
}




