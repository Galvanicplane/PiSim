# PiSim Mimari Haritası (Architecture Registry)

Bu belge, PiSim projesinin modüler klasör hiyerarşisini, sınıfların sorumluluklarını ve haberleşme sözleşmelerini tanımlar.
Geliştirme yaparken körlemesine arama (grep) yapmak yerine bu haritayı ve ilgili sınıfın `.h` başlık dosyasını referans alınız.

---

## 1. Modül ve Dizin Hiyerarşisi

### `Source/PiSim/1_Vehicle/` (Araç Omurgası)
- **`Vehicle.h / .cpp` (`APiSimVehicle`):**
  - Saf ve hafif araç Pawn'ı.
  - Sorumlulukları:
    - İskelet ağı (`USkeletalMeshComponent`) ve görsel ağları barındırmak.
    - Kemik slotlarını tanımlamak (Her kemik bir slot barındırabilir).
    - Temel araç fiziğini (açma/kapama, kütle merkezi) aktive etmek.
  - Kesinlikle garaj UI'ı veya dosya ithalat kodu içermez.

---

### `Source/PiSim/2_Motors/` (Motorlar ve Kontrol Yüzeyleri)
Tüm motorlar `UActorComponent` türevidir ve `IPiSimInspectableModule` arayüzünü uygular.
- **`Wheel.h / .cpp` (`UPiSimWheelComponent`):**
  - Drive Wheel (tork/RPM ile tahrik) ve Free Wheel (serbest dönen caster/rulman) tekerlek fiziği.
- **`Servo.h / .cpp` (`UPiSimServoComponent`):**
  - Açısal mafsal servoları (Robot kolu eklemleri, direksiyon yaw servosu, uçak kontrol yüzeyleri).
- **`Propeller.h / .cpp` (`UPiSimPropellerComponent`):**
  - Havacılık ve deniz araçları için itki (Thrust) ve tork üreten pervane simülasyonu.
- **`LiftBody.h / .cpp` (`UPiSimLiftBodyComponent`):**
  - Sabit kanat, elevon, rudder, flap ve tekne gövdesi için aerodinamik kaldırma (Lift) ve sürükleme (Drag) fiziği.
- **`Track.h / .cpp` (`UPiSimTrackComponent`):**
  - Paletli araç sürtünme plakası (Taslak).
- **`Actuator.h / .cpp` (`UPiSimLinearActuatorComponent`):**
  - Lineer hidrolik/elektrikli piston hareketi (Taslak).

---

### `Source/PiSim/3_Sensors/` (Sanal Sensörler)
Sensörler `UActorComponent` türevidir ve ölçtükleri veriyi doğrudan `UTelemetryGateway`'e aktarır.
- **`CameraSensor.h / .cpp` (`UPiSimCameraSensor`):**
  - FPV/Gimbal sanal kamera; RenderTarget üzerinden JPEG sıkıştırması yapar.
- **`GPSSensor.h / .cpp` (`UPiSimGPSSensor`):**
  - Simülasyon dünyasını gerçek dünya koordinatlarına çevirir (Ankara, İstanbul vb. sahte başlangıç konfigürasyonunu destekler).
- **`IMUSensor.h / .cpp` (`UPiSimIMUSensor`):**
  - 3-eksen ivmeölçer, jiroskop ve yönelim (Quaternion) verisi üretir.
- **`LidarSensor.h / .cpp` (`UPiSimLidarSensor`):**
  - Mesafe / Lazer tarayıcı sensör bileşeni (Taslak).

---

### `Source/PiSim/4_Communication/` (Haberleşme & Dış Dünya Köprüleri)
- **`TelemetryGateway.h / .cpp` (`UPiSimTelemetryGateway`):**
  - [ORTAK ÇIKIŞ KAPISI] Tüm sensörlerin bağlandığı, telemetri paketlerini tek elde toplayan merkezi dağıtıcı.
- **`Pi5Bridge.h / .cpp` (`UPiSimPi5Bridge`):**
  - Raspberry Pi 5 köprüsü. İki ayrı asenkron iş parçacığı (Thread) yönetir:
    - Thread 1: Port A üzerinden Canlı JPEG Video Akışı.
    - Thread 2: Port B üzerinden ROS 2 CDR / JSON Telemetri Akışı.
- **`AutopilotBridge.h / .cpp` (`UPiSimAutopilotBridge`):**
  - ArduPilot (Port 9002/9003 SITL JSON & 16-Kanal Binary PWM) ve PX4 (Port 14560 MAVLink) köprüsü.
- **`SerialPort.h / .cpp` (`UPiSimSerialPort`):**
  - Donanımsal FTDI/USB UART seri port iletişimi.

---

### `Source/PiSim/5_Configurators/` (Yapılandırıcılar ve Kalıcılık)
- **`FbxImporter.h / .cpp` (`UPiSimFbxImporter`):**
  - FBX/GLB model dosyasını `APiSimVehicle` iskeletine yükler; Scale, Pivot ve Normal dönüşümlerini yönetir.
- **`MotorConfigurator.h / .cpp` (`UPiSimMotorConfigurator`):**
  - İskeletteki kemik isimlerine göre otomatik motor rolü atar. UI'dan yapılan seçimleri ilgili kemiğe bağlar/değiştirir.
- **`Baker.h / .cpp` (`UPiSimBaker`):**
  - Yapılan motor, mafsal, physics constraint ve ArduPilot kanal eşleşmelerini kalıcı simülasyon modeline gömer (`Bake`).
- **`SaveManager.h / .cpp` (`UPiSimSaveManager`):**
  - Kullanıcıdan isim alarak yapılandırılmış aracı JSON / Data asset olarak diske kaydeder ve kaydedilen aracı doğrudan yükler.

---

### `Source/PiSim/6_UI/` (Kullanıcı Arayüzü & Otopark Slotları)
- **`HUD.h / .cpp` (`APiSimHUD`):**
  - Canlı uçuş/sürüş anındaki telemetri ibreleri ve göstergeleri.
- **`GarageWidget.h / .cpp` (`UPiSimGarageWidget`):**
  - Otopark slot widget'ı. Solda kemik hiyerarşisi listesi, sağda seçilen modülün dinamik parametre paneli (Slider, InputBox, Toggle).
  - Cast<T> yapmaz; `IPiSimInspectableModule` üzerinden konuşur.
- **`ImporterWidget.h / .cpp` (`UPiSimImporterWidget`):**
  - Dosya seçme ve basit içe aktarma arayüzü.

---

### `Source/PiSim/7_Core/` (Çekirdek Sistem)
- **`PiSim.h / .cpp`:** Unreal Engine modül yaşam döngüsü.
- **`IPiSimInspectableModule.h`:** UI otopark slotlarıyla konuşan dinamik özellik arayüzü sözleşmesi.
