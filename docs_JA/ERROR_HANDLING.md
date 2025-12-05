# エラーハンドリングガイド

CppLiveTuner v2.0 では、詳細なエラーハンドリング機能が追加されました。

## 概要

従来のバージョンでは、ファイル読み込みやパース失敗時に単に `false` や `nullopt` を返すだけで、失敗理由が不明でした。新バージョンでは以下の機能を提供します：

1. **エラー種別の分類**: ファイルが存在しない、アクセス権限がない、パースエラーなど
2. **詳細なエラーメッセージ**: 失敗理由とファイルパスを含む
3. **ログコールバック**: カスタマイズ可能なログ出力
4. **エラー取得API**: プログラムから最後のエラー情報を取得可能

## エラー種別

```cpp
enum class ErrorType {
    None,                   // エラーなし
    FileNotFound,          // ファイルが存在しない
    FileAccessDenied,      // ファイルへのアクセスが拒否された
    FileEmpty,             // ファイルが空
    FileReadError,         // ファイルの読み込みに失敗
    ParseError,            // パースエラー（文法エラー）
    InvalidFormat,         // フォーマットが不正
    Timeout,               // タイムアウト
    WatcherError,          // ファイル監視の開始に失敗
    Unknown                // 不明なエラー
};
```

## 基本的な使い方

### LiveTuner の場合

```cpp
#include <LiveTuner.h>
#include <iostream>

int main() {
    livetuner::LiveTuner tuner("params.txt");
    
    float speed = 1.0f;
    
    while (running) {
        if (!tuner.try_get(speed)) {
            // 失敗した場合、エラー情報を確認
            auto error = tuner.last_error();
            if (error) {
                std::cerr << "Error: " << error.to_string() << std::endl;
                
                // エラー種別に応じた処理
                switch (error.type) {
                    case livetuner::ErrorType::FileNotFound:
                        std::cerr << "Create the file: " << error.file_path << std::endl;
                        break;
                    case livetuner::ErrorType::FileAccessDenied:
                        std::cerr << "Check file permissions!" << std::endl;
                        break;
                    case livetuner::ErrorType::ParseError:
                        std::cerr << "Fix the file format!" << std::endl;
                        break;
                    default:
                        break;
                }
            }
        }
        
        player.move(speed);
    }
}
```

### Params の場合

```cpp
#include <LiveTuner.h>
#include <iostream>

int main() {
    livetuner::Params params("config.json");
    
    float speed = 1.0f;
    float gravity = 9.8f;
    
    params.bind("speed", speed, 1.0f);
    params.bind("gravity", gravity, 9.8f);
    
    while (running) {
        if (!params.update()) {
            // 失敗した場合、エラー情報を確認
            if (params.has_error()) {
                auto error = params.last_error();
                std::cerr << "Failed to update config: " 
                         << error.to_string() << std::endl;
            }
        }
        
        // speed, gravity は更新される（失敗時はデフォルト値）
        player.move(speed);
        physics.set_gravity(gravity);
    }
}
```

## ログコールバックの設定

グローバルなログコールバックを設定することで、全てのエラーや警告をキャッチできます。

```cpp
#include <LiveTuner.h>
#include <iostream>
#include <fstream>

int main() {
    // ログファイルを開く
    std::ofstream log_file("livetuner.log", std::ios::app);
    
    // ログコールバックを設定
    livetuner::set_log_callback([&log_file](livetuner::LogLevel level, const std::string& msg) {
        const char* level_str = "INFO";
        switch (level) {
            case livetuner::LogLevel::Debug:   level_str = "DEBUG"; break;
            case livetuner::LogLevel::Info:    level_str = "INFO";  break;
            case livetuner::LogLevel::Warning: level_str = "WARN";  break;
            case livetuner::LogLevel::Error:   level_str = "ERROR"; break;
        }
        
        // 標準エラー出力とファイルの両方に出力
        std::string log_msg = "[LiveTuner:" + std::string(level_str) + "] " + msg;
        std::cerr << log_msg << std::endl;
        log_file << log_msg << std::endl;
    });
    
    // 以降、全てのエラーがログに記録される
    livetuner::LiveTuner tuner("params.txt");
    float speed;
    tuner.try_get(speed);
    
    return 0;
}
```

## エラー情報の詳細

`ErrorInfo` 構造体には以下の情報が含まれます：

```cpp
struct ErrorInfo {
    ErrorType type;                              // エラー種別
    std::string message;                         // エラーメッセージ
    std::string file_path;                       // 問題が発生したファイルパス
    std::chrono::system_clock::time_point timestamp;  // エラー発生時刻
    
    // エラーがあるかチェック
    explicit operator bool() const;
    
    // エラー情報を文字列に変換
    std::string to_string() const;
};
```

### 使用例

```cpp
livetuner::Params params("config.json");

if (!params.update()) {
    auto error = params.last_error();
    
    if (error) {  // エラーがある場合
        std::cerr << "Error type: " << livetuner::ErrorInfo::type_to_string(error.type) << std::endl;
        std::cerr << "Message: " << error.message << std::endl;
        std::cerr << "File: " << error.file_path << std::endl;
        
        // または一括で出力
        std::cerr << error.to_string() << std::endl;
        // 出力例: [FileNotFound] config.json: File does not exist
    }
}
```

## リリース後のデバッグ

リリースビルドでも診断情報を取得できるようにする例：

```cpp
#include <LiveTuner.h>
#include <fstream>
#include <ctime>

class DiagnosticLogger {
public:
    DiagnosticLogger(const std::string& log_path) {
        log_file_.open(log_path, std::ios::app);
        
        livetuner::set_log_callback([this](livetuner::LogLevel level, const std::string& msg) {
            // エラーと警告のみをログに記録（デバッグ情報は除外）
            if (level == livetuner::LogLevel::Error || 
                level == livetuner::LogLevel::Warning) {
                
                auto now = std::chrono::system_clock::now();
                auto time_t = std::chrono::system_clock::to_time_t(now);
                
                log_file_ << std::ctime(&time_t) << " - " << msg << std::endl;
                log_file_.flush();
            }
        });
    }
    
private:
    std::ofstream log_file_;
};

int main() {
    DiagnosticLogger logger("diagnostics.log");
    
    livetuner::Params params("config.json");
    // ... アプリケーションコード ...
    
    // ユーザーから「設定が反映されない」という報告があった場合、
    // diagnostics.log を確認すれば原因が分かる
    
    return 0;
}
```

## エラーのクリア

エラー情報は自動的にクリアされませんが、手動でクリアすることもできます：

```cpp
livetuner::LiveTuner tuner("params.txt");

float speed;
if (!tuner.try_get(speed)) {
    // エラー処理
    auto error = tuner.last_error();
    std::cerr << error.to_string() << std::endl;
}

// エラーをクリア（必要に応じて）
tuner.clear_error();

// または、次の成功した読み込みで自動的にクリアされます
```

## パフォーマンスへの影響

エラーハンドリング機能は以下のように設計されています：

- **ログコールバックが未設定の場合**: ほぼオーバーヘッドなし（関数ポインタのチェックのみ）
- **エラー情報の記録**: 失敗時のみ発生するため、通常動作には影響なし
- **エラー取得API**: ミューテックスでロックするため、頻繁な呼び出しは避ける

## よくあるエラーと対処法

### FileNotFound
```
[FileNotFound] params.txt: File does not exist
```
**対処法**: ファイルを作成するか、正しいパスを指定する

### FileAccessDenied
```
[FileAccessDenied] config.json: Cannot open file for reading
```
**対処法**: ファイルの権限を確認する、または別のプロセスがロックしていないか確認

### ParseError
```
[ParseError] config.json: Failed to parse JSON format
```
**対処法**: ファイルの文法を確認する（JSON/YAML/INI形式）

### FileEmpty
```
[FileEmpty] params.txt: File is empty
```
**対処法**: ファイルに値を書き込む

### Timeout
```
[Timeout] params.txt: Timeout waiting for valid value
```
**対処法**: タイムアウト時間を延長するか、ファイルが正しく更新されているか確認

## まとめ

エラーハンドリング機能により、以下が可能になりました：

- ✅ 設定が反映されない理由を特定できる
- ✅ リリース後の現場でのデバッグが容易
- ✅ ログファイルに詳細な診断情報を記録できる
- ✅ エラー種別に応じた適切な対処が可能

これにより、「設定が反映されないが理由がわからない」という状況を回避できます。
