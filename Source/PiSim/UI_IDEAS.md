# PiSim - Modern UI Tasarım Fikirleri ve Gösterge Kataloğu (UI Ideas)

Bu belge, PiSim projesinin kullanıcı arayüzünü (UI) hantal C++ Slate kodlamasından kurtarıp, **modern, profesyonel, havacılık standartlarında ve tasarımcı dostu** bir yapıya kavuşturmak için izlenebilecek yöntemleri, araçları ve hazır bileşen linklerini içerir.

> **Derleme Notu:** Bu dosya bir Markdown (`.md`) dokümanıdır. Unreal Engine derleyicisi (UnrealBuildTool - UBT) `.md` uzantılı dosyaları derlemeye dahil etmez, tamamen yok sayar. Projenin hiçbir yerinde derleme hatasına veya yavaşlamaya **kesinlikle neden olmaz.**

---

## 1. Temel Yaklaşım: 3 Tasarım Yolu

### Yöntem A: Web Tabanlı UI + Unreal WebBrowser Eklentisi (En Çok Tavsiye Edilen 🚀)
SpaceX Dragon kapsülü, modern F-35 simülatörleri ve havacılık yer istasyonlarının kullandığı mimaridir.
* **Nasıl Çalışır?** Arayüz HTML5, CSS3, JavaScript (veya React/Vue/Svelte) ile bir web sayfası gibi tasarlanır. Unreal Engine içindeki yerleşik `WebBrowser` (Chromium tabanlı) eklentisiyle oyun ekranına gömülür.
* **C++ Entegrasyonu:** Unreal C++ telemetrisi (hız, irtifa, açılar), arka planda yerel WebSocket veya JSON köprüsü üzerinden bu web sayfasına canlı olarak aktarılır (60+ FPS).
* **Avantajı:** İnternetteki binlerce modern CSS/JS kütüphanesini, animasyonu, cam efektlerini (glassmorphism) doğrudan kullanabilirsiniz. Kod derlemesi beklemeden tarayıcıda anında test edilebilir.

### Yöntem B: Figma / Mockup Ekran Görüntüsü ile Tasarım
* Beğendiğiniz bir kokpit, yer istasyonu veya arayüzün ekran görüntüsünü (PNG/JPG) ya da Figma linkini paylaşabilirsiniz.
* Görsel piksel piksel incelenerek renk paleti, buton kıvrımları, tipografi ve yerleşim birebir koda dökülür.

### Yöntem C: Unreal Engine UMG (Görsel Sürükle-Bırak)
* C++ kodları yerine Unreal Editor içindeki **Widget Blueprint (UMG)** görsel tasarımcısı kullanılır.
* Butonlar, paneller ve resimler fareyle sürükle-bırak yapılarak tasarlanır; arkasındaki veri bağlamalarını (binding) C++ üstlenir.

---

## 2. Hazır Havacılık Göstergeleri Kataloğu (Open Source & Linkler)

Arayüzde sıfırdan çizim yapmak yerine havacılık endüstrisinde standartlaşmış, doğrudan kullanabileceğimiz açık kaynaklı göstergeler:

### 1. Flight Indicators JS (En Popüler Uçak Göstergeleri)
* **Açıklama:** HTML5, SVG ve Canvas tabanlı, tamamen gerçekçi ve animasyonlu 6 temel uçuş göstergesi (Six-Pack).
* **İçerik:**
  * **Attitude Indicator:** Yapay ufuk (Roll & Pitch).
  * **Heading Indicator:** 360° Döner pusula gülü.
  * **Airspeed Indicator:** İbreli hava hızı göstergesi (Knots / Km/h).
  * **Altimeter:** Deniz seviyesi irtifa göstergesi (Feet / Metre).
  * **Turn Coordinator:** Dönüş koordinatörü ve kayış bilyesi.
  * **Variometer:** Dikey tırmanış/alçalış hızı göstergesi.
* **GitHub Deposu:** [ignaciovr/flight-indicators-js](https://github.com/ignaciovr/flight-indicators-js)
* **Canlı Demo:** [Flight Indicators Canlı Test](https://ignaciovr.github.io/flight-indicators-js/)

### 2. D3 / Canvas Havacılık Göstergeleri
* **D3 Horizon Plugin:** Uçak pitch/roll yapay ufuk çizgisi ve HUD projeksiyonu.
* **SVG Cockpit Gauges:** [cockpit-instruments](https://github.com/mdeverdelhan/cockpit-instruments)

### 3. Canlı Taktik Harita (GPS Mini-Map)
* **Leaflet.js:** Çok hafif, uydu ve harita katmanlarını destekleyen açık kaynak harita kütüphanesi. Uçağın rotasını ve anlık GPS konumunu çizer.
  * **Link:** [LeafletJS Resmi Sitesi](https://leafletjs.com/)
* **OpenLayers:** İleri düzey CBS ve koordinat desteği.

### 4. Telemetri ve Motor Grafikleri
* **Apache ECharts:** Yüksek performanslı, karanlık mod destekli motor devri (RPM), gaz yüzdesi, pil voltajı ve sıcaklık göstergeleri.
  * **Link:** [Apache ECharts Örnekleri](https://echarts.apache.org/examples/en/index.html)
* **Chart.js:** Canlı akan sensör gürültüsü ve ivme grafikleri.

---

## 3. Tasarım Araçları ve Prototipleme Kaynakları

Kendi arayüzünüzü internette tasarlayıp doğrudan bana dosya olarak vermek veya ilham almak için kullanabileceğiniz araçlar:

1. **v0.dev (Yapay Zeka Destekli UI Üretici):**
   * Prompt ile *"SpaceX tarzı, koyu temalı, havacılık telemetri ekranı oluştur"* dediğinizde saniyeler içinde eksiksiz modern kod üretir.
   * **Link:** [v0.dev](https://v0.dev/)
2. **Figma:**
   * Dünyanın en popüler UI/UX tasarım aracı. Uçak HUD şablonları ve kokpit mockup'ları hazırdır.
   * **Topluluk Şablonları:** [Figma Dashboard Templates](https://www.figma.com/community/search?resource_type=mixed&sort_by=relevancy&query=dashboard)
3. **Tailwind UI & Shadcn/UI:**
   * Modern, şık ve endüstriyel koyu tema kutuları, kartlar, sekmeler ve butonlar.

---

## 4. Modüler UI Bileşen Listesi (Seçebileceğiniz Lego Parçaları)

Arayüzü kurarken *"Bunu alalım, şunu şuraya koyalım"* diyebileceğiniz ana bloklar:

| Bileşen | Görevi | Görsel Tarz |
| :--- | :--- | :--- |
| **Primary Flight Display (PFD)** | Yapay ufuk, hız şeridi, irtifa şeridi | F-35 / Modern Glass Cockpit |
| **Engine & Power Card** | Motor devri (RPM), Gaz (%), Batarya (V / A) | Neon mavi/yeşil seviye çubukları |
| **Tactical GPS Map** | Dünya haritası, uçak ikonu, rota çizgisi | Karanlık askeri radar / uydu haritası |
| **Autopilot Status Bar** | Otopilot modu (FBWA, Auto, RTL), Bağlantı (9002/9003) | Üst bilgi çubuğu / Hap etiketler (Pills) |
| **Aero Surfaces Visualizer** | Kanatçıkların (Aileron, Rudder, Flap) anlık açıları | 3D tel çerçeve uçak şeması |
| **Control Console / Log** | Sistem logları, hata uyarıları, kalibrasyon durumu | Konsol terminal penceresi |
