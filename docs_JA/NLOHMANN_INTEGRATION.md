# CppLiveTuner への nlohmann/json アダプター追加について

## 追加された機能

nlohmann/json サポートは `LiveTuner.h` に統合されています。

### 有効化方法

`LIVETUNER_USE_NLOHMANN_JSON` マクロを定義し、nlohmann/json を先にインクルードしてから `LiveTuner.h` をインクルードしてください。

```cpp
#define LIVETUNER_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#define LIVETUNER_IMPLEMENTATION  // 1つのソースファイルでのみ
#include "LiveTuner.h"
```

### ドキュメント
- `docs/NLOHMANN_ADAPTER.md` - 詳細なドキュメント（API リファレンス含む）
- `docs/NLOHMANN_QUICKSTART.md` - 5分で始めるクイックスタートガイド

### サンプルとテスト
- `examples/example_nlohmann.cpp` - 実用的なサンプルコード
- `Test/test_nlohmann_adapter.cpp` - 包括的なテストコード

---

## README.md への追記案

以下の内容を `README.md` または `README_JA.md` に追記することをお勧めします：

### 追記場所: 「特徴」セクションの後

```markdown
## 🚀 nlohmann/json サポート

CppLiveTuner は **nlohmann/json** ライブラリとの統合をサポートしています！

複雑なJSON構造、配列、ネストされたオブジェクトを簡単に扱えます。

```cpp
#define LIVETUNER_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#define LIVETUNER_IMPLEMENTATION
#include "LiveTuner.h"

livetuner::NlohmannParams params("config.json");

while (running) {
    if (params.update()) {
        // ネストされた値を取得
        auto speed = params.get<float>("player.speed", 1.0f);
        auto colors = params.get<std::vector<int>>("colors", {});
    }
}
```

**詳しくは:**
- 📖 [クイックスタート](docs/NLOHMANN_QUICKSTART.md) - 5分で始める
- 📚 [詳細ドキュメント](docs/NLOHMANN_ADAPTER.md) - API リファレンス

---
```

### 追記場所: 使用例セクション

```markdown
### 📦 nlohmann/json を使った高度な例

```cpp
#define LIVETUNER_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#define LIVETUNER_IMPLEMENTATION
#include "LiveTuner.h"

livetuner::NlohmannBinder binder("config.json");

float speed;
std::string name;
std::vector<float> position;

// 変数を自動バインド
binder.bind("player.speed", speed, 1.0f);
binder.bind("player.name", name, std::string("Hero"));
binder.bind("player.position", position, {0.0f, 0.0f, 0.0f});

while (running) {
    if (binder.update()) {
        // 全ての変数が自動更新される！
        player.move(speed);
    }
}
```

**config.json:**
```json
{
  "player": {
    "name": "Warrior",
    "speed": 2.5,
    "position": [10.0, 20.0, 30.0]
  }
}
```

詳しくは [examples/example_nlohmann.cpp](examples/example_nlohmann.cpp) を参照。
```

### 追記場所: ビルド方法セクション

```markdown
### nlohmann/json サンプルのビルド

```bash
# CMake でビルド
mkdir build && cd build
cmake .. -DLIVETUNER_BUILD_EXAMPLES=ON
cmake --build . --target livetuner_example_nlohmann

# 直接コンパイル
g++ -std=c++17 examples/example_nlohmann.cpp -I include -o example_nlohmann
```
```

---

## 機能一覧

### `NlohmannParams` クラス

| 機能 | 説明 |
|------|------|
| `get<T>()` | 型安全な値の取得 |
| `get_json()` | JSONオブジェクトの取得 |
| `set()` | 値の設定 |
| `has()` | 値の存在チェック |
| `save()` | ファイルへの保存 |
| `update()` | ファイル変更の検出と更新 |

### `NlohmannBinder` クラス

| 機能 | 説明 |
|------|------|
| `bind()` | 変数の自動バインディング |
| `update()` | 全バインド変数の自動更新 |

### JSONパス記法

- `"key"` - トップレベルのキー
- `"parent.child"` - ネストされたキー
- `"array[0]"` - 配列要素
- `"parent.array[1].value"` - 複雑なパス

---

## 利用例

### ゲーム開発
```cpp
binder.bind("player.speed", player_speed, 5.0f);
binder.bind("enemy.speed", enemy_speed, 3.0f);
binder.bind("physics.gravity", gravity, 9.8f);
```

### シミュレーション
```cpp
auto time_step = params.get<double>("sim.time_step", 0.01);
auto iterations = params.get<int>("sim.iterations", 100);
```

### 機械学習
```cpp
auto learning_rate = params.get<double>("ml.learning_rate", 0.001);
auto batch_size = params.get<int>("ml.batch_size", 32);
```

---

## テスト

テストを実行して動作を確認：

```bash
# CMake でテストをビルド & 実行
mkdir build && cd build
cmake .. -DLIVETUNER_BUILD_TESTS=ON
cmake --build . --target livetuner_test_nlohmann_adapter
ctest

# または直接実行
./livetuner_test_nlohmann_adapter
```

---

## サポート

質問や問題がある場合：

1. まず [NLOHMANN_QUICKSTART.md](NLOHMANN_QUICKSTART.md) を確認
2. [NLOHMANN_ADAPTER.md](NLOHMANN_ADAPTER.md) のトラブルシューティングを参照
3. サンプルコード [examples/example_nlohmann.cpp](examples/example_nlohmann.cpp) を試す

---

## ライセンス

このアダプターは CppLiveTuner と同じ MIT License です。
nlohmann/json も MIT License で提供されています。
