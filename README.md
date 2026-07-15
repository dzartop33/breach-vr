# Breach VR

Тактическая VR-игра «штурм» для **Meta Quest 3S**, где **карта строится из планировки
твоей реальной квартиры**. Поддерживает **отслеживание рук (hand tracking)** и
**контроллеры (джойстики)**.

Проект сделан на **чистом Android (Kotlin/Java + NDK/C++)** поверх **OpenXR** —
**без Unity и без Godot**, поэтому APK собирается в GitHub Actions **без каких-либо
лицензий**.

## Как это работает

| Слой | Технология |
|------|-----------|
| Сборка | Gradle 8.9 + Android Gradle Plugin 8.5 |
| Нативный код VR | C++17 + NDK 26, OpenXR loader (Khronos AAR) |
| Точка входа | `NativeActivity` → `android_main` (`app/src/main/cpp/main.cpp`) |
| Отслеживание рук | `XR_EXT_hand_tracking` (пинч = основное действие) |
| Карта из квартиры | `XR_FB_scene` / spatial entities (сканирование комнаты Quest) |
| Passthrough | `XR_FB_passthrough` (видно реальную квартиру) |

## Структура репозитория

```
.
├── app/
│   ├── build.gradle.kts          # конфиг модуля приложения (SDK, NDK, ABI arm64)
│   └── src/main/
│       ├── AndroidManifest.xml   # VR-флаги, разрешения рук/сцены, Quest 3S
│       ├── cpp/                  # нативный движок на OpenXR
│       │   ├── CMakeLists.txt
│       │   ├── main.cpp          # android_main + главный цикл
│       │   ├── openxr_app.h
│       │   └── openxr_app.cpp    # instance/session/hand tracking/scene
│       └── res/                  # строки, иконка
├── .github/workflows/main.yml    # автосборка APK
├── build.gradle.kts              # версии плагинов
├── settings.gradle.kts
└── gradlew / gradlew.bat         # Gradle wrapper
```

## Сборка APK через GitHub Actions

Ничего настраивать не нужно — просто запушь в репозиторий. Workflow
`.github/workflows/main.yml`:

1. ставит JDK 17, Android SDK, NDK 26 и CMake;
2. создаёт Gradle wrapper (если его ещё нет);
3. запускает `./gradlew assembleDebug`;
4. кладёт готовый `.apk` в **Artifacts** сборки.

Скачать APK: вкладка **Actions** → нужный запуск → раздел **Artifacts** →
`breach-vr-debug-apk`.

APK подписан debug-ключом, поэтому его можно сразу поставить на Quest 3S через
`adb install` или SideQuest.

## Локальная сборка (необязательно)

```bash
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Статус

Каркас проекта: манифест, Gradle/NDK-конфигурация, точка входа OpenXR и хуки
под hand tracking и сканирование квартиры готовы. Следующие шаги —
рендер-цикл (EGL/GLES + swapchain), отрисовка рук и геймплей штурма.
