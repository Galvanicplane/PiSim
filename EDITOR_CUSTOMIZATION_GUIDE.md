# 🛠️ PiSim Editör Özelleştirme & Kilit Rehberi (Editor Customization Guide)

Bu belge, **PiSim** projesinde son kullanıcı için basitleştirilen, gizlenen veya kilitlenen tüm Unreal Editor arayüz öğelerinin **tam listesini**, nerede tanımlandıklarını ve **nasıl geri açılacaklarını (Unlock)** içerir.

---

## 📋 Aktif Kilit ve Gizleme Listesi

| # | Gizlenen / Kilitlenen Öğe | Açık Bırakılan Kısım | Tanımlandığı Dosya | Nasıl Geri Açılır (Unlock)? |
| :--- | :--- | :--- | :--- | :--- |
| **1** | **`APiSimPrimitiveCube` İçin Blueprint / Event Graph Kilidi**<br>`NotBlueprintable, NotBlueprintType` ile Blueprint oluşturulması ve Event Graph'a girilmesi kökten kilitlendi. | **🎯 Sahne Yerleşimi & Bileşenler:**<br>• Sahneye doğrudan yerleştirilebilir (`Placeable`)<br>• 3D Viewport'ta serbestçe taşınır/ölçeklenir<br>• Details panelinden **+ Add Component** ile parça eklenir<br>• Details panelinden tüm değerler girilir | [`Source/PiSim/PiSimPrimitiveCube.h`](file:///c:/PiSim/Source/PiSim/PiSimPrimitiveCube.h) | `UCLASS(NotBlueprintable, NotBlueprintType, Placeable)` satırını `UCLASS(Blueprintable)` olarak değiştirin. |
| **2** | **Details Paneli Kategorileri**<br>• **Lighting** (Işıklandırma)<br>• Rendering, HLOD, Navigation<br>• Replication, Input, ActorTick<br>• LOD, Cooking, WorldPartition | **🎯 Sadece Simülasyon & Robot Ayarları:**<br>• Transform (Konum/Rotasyon)<br>• PiSim Simülasyon Parametreleri | [`Source/PiSim/PiSim.cpp`](file:///c:/PiSim/Source/PiSim/PiSim.cpp) | `FPiSimRobotDetailCustomization::CustomizeDetails` içinde ilgili `DetailBuilder.HideCategory(...)` satırını silin veya `//` yapın. |
| **3** | **Content Browser Sağ Tık Menüsü Alt Kategorileri**<br>• Niagara / FX<br>• Animation, AI, Audio, Blueprint<br>• Cinematics, Foliage, Gameplay<br>• Input, Media, Paper2D, Physics, UI vb. | **🎯 Temel Varlıklar:**<br>• Folder<br>• Animation Blueprint<br>• Blueprint Class<br>• Level<br>• Level Sequence<br>• **Material (En altta)** | [`Source/PiSim/PiSim.cpp`](file:///c:/PiSim/Source/PiSim/PiSim.cpp) | `CategoriesToRemove` listesinden istediğiniz kategorinin adını silin veya `//` yapın. |
| **4** | **Viewport Toolbar (Sol & Sağ Butonlar)**<br>• Taşıma/Döndürme/Ölçekleme Gizmo Butonları<br>• Izgara / Açı / Snap Butonları (0, 10, 10°, 0.25)<br>• Kamera Hızı (20.7) ve Ayar Butonları | **🎯 Kamera / Görünüm Modu Seçici**<br>*(Player Collision / Lit / Unlit / Wireframe)* | [`Source/PiSim/PiSim.cpp`](file:///c:/PiSim/Source/PiSim/PiSim.cpp) & [`Config/DefaultEditorPerProjectUserSettings.ini`](file:///c:/PiSim/Config/DefaultEditorPerProjectUserSettings.ini) | `PiSim.cpp` içindeki `VpToolbarLeft->RemoveSection(...)` satırlarını kaldırın. |

---

## 📂 Dosya Bazında Kod Detayları

### 1. `APiSimPrimitiveCube` - Event Graph Kilidi & Sahne Yerleşimi
* **Dosya:** [`Source/PiSim/PiSimPrimitiveCube.h`](file:///c:/PiSim/Source/PiSim/PiSimPrimitiveCube.h)
* **Kod:**
  ```cpp
  UCLASS(NotBlueprintable, NotBlueprintType, Placeable)
  class PISIM_API APiSimPrimitiveCube : public AActor
  ```
* **Amaç:** Kullanıcının Blueprint açıp node bağlamasını kesin olarak engeller. Kullanıcı küpü sahneye koyar, **+ Add Component** ile bileşen ekler, 3D sahnede konumlandırır ve Details panelinden parametrelerini girer.

---

*Bu dosya yeni arayüz öğeleri gizlendikçe/özelleştirildikçe adım adım güncellenecektir.*
