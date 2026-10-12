# PiSim Mimari Haritası (Architecture Registry)

Bu belge, PiSim projesinin modüler klasör hiyerarşisini, sınıfların sorumluluklarını ve haberleşme sözleşmelerini tanımlar.
Geliştirme yaparken körlemesine arama (grep) yapmak yerine bu haritayı ve ilgili sınıfın `.h` başlık dosyasını referans alınız.

---

## 1. Modül ve Dizin Hiyerarşisi

### `Source/PiSim/1_Vehicle/` (Araç Omurgası)
- **`Vehicle.h / .cpp` (`APiSimVehicle`):**
  - Saf ve hafif araç Pawn'ı.
  - Sorumlulukları:
    - İskelet ağı (`USkeletalMeshComponent`), dinamik parçalar (`VisualMeshComponents`) ve UCX çarpışma zırhı (`CollisionMeshComponents`).
    - Kemik slotlarını (`BoneSlotRegistry`), kayıtlı kemik isimlerini (`VehicleBoneNames`) ve hiyerarşik ağacı (`VehicleBoneHierarchy` / `FPiSimBoneHierarchyNode`) barındırmak.
    - Kemik sahne bileşenleri haritası (`BoneSceneComponents`, `GetSceneComponentForBone`) ile procedural/steer mafsal referanslarını tutmak.
    - Araç fiziğini (`SetVehiclePhysicsEnabled`) açma/kapama: KESİNLİKLE Visual Mesh'e fizik ve collision vermez; fiziği ve yerçekimini SADECE UCX zırh gövdesine (`CollisionMeshComponents[0]`) verir. UCX yoksa kırmızı hata fırlatır.
    - Fare ile 360° Orbit (Sol tık), Pan (Sağ tık) ve Zoom (Tekerlek) kontrollerini `Tick()` içinde yürütmek.
    - UCX çarpışma zırhı inceleme görünürlüğünü (`SetCollisionMeshViewEnabled`) yönetmek.
    - Başlangıçta hedef model bulunamazsa asla rastgele/fallback model atamama emniyeti.
  - Kesinlikle dosya ayrıştırma veya doğrudan garaj Slate UI kodu içermez.

---

## 2. Motorlar ve Kontrol Yüzeyleri (`Source/PiSim/2_Motors/`)
Tüm motorlar `UActorComponent` türevidir ve `IPiSimInspectableModule` arayüzünü uygular.
- **`Wheel.h / .cpp` (`UPiSimWheelComponent`):**
  - Drive Wheel (tork/RPM ile tahrik), Steered Wheel (direksiyon açılı mafsal) ve Free Wheel (serbest dönen caster) tekerlek fiziği.
- **`Servo.h / .cpp` (`UPiSimServoComponent`):**
  - Açısal mafsal servoları (Elevon mikseri, direksiyon yaw servosu, robot eklemleri).
- **`Propeller.h / .cpp` (`UPiSimPropellerComponent`):**
  - Havacılık ve deniz araçları için karesel itki ($T \propto \text{Throttle}^2$) üreten pervane simülasyonu.
- **`LiftBody.h / .cpp` (`UPiSimLiftBodyComponent`):**
  - Sabit kanat, elevon ve gövde için Khan & Nahon aerodinamik kaldırma (Lift) ve sürükleme (Drag) fiziği.
- **`Track.h / .cpp` (`UPiSimTrackComponent`):**
  - Paletli araç sürtünme plakası bileşeni.
- **`Actuator.h / .cpp` (`UPiSimLinearActuatorComponent`):**
  - Lineer hidrolik/elektrikli piston hareketi.

---

## 3. Sanal Sensörler (`Source/PiSim/3_Sensors/`)
Sensörler `UActorComponent` türevidir ve ölçtükleri veriyi doğrudan `UTelemetryGateway`'e aktarır.
- **`IMUSensor.h / .cpp` (`UPiSimIMUSensor`):**
  - 3-eksen ivmeölçer, jiroskop ve NED gövde koordinatlarında manyetometre verisi.
- **`GPSSensor.h / .cpp` (`UPiSimGPSSensor`):**
  - Simülasyon dünyasını WGS-84 enlem/boylam/irtifa ve NED hız vektörüne çevirir.
- **`BarometerSensor.h / .cpp` (`UPiSimBarometerSensor`):**
  - ISA standart atmosfer statik ve pitot dinamik basınç sensörü.
- **`LidarSensor.h / .cpp` (`UPiSimLidarSensor`):**
  - Mesafe / Lazer tarayıcı sensör bileşeni.

---

## 4. Haberleşme & Dış Dünya Köprüleri (`Source/PiSim/4_Communication/`)
- **`TelemetryGateway.h / .cpp` (`UPiSimTelemetryGateway`):**
  - [ORTAK ÇIKIŞ KAPISI] Tüm sensörlerin bağlandığı, telemetri paketlerini tek elde toplayan merkezi dağıtıcı (SITL JSON).
- **`AutopilotBridge.h / .cpp` (`UPiSimAutopilotBridge`):**
  - Çift yönlü ArduPilot SITL UDP köprüsü (Port 9002 JSON telemetri TX, Port 9003 PWM RX).

---

## 5. Yapılandırıcılar ve Kalıcılık (`Source/PiSim/5_Configurators/`)
- **`ModelFbxImporter.h / .cpp` (`UPiSimModelFbxImporter`):**
  - İkili (Binary) FBX ayrıştırma motoru (`ParseBinaryFbxFile`).
  - Her parçanın pivot noktasını bağlı olduğu kemiğin eklem konumu (`BonePosition`) olarak kilitler.
  - `bEnableSmoothNormals` parametresi ile görsel mesh için yumuşatma, UCX için düz (flat) normal üretimi.
  - Çıktı sınıflandırması: `OutVisual` (görsel parçalar), `OutUCX` (Chaos zırh gövdeleri), `OutSensors` (soket transformları), `OutPureBones` (geometrisiz iskelet mafsalları), `OutBoneHierarchy` (sensörsüz hiyerarşik ağaç).
  - Pure Steer kemiklerini (`M_Steer_FR`, `M_Steer_FL` vb.) `ModelMap` üzerinden eksiksiz çıkarma ve ön tekerlekler için `SteerJointComp` kurma.
  - **Kesin UCX Kuralı:** Modelde UCX yoksa asla Visual Mesh'e collision vermez, kırmızı hata basar. Fiziğin tek taşıyıcısı UCX Procedural bileşenleridir (`CollisionMeshComponents`). Visual mesh tamamen `NoCollision` ve `SimulatePhysics=false` kalır.
  - **Merkezleme ve SpaceX Kamera Kilidi:** Ana şasi pivotu (`BaseChassisPivot`) baz alınarak tüm parçalar aktör merkezine yerleştirilir. `CameraSpringArm` ithalat anında `VisualMeshComponents[0]` şasisine kilitlenerek 360° orbit kameranın her zaman aracı tam merkezde göstermesi sağlanır.
  - **Kök Bileşen Emniyeti:** Yeniden ithalatta (`ClearVehicleModel`) kök bileşenin yok edilmesini önlemek amacıyla aktör kökü güvenli `VehicleSkeletalMesh` üzerinde korunur.
  - `GetAvailableCacheModels`: `Saved/Robots/Cache/` altındaki `.fbx` dosyalarını listeleme.
  - `ImportFbxFileToVehicle`: Modeli `APiSimVehicle`'a monte etme ve `MotorConfigurator` otomatik rol atamasını tetikleme.
- **`MotorConfigurator.h / .cpp` (`UPiSimMotorConfigurator`):**
  - İskeletteki kemik isimlerine göre otomatik motor/servoları atar ve PWM kanallarını haritalar.
  - Oluşturulan motor bileşenlerine `TargetSceneComponent` referansını bağlayarak canlı rotasyonu ve telemetriyi aktive eder.
- **`Baker.h / .cpp` (`UPiSimBaker`):**
  - Yapılan motor, mafsal ve fizik eşleşmelerini kalıcı modele gömer (`Bake`).
- **`SaveManager.h / .cpp` (`UPiSimSaveManager`):**
  - Yapılandırılmış aracı JSON profil olarak kaydeder ve yükler.

---

## 6. Kullanıcı Arayüzü & Otopark Slotları (`Source/PiSim/6_UI/`)
- **`ModularHUD.h / .cpp` (`APiSimModularHUD`):**
  - PFD aviyonik HUD (Suni ufuk, Airspeed ve Altitude Tape, Heading Pusulası, Telemetri çubuğu).
  - Garaj/İthalat modunda ekranı temiz tutmak için varsayılan olarak kapalıdır (`bShowAvionicsHUD = false`).
- **`GarageWidget.h / .cpp` (`UPiSimModularGarageWidget`):**
  - Otopark slot widget'ı.
  - **3-Sütunlu Slate Düzeni:** Sol panel sola yaslı (330px), Sağ panel sağa yaslı (390px), Orta alan (%40+) tamamen boş ve `SelfHitTestInvisible` (3D sahneyi görme ve sol/sağ fareyle orbit/pan yapma imkanı).
  - Üst Toolbar:
    - `[ 📦 Modeli Değiştir: test.fbx ]`: Diskteki FBX modelleri arasında hızlı geçiş yapar.
    - `[ × 0.1 ]` ve `[ × 10 ]`: Model ölçekleme çarpanları.
    - `[ 🔄 REIMPORT ]`: Aktif modeli diskten tekrar yükler ve kemik ağacını yeniler.
    - `[ 🎨 GÖRSEL MOD / 🛡️ UCX ZIRH ]`: Araç üzerindeki UCX fizik zırhını görünür/gizli yapar.
    - `[ ⚡ FİZİK SİMÜLE ET ]`: `Vehicle->SetVehiclePhysicsEnabled`'ı tetikler.
    - `[ 🤖 OTO ROL ]`, `[ 💾 KAYDET ]`, `[ 📂 YÜKLE ]`, `[ 🔥 BAKE ]`.
  - Solda hiyerarşik girintili kemik ağacı listesi (`IndentLevel * 16px` ve `↳ ` girintisi), sağda `IPiSimInspectableModule` üzerinden dinamik parametre slotları (Slider, Toggle) ve canlı motor debug loglama yapısı.
  - Cast<T> yapmaz.
- **`ImporterWidget.h / .cpp` (`UPiSimModularImporterWidget`):**
  - Model İthalat Arayüzü (Slate Modal).
  - Diskteki `Saved/Robots/Cache/` FBX dosyalarını listeler, ölçekleme (`0.1x / 1.0x / 10x`) ve yumuşak normal seçimi sunar.
  - İthalat tamamlandığında aracı donatmak üzere Garaj paneline geçiş yapar.

---

## 7. Çekirdek Sistem (`Source/PiSim/7_Core/`)
- **`PiSim.h / .cpp`:** Unreal Engine modül yaşam döngüsü.
- **`IPiSimInspectableModule.h`:** UI otopark slotlarıyla konuşan dinamik özellik arayüzü sözleşmesi (`FPiSimPropertyDescriptor`).
