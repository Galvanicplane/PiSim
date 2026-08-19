# 🛠️ PiSim Editör Özelleştirme & Kilit Rehberi (Editor Customization Guide)

Bu belge, **PiSim** projesinde son kullanıcı için basitleştirilen, gizlenen veya kilitlenen tüm Unreal Editor arayüz öğelerinin **tam listesini**, nerede tanımlandıklarını ve **nasıl geri açılacaklarını** içerir.

---

## 📋 Aktif Kilit ve Gizleme Listesi

| # | Gizlenen / Kilitlenen Öğe | Açık Bırakılan Kısım | Tanımlandığı Dosya | Nasıl Geri Açılır (Unlock)? |
| :--- | :--- | :--- | :--- | :--- |
| **1** | **FX / Niagara / Partikül Sistemleri** | Standart meshler ve temel aktörler | `PiSim.uproject` | `Niagara` ve `NiagaraFluids` satırlarındaki `"Enabled": false` değerini `true` yapın veya silin. |
| **2** | **Viewport Toolbar (Sol & Sağ Araçlar)**<br>• Taşıma/Döndürme/Ölçekleme Gizmo Butonları<br>• Izgara / Açı / Snap Butonları (0, 10, 10°, 0.25)<br>• Kamera Hızı (20.7) ve Ayar Butonları | **🎯 Kamera / Görünüm Modu Seçici**<br>*(Player Collision / Lit / Unlit / Wireframe)* | `Config/DefaultEditorPerProjectUserSettings.ini` | `bDrawGizmos`, `bDrawSnappingToolbar`, `bDrawCameraSpeed` değerlerini `True` yapın. |

---

## 📂 Dosya Bazında Detaylar ve Kodlar

### 1. Partiküllerin Sağ Tık Menüsünden Gizlenmesi
* **Dosya:** [`PiSim.uproject`](file:///c:/PiSim/PiSim.uproject)
* **Ayar:**
  ```json
  {
      "Name": "Niagara",
      "Enabled": false
  },
  {
      "Name": "NiagaraFluids",
      "Enabled": false
  }
  ```
* **Geri Açma:** `Enabled: true` yapıldığında veya bu bloklar kaldırıldığında partiküller anında geri gelir.

---

### 2. Viewport Toolbar Sadeleştirmesi
* **Dosya:** [`Config/DefaultEditorPerProjectUserSettings.ini`](file:///c:/PiSim/Config/DefaultEditorPerProjectUserSettings.ini)
* **Ayar:**
  ```ini
  [/Script/UnrealEd.LevelEditorViewportSettings]
  bDrawGizmos=False
  bDrawSnappingToolbar=False
  bDrawCameraSpeed=False
  bShowViewportControls=False
  ```
* **Geri Açma:** İlgili satırı `True` yapmanız veya satırı silmeniz yeterlidir.

---

*Bu dosya yeni arayüz öğeleri gizlendikçe/özelleştirildikçe adım adım güncellenecektir.*
