# ロギング設定ガイド

## 概要

LiveTunerは、様々なデプロイシナリオに合わせてカスタマイズ可能な柔軟なロギング機能を提供します。このガイドでは、様々なユースケースに対応するロギング動作の制御方法を説明します。

## デフォルト動作

### デバッグビルド
デバッグビルド（`_DEBUG`、`DEBUG`、または`NDEBUG`が未定義の場合）では、LiveTunerはデフォルトで `std::cerr` にログメッセージを自動出力します。

### リリースビルド
リリースビルド（`NDEBUG`が定義されている場合）では、オーバーヘッドを最小限に抑えるため、デフォルトロギングは無効になります。

## デフォルトロギングの制御

### デフォルトロギングを無効化

コンソール出力が見えないGUI専用Windowsアプリケーションに便利です：

```cpp
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

int main() {
    // デフォルトではstderrへのログ出力は行われない
    livetuner::Params params("config.json");
    // ...
}
```

### デフォルトロギングを強制有効化

リリースビルドでもロギングを強制的に有効化：

```cpp
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 1
#include "LiveTuner.h"

int main() {
    // リリースビルドでもstderrにログが出力される
    livetuner::Params params("config.json");
    // ...
}
```

## カスタムロギング統合

### ゲームエンジン統合

#### Unreal Engineの例
```cpp
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

void InitializeLiveTuner() {
    livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
        switch (level) {
            case livetuner::LogLevel::Debug:
                UE_LOG(LogTemp, Verbose, TEXT("[LiveTuner] %s"), UTF8_TO_TCHAR(msg.c_str()));
                break;
            case livetuner::LogLevel::Info:
                UE_LOG(LogTemp, Log, TEXT("[LiveTuner] %s"), UTF8_TO_TCHAR(msg.c_str()));
                break;
            case livetuner::LogLevel::Warning:
                UE_LOG(LogTemp, Warning, TEXT("[LiveTuner] %s"), UTF8_TO_TCHAR(msg.c_str()));
                break;
            case livetuner::LogLevel::Error:
                UE_LOG(LogTemp, Error, TEXT("[LiveTuner] %s"), UTF8_TO_TCHAR(msg.c_str()));
                break;
        }
    });
}
```

#### Unity ネイティブプラグインの例
```cpp
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

extern "C" {
    // UnityのDebug.Logコールバック
    typedef void (*UnityLogCallback)(int level, const char* message);
    UnityLogCallback g_UnityLog = nullptr;
    
    void SetUnityLogger(UnityLogCallback callback) {
        g_UnityLog = callback;
        
        livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
            if (g_UnityLog) {
                g_UnityLog(static_cast<int>(level), msg.c_str());
            }
        });
    }
}
```

### カスタムロガーの例

```cpp
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"
#include "MyGameLogger.h"

void SetupLiveTunerLogging() {
    livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
        MyGame::LogLevel gameLevel;
        switch (level) {
            case livetuner::LogLevel::Debug:   gameLevel = MyGame::LogLevel::Debug; break;
            case livetuner::LogLevel::Info:    gameLevel = MyGame::LogLevel::Info; break;
            case livetuner::LogLevel::Warning: gameLevel = MyGame::LogLevel::Warning; break;
            case livetuner::LogLevel::Error:   gameLevel = MyGame::LogLevel::Error; break;
        }
        
        MyGame::Logger::Log(gameLevel, "LiveTuner", msg);
    });
}
```

### ファイルロギングの例

```cpp
#include <fstream>

void SetupFileLogging() {
    static std::ofstream logFile("livetuner.log", std::ios::app);
    
    livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
        if (logFile.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            
            const char* level_str = "INFO";
            switch (level) {
                case livetuner::LogLevel::Debug:   level_str = "DEBUG"; break;
                case livetuner::LogLevel::Info:    level_str = "INFO"; break;
                case livetuner::LogLevel::Warning: level_str = "WARN"; break;
                case livetuner::LogLevel::Error:   level_str = "ERROR"; break;
            }
            
            logFile << "[" << std::ctime(&time) << "] "
                    << "[" << level_str << "] " 
                    << msg << std::endl;
        }
    });
}
```

## すべてのロギングを無効化

本番環境やパフォーマンスが重要なシナリオでは、ロギングを完全に無効化できます：

```cpp
livetuner::set_log_callback(nullptr);
```

## ユースケース

### GUI専用Windowsアプリケーション
```cpp
// コンソールウィンドウがないため、stderr出力を無効化
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

// オプション: ファイルやカスタムロギングシステムにログを送信
void SetupLogging() {
    livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
        // ファイルまたはカスタムGUIログウィンドウに書き込み
        MyApp::ShowLogInGUI(level, msg);
    });
}
```

### 組み込みシステム / リソース制約環境
```cpp
// ロギングを無効にしてオーバーヘッドを最小化
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

int main() {
    // ロギングコールバックなし - 最小限のオーバーヘッド
    livetuner::Params params("config.txt");
    // ...
}
```

### デバッグ出力を使った開発
```cpp
// デフォルト動作を維持（デバッグビルドではstderrにログ出力）
#include "LiveTuner.h"

int main() {
    // デバッグビルドではコンソールにログを表示
    // リリースビルドではロギングなし
    livetuner::Params params("config.json");
    // ...
}
```

### 条件付きロギング
```cpp
#include "LiveTuner.h"

void SetupConditionalLogging(bool enableLogging) {
    if (enableLogging) {
        livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
            const char* level_str = "INFO";
            switch (level) {
                case livetuner::LogLevel::Debug:   level_str = "DEBUG"; break;
                case livetuner::LogLevel::Info:    level_str = "INFO"; break;
                case livetuner::LogLevel::Warning: level_str = "WARN"; break;
                case livetuner::LogLevel::Error:   level_str = "ERROR"; break;
            }
            std::cerr << "[LiveTuner:" << level_str << "] " << msg << std::endl;
        });
    } else {
        livetuner::set_log_callback(nullptr);
    }
}
```

## ログレベル

LiveTunerは4つのログレベルを提供します：

- **Debug**: 詳細な診断情報
- **Info**: 一般的な情報メッセージ
- **Warning**: 動作を妨げない警告メッセージ
- **Error**: 失敗を示すエラーメッセージ

## ベストプラクティス

1. **開発時**: デバッグ用にデフォルトロギングまたはカスタムロガーを使用
2. **本番環境**: ロギングを無効化するか、エラーのみの最小限のロギングを使用
3. **GUIアプリケーション**: 常にデフォルトのstderrロギングを無効化
4. **ゲームエンジン**: エンジンのネイティブロギングシステムと統合
5. **パフォーマンス重視**: ロギングを完全に無効化するか、条件付きコンパイルを使用

## スレッドセーフ

ロギングシステムはスレッドセーフです。グローバルログコールバックは複数のスレッドから安全にアクセスできます。ただし、個々のログコールバック実装がI/Oや共有リソースにアクセスする場合は、独自のスレッドセーフを確保する必要があります。
