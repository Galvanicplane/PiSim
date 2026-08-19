# 🛠️ PiSim Editör Özelleştirme & Kilit Rehberi (Editor Customization Guide)

Bu belge, **PiSim** projesinde son kullanıcı için basitleştirilen, gizlenen veya kilitlenen tüm Unreal Editor arayüz öğelerinin **tam listesini**, nerede tanımlandıklarını ve **nasıl geri açılacaklarını (Unlock)** içerir.

---

## 📋 Aktif Kilit ve Gizleme Listesi

| # | Gizlenen / Kilitlenen Öğe | Açık Bırakılan Kısım | Tanımlandığı Dosya | Nasıl Geri Açılır (Unlock)? |
| :--- | :--- | :--- | :--- | :--- |
| **1** | **Details Paneli Kategorileri**<br>• **Lighting** (Işıklandırma)<br>• Rendering, HLOD, Navigation<br>• Replication, Input, ActorTick<br>• LOD, Cooking, WorldPartition | **🎯 Sadece Simülasyon & Robot Ayarları:**<br>• Transform (Konum/Rotasyon)<br>• PiSim Simülasyon Parametreleri | [`Source/PiSim/PiSim.cpp`](file:///c:/PiSim/Source/PiSim/PiSim.cpp) | `FPiSimRobotDetailCustomization::CustomizeDetails` içinde ilgili `DetailBuilder.HideCategory(...)` satırını silin veya `//` yapın. |
| **2** | **Content Browser Sağ Tık Menüsü Alt Kategorileri**<br>• Niagara / FX<br>• Animation, AI, Audio, Blueprint<br>• Cinematics, Foliage, Gameplay<br>• Input, Media, Paper2D, Physics, UI vb. | **🎯 Temel Varlıklar:**<br>• Folder<br>• Animation Blueprint<br>• Blueprint Class<br>• Level<br>• Level Sequence<br>• **Material (En altta)** | [`Source/PiSim/PiSim.cpp`](file:///c:/PiSim/Source/PiSim/PiSim.cpp) | `CategoriesToRemove` listesinden istediğiniz kategorinin adını silin veya `//` yapın. |
| **3** | **Viewport Toolbar (Sol & Sağ Butonlar)**<br>• Taşıma/Döndürme/Ölçekleme Gizmo Butonları<br>• Izgara / Açı / Snap Butonları (0, 10, 10°, 0.25)<br>• Kamera Hızı (20.7) ve Ayar Butonları | **🎯 Kamera / Görünüm Modu Seçici**<br>*(Player Collision / Lit / Unlit / Wireframe)* | [`Source/PiSim/PiSim.cpp`](file:///c:/PiSim/Source/PiSim/PiSim.cpp) & [`Config/DefaultEditorPerProjectUserSettings.ini`](file:///c:/PiSim/Config/DefaultEditorPerProjectUserSettings.ini) | `PiSim.cpp` içindeki `VpToolbarLeft->RemoveSection(...)` satırlarını kaldırın. |

---

## 📂 Dosya Bazında Detaylar ve Nasıl Çalışır?

### 1. Details Panelinde `Lighting` ve Gereksiz Motor Kategorilerini Gizleme
* **Dosya:** [`Source/PiSim/PiSim.cpp`](file:///c:/PiSim/Source/PiSim/PiSim.cpp)
* **İlgili Sınıf:** `FPiSimRobotDetailCustomization`
* **Kod:**
  ```cpp
  DetailBuilder.HideCategory("Lighting");
  DetailBuilder.HideCategory("Rendering");
  DetailBuilder.HideCategory("HLOD");
  DetailBuilder.HideCategory("Navigation");
  DetailBuilder.HideCategory("Replication");
  DetailBuilder.HideCategory("Input");
  DetailBuilder.HideCategory("ActorTick");
  DetailBuilder.HideCategory("LOD");
  DetailBuilder.HideCategory("Cooking");
  DetailBuilder.HideCategory("DataLayers");
  DetailBuilder.HideCategory("WorldPartition");
  ```

---

*Bu dosya yeni arayüz öğeleri gizlendikçe/özelleştirildikçe adım adım güncellenecektir.*
