# ロギング/エラーハンドリングの外部注入機能の改善

## 変更概要

LiveTunerのロギングシステムを改善し、様々な環境（特にGUIのみのWindowsアプリケーションやゲームエンジン）での使用をより柔軟にしました。

## 実装された機能

### 1. デフォルトロギングの制御マクロ

新しいマクロ `LIVETUNER_ENABLE_DEFAULT_LOGGING` を追加しました。

#### デフォルト動作
- **デバッグビルド** (`_DEBUG`, `DEBUG`, または `NDEBUG` が未定義): ログを `std::cerr` に出力
- **リリースビルド** (`NDEBUG` が定義されている): ログ出力を無効化

#### カスタマイズ方法

```cpp
// デフォルトログ出力を無効化（GUIアプリに便利）
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"
```

```cpp
// リリースビルドでも強制的に有効化
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 1
#include "LiveTuner.h"
```

### 2. デフォルトログハンドラの実装

- 新しい内部関数 `internal::default_log_handler()` を実装
- `LIVETUNER_ENABLE_DEFAULT_LOGGING` マクロに基づいて動作を制御
- 無効化時はコンパイル時に最適化されるため、オーバーヘッドなし

### 3. カスタムロギング統合の強化

既存の `set_log_callback()` 機能はそのまま維持しつつ、より詳細なドキュメントを追加：

```cpp
// カスタムロガーの設定
livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
    MyEngine::Log(level, msg);
});

// すべてのログを無効化
livetuner::set_log_callback(nullptr);
```

## 変更されたファイル

### コアライブラリ
- `include/LiveTuner.h`
  - マクロ `LIVETUNER_ENABLE_DEFAULT_LOGGING` の追加
  - `internal::default_log_handler()` の実装
  - `<iostream>` のインクルード追加
  - `set_log_callback()` のドキュメント強化

### ドキュメント
- `README.md` - ロギング設定セクションの追加（英語）
- `README_JA.md` - ロギング設定セクションの追加（日本語）
- `LOGGING.md` - 詳細なロギングガイドの新規作成

### テスト
- `Test/test_logging.cpp` - ロギング機能の総合テスト
- `Test/test_logging_disabled.cpp` - ログ無効化のテスト

## 使用例

### GUIのみのWindowsアプリケーション

```cpp
// コンソール出力を無効化
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

int WINAPI WinMain(...) {
    // std::cerrへの出力は発生しない
    livetuner::Params params("config.json");
    // ...
}
```

### Unreal Engineとの統合

```cpp
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

void InitializeLiveTuner() {
    livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
        switch (level) {
            case livetuner::LogLevel::Error:
                UE_LOG(LogTemp, Error, TEXT("%s"), UTF8_TO_TCHAR(msg.c_str()));
                break;
            // ...
        }
    });
}
```

### 組み込みシステム（リソース制約環境）

```cpp
// ログを完全に無効化してオーバーヘッドを最小化
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

int main() {
    // コールバック未設定なのでログ出力なし
    livetuner::Params params("config.txt");
    // ...
}
```

## 後方互換性

- **完全に後方互換**: 既存コードは変更なしで動作します
- デフォルト動作: デバッグビルドではログ出力、リリースビルドでは無効
- 既存の `set_log_callback()` 使用コードは影響を受けません

## テスト結果

### test_logging.cpp
- ✅ デフォルトログ出力（デバッグビルド）
- ✅ カスタムロガーの設定
- ✅ ログの無効化

### test_logging_disabled.cpp
- ✅ マクロによるコンパイル時のログ無効化
- ✅ デフォルトログ無効化後もカスタムログは機能

## 利点

1. **コンソールレスアプリ対応**: GUIのみのWindowsアプリで不要な `std::cerr` 出力を防止
2. **ゲームエンジン統合**: Unreal、Unity等のネイティブロガーと簡単に統合
3. **パフォーマンス最適化**: リリースビルドでデフォルトでログを無効化
4. **柔軟性**: マクロで細かく制御可能
5. **ゼロオーバーヘッド**: 無効化時はコンパイル時に最適化

## まとめ

この改善により、LiveTunerは様々なアプリケーション環境（特にゲームエンジンやGUIアプリ）でより使いやすくなりました。デフォルトの動作は既存コードに影響を与えず、必要に応じて柔軟にカスタマイズできます。

## ブロッキングAPIとEvent-Drivenフォールバックの明確化

### ブロッキング動作について

`LiveTuner::get<T>()` は**ブロッキングAPI**であり、ファイルから有効な値が読み込まれるまで呼び出し元をブロックします。

```cpp
float speed;
tuner.get(speed);  // 値が読み込まれるまでブロック
```

**⚠️ 注意**: このAPIはデバッグや実験的用途には便利ですが、商用・企業向けコードではブロッキングAPIは敬遠される傾向があります。本番コードでは以下の非ブロッキングAPIを推奨します：

```cpp
// 推奨: 非ブロッキングAPI
float speed = 1.0f;  // デフォルト値
tuner.try_get(speed);  // 即座に返る（ブロックしない）

// 推奨: タイムアウト付きAPI
float speed = 1.0f;
if (tuner.get_timeout(speed, std::chrono::milliseconds(100))) {
    // 値が読み込まれた
} else {
    // タイムアウト - デフォルト値を使用
}
```

### Event-Driven ファイル監視のフォールバック

Event-Driven モードでファイル監視の開始に失敗した場合、自動的にポーリングモードにフォールバックします。この際、以下の詳細なログメッセージがユーザーに通知されます：

**ログ出力例**:
```
[WARNING] [Error] WatcherError: Failed to start file watcher, falling back to polling mode (path: config.json)
[INFO] LiveTuner: Event-driven file watching failed for 'config.json'. Falling back to polling mode (100ms interval). This may occur due to: OS watcher resource limits, unsupported filesystem, or permission issues. Performance may be slightly reduced.
```

**フォールバックの原因**:
1. **OS watcher リソース制限**: inotify (Linux) や kqueue (macOS) のウォッチ数上限に達した
2. **サポートされていないファイルシステム**: NFS、CIFS などのネットワークファイルシステム
3. **権限の問題**: ファイルまたはディレクトリへのアクセス権限不足
4. **プラットフォーム固有の問題**: 特定のOS設定やカーネルパラメータ

**フォールバック時の動作**:
- ポーリング間隔: 100ms
- パフォーマンスへの影響: わずかに増加（ただし機能は完全に維持）
- ユーザーアクション: 通常は不要（自動的に回復）

### エラーハンドリングのカスタマイズ

フォールバック通知をカスタムログハンドラで受け取ることができます：

```cpp
livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
    if (level == livetuner::LogLevel::Warning && 
        msg.find("falling back to polling") != std::string::npos) {
        // フォールバック発生時の特別な処理
        notifyUser("ファイル監視がポーリングモードに切り替わりました");
    }
    // 通常のログ処理
    MyLogger::log(level, msg);
});
```

