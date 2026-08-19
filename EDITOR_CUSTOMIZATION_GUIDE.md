# 🛠️ PiSim Editör Özelleştirme & Kilit Rehberi (Editor Customization Guide)

Bu belge, **PiSim** projesinde son kullanıcı için basitleştirilen, gizlenen veya kilitlenen tüm Unreal Editor arayüz öğelerinin **tam listesini**, nerede tanımlandıklarını ve **nasıl geri açılacaklarını (Unlock)** içerir.

---

## 📋 Aktif Kilit ve Gizleme Listesi

| # | Gizlenen / Kilitlenen Öğe | Açık Bırakılan Kısım | Tanımlandığı Dosya | Nasıl Geri Açılır (Unlock)? |
| :--- | :--- | :--- | :--- | :--- |
| **1** | **Content Browser Sağ Tık Menüsü Alt Kategorileri**<br>• Niagara / FX<br>• Animation, AI, Audio, Blueprint<br>• Cinematics, Foliage, Gameplay<br>• Input, Media, Paper2D, Physics, UI vb. | **🎯 Temel Varlıklar:**<br>• Folder<br>• Animation Blueprint<br>• Blueprint Class<br>• Level<br>• Level Sequence<br>• **Material (En altta)** | [`Source/PiSim/PiSim.cpp`](file:///c:/PiSim/Source/PiSim/PiSim.cpp) | `CategoriesToRemove` listesinden istediğiniz kategorinin adını silin veya `//` ile yorum satırı yapın. |
| **2** | **Viewport Toolbar (Sol & Sağ Butonlar)**<br>• Taşıma/Döndürme/Ölçekleme Gizmo Butonları<br>• Izgara / Açı / Snap Butonları (0, 10, 10°, 0.25)<br>• Kamera Hızı (20.7) ve Ayar Butonları | **🎯 Kamera / Görünüm Modu Seçici**<br>*(Player Collision / Lit / Unlit / Wireframe)* | [`Source/PiSim/PiSim.cpp`](file:///c:/PiSim/Source/PiSim/PiSim.cpp) & [`Config/DefaultEditorPerProjectUserSettings.ini`](file:///c:/PiSim/Config/DefaultEditorPerProjectUserSettings.ini) | `PiSim.cpp` içindeki `VpToolbarLeft->RemoveSection(...)` satırlarını kaldırın. |

---

## 📂 Kod Seviyesinde Detaylar ve Nasıl Çalışır?

### 1. Neden `.ini` Yerine C++ `UToolMenus` Kullanıyoruz?
* **Problem:** Unreal Engine, kullanıcının bilgisayarında `Saved/Config/` adında yerel bir önbellek tutar ve `.ini` dosyalarındaki değişiklikler bu önbellek tarafından ezilebilir.
* **C++ Çözümü:** `Source/PiSim/PiSim.cpp` içinde çalışan `UToolMenus` kodu, **projeyi hangi kullanıcı hangi bilgisayarda açarsa açsın** editör başlarken arayüzü doğrudan bellekte ameliyat gibi keser ve sadeleştirir.

---

### 2. Sağ Tık Menüsünü Özelleştirme
* **Dosya:** [`Source/PiSim/PiSim.cpp`](file:///c:/PiSim/Source/PiSim/PiSim.cpp)
* **İlgili Fonksiyon:** `FPiSimModule::CustomizeEditorToolbarsAndMenus()`
* **Kod:**
  ```cpp
  const TCHAR* CategoriesToRemove[] = {
      TEXT("Animation"),
      TEXT("Artificial Intelligence"),
      TEXT("Audio"),
      TEXT("Blueprint"),
      TEXT("Cinematics"),
      TEXT("Editor Utilities"),
      TEXT("Foliage"),
      TEXT("FX"),
      TEXT("Gameplay"),
      TEXT("Input"),
      TEXT("Interchange"),
      TEXT("Media"),
      TEXT("Miscellaneous"),
      TEXT("Paper2D"),
      TEXT("Physics"),
      TEXT("Texture"),
      TEXT("Tool Presets"),
      TEXT("User Interface"),
      TEXT("World")
  };
  ```

---

*Bu dosya yeni arayüz öğeleri gizlendikçe/özelleştirildikçe adım adım güncellenecektir.*
