# nlohmann/json アダプター

CppLiveTuner で **nlohmann/json** ライブラリを使用して、複雑なJSON構造を扱うためのアダプターです。

## 特徴

- ✨ **ネストされたJSONオブジェクト**の読み込み対応
- 📦 **配列のサポート** - JSON配列を簡単に扱える
- 🔒 **型安全** - テンプレートによる型安全な値の取得
- 🔄 **リアルタイム更新** - ファイル変更を自動検出
- 🎯 **JSONパス** - ドット記法でネストした値にアクセス（例: `"player.speed"`）
- 🔧 **自動バインディング** - 変数を自動的に更新
- 💾 **読み書き対応** - JSONファイルの読み込みと保存

## 必要なもの

- C++17 以降
- nlohmann/json ライブラリ（`Json.hpp`）
- CppLiveTuner の基本機能

## インストール

1. nlohmann/json ライブラリをプロジェクトに追加
2. `LIVETUNER_USE_NLOHMANN_JSON` マクロを定義
3. nlohmann/json を先にインクルードし、その後 `LiveTuner.h` をインクルード

```cpp
#define LIVETUNER_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#define LIVETUNER_IMPLEMENTATION  // 1つのソースファイルでのみ定義
#include "LiveTuner.h"
```

## 基本的な使い方

### 1. シンプルな値の取得

```cpp
#define LIVETUNER_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#define LIVETUNER_IMPLEMENTATION
#include "LiveTuner.h"

livetuner::NlohmannParams params("config.json");

// 基本的な値の取得
float speed = params.get<float>("player.speed", 1.0f);
std::string name = params.get<std::string>("player.name", "Player");
bool debug = params.get<bool>("debug", false);

while (running) {
    if (params.update()) {
        // ファイルが変更されたら値を再取得
        speed = params.get<float>("player.speed", 1.0f);
    }
    
    // ゲームロジック
    player.move(speed);
}
```

**config.json:**
```json
{
  "player": {
    "name": "Hero",
    "speed": 2.5
  },
  "debug": true
}
```

### 2. 配列の取得

```cpp
// 配列全体を取得
auto colors = params.get<std::vector<int>>("colors", {255, 0, 0});

// 配列の特定要素にアクセス
int red = params.get<int>("colors[0]", 255);
int green = params.get<int>("colors[1]", 0);

// ネストされた配列
auto position = params.get<std::vector<float>>("player.position", {0.0f, 0.0f, 0.0f});
```

**config.json:**
```json
{
  "colors": [255, 128, 64],
  "player": {
    "position": [10.0, 20.0, 30.0]
  }
}
```

### 3. 自動バインディング

変数を自動的に更新したい場合は `NlohmannBinder` を使用します。

```cpp
livetuner::NlohmannBinder binder("config.json");

// 変数をバインド
float speed;
std::string name;
bool debug;

binder.bind("player.speed", speed, 1.0f);
binder.bind("player.name", name, std::string("Player"));
binder.bind("debug", debug, false);

while (running) {
    if (binder.update()) {
        // 全ての変数が自動的に更新される！
        std::cout << "Speed: " << speed << "\n";
        std::cout << "Name: " << name << "\n";
    }
}
```

### 4. ネストされたオブジェクト

深くネストされた値にもアクセス可能です。

```cpp
// 深くネストされた値
auto resolution_width = params.get<int>("game.settings.graphics.resolution[0]", 1920);
auto volume = params.get<float>("game.settings.audio.master_volume", 1.0f);
auto quality = params.get<std::string>("game.settings.graphics.quality", "medium");
```

**config.json:**
```json
{
  "game": {
    "settings": {
      "graphics": {
        "resolution": [1920, 1080],
        "quality": "high"
      },
      "audio": {
        "master_volume": 0.8
      }
    }
  }
}
```

### 5. JSON全体の操作

```cpp
// JSON全体を取得
auto full_json = params.get_json();
std::cout << full_json.dump(2) << "\n";

// 特定のセクションを取得
auto player_data = params.get_json("player");

// 値が存在するかチェック
if (params.has("player.speed")) {
    // ...
}
```

### 6. 値の設定と保存

```cpp
// プログラムから値を変更
params.set("player.speed", 3.5f);
params.set("player.name", std::string("NewName"));
params.set("debug", true);

// ファイルに保存
params.save(true);  // true = 整形して保存
```

## API リファレンス

### NlohmannParams

メインのパラメータ管理クラス。

#### コンストラクタ

```cpp
NlohmannParams(const std::string& file_path)
```

#### メソッド

```cpp
// 更新チェック
bool update()

// 値を取得（デフォルト値指定可能）
template<typename T>
T get(const std::string& json_path, const T& default_value = T{})

// JSONオブジェクトを取得
json get_json(const std::string& json_path = "")

// 値が存在するかチェック
bool has(const std::string& json_path)

// 値を設定
template<typename T>
bool set(const std::string& json_path, const T& value)

// ファイルに保存
bool save(bool pretty = true)

// JSON全体を文字列として取得（デバッグ用）
std::string dump(int indent = 2)

// エラーコールバックを設定
void set_error_callback(ErrorCallback callback)

// 最後のエラーを取得
const ErrorInfo& last_error()
```

### NlohmannBinder

自動バインディング用のヘルパークラス。

```cpp
// 変数をバインド
template<typename T>
void bind(const std::string& json_path, T& variable, const T& default_value = T{})

// 更新チェックと全変数の更新
bool update()

// 元のNlohmannParamsにアクセス
NlohmannParams& params()
```

## JSONパスの記法

| パス | 説明 | 例 |
|------|------|-----|
| `"key"` | トップレベルのキー | `"debug"` |
| `"parent.child"` | ネストされたキー | `"player.speed"` |
| `"array[0]"` | 配列の要素 | `"colors[0]"` |
| `"parent.array[1]"` | ネストされた配列 | `"player.position[1]"` |

## エラーハンドリング

```cpp
livetuner::NlohmannParams params("config.json");

// エラーコールバックを設定
params.set_error_callback([](const livetuner::ErrorInfo& error) {
    std::cerr << "エラー: " << error.message << "\n";
    std::cerr << "種類: " << livetuner::ErrorInfo::type_to_string(error.type) << "\n";
});

// 最後のエラーを確認
if (params.last_error()) {
    std::cerr << params.last_error().message << "\n";
}
```

## サポートされる型

- 基本型: `int`, `float`, `double`, `bool`, `std::string`
- コンテナ: `std::vector<T>`, `std::array<T, N>`
- nlohmann/json がサポートする全ての型

## 実行例

```bash
# コンパイル（nlohmann/json のパスを指定）
g++ -std=c++17 examples/example_nlohmann.cpp -I include -I <nlohmann_include_path> -o example_nlohmann.exe

# 実行
./example_nlohmann.exe
```

プログラム実行中に `config_nlohmann.json` を編集すると、値がリアルタイムで反映されます。

## 使用例コード

詳細な使用例は以下を参照してください：

- [`examples/example_nlohmann.cpp`](../examples/example_nlohmann.cpp) - 基本的な使い方から高度な使い方まで

## パフォーマンス

- ファイル監視はOSネイティブAPI（Windows: ReadDirectoryChangesW, Linux: inotify）を使用
- JSONのパースは変更検出時のみ実行
- スレッドセーフな実装

## 制限事項

- JSONファイルは有効なJSON形式である必要があります
- 非常に大きなJSONファイル（数MB以上）の場合、パース時間が長くなる可能性があります
- JSONパスは基本的なドット記法のみサポート（複雑なクエリは未サポート）

## トラブルシューティング

### Q: JSONのパースエラーが発生する

**A:** JSONファイルの構文を確認してください。カンマの位置、括弧の対応、文字列のクォーテーションなどをチェックしてください。

### Q: 値が更新されない

**A:** 
1. `update()` メソッドを定期的に呼び出しているか確認
2. JSONパスが正しいか確認（大文字小文字も区別されます）
3. エラーコールバックを設定してエラーメッセージを確認

### Q: 配列の要素にアクセスできない

**A:** 配列アクセスの記法は `"array[0]"` です。インデックスは0始まりです。

## ライセンス

このアダプターは CppLiveTuner と同じライセンス（MIT License）で提供されます。

nlohmann/json ライブラリは MIT License で提供されています。
