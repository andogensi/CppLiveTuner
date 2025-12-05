# nlohmann/json アダプター クイックスタート

## 5分で始める nlohmann/json + CppLiveTuner

### ステップ 1: 必要なファイルを用意

```
your_project/
├── LiveTuner.h                    # CppLiveTuner本体
├── nlohmann/
│   └── json.hpp                   # nlohmann/json ライブラリ
└── main.cpp                        # あなたのコード
```

### ステップ 2: 最小限のコード例

```cpp
// Step 1: nlohmann/json サポートを有効化
#define LIVETUNER_USE_NLOHMANN_JSON

// Step 2: nlohmann/json を先にインクルード
#include <nlohmann/json.hpp>

// Step 3: 実装を有効化（1つのソースファイルでのみ）
#define LIVETUNER_IMPLEMENTATION
#include "LiveTuner.h"

#include <iostream>
#include <thread>
#include <chrono>

int main() {
    // JSONファイルを監視
    livetuner::NlohmannParams params("config.json");
    
    while (true) {
        // ファイル変更をチェック
        if (params.update()) {
            std::cout << "設定が更新されました！\n";
        }
        
        // 値を取得
        float speed = params.get<float>("player.speed", 1.0f);
        std::string name = params.get<std::string>("player.name", "Player");
        
        std::cout << name << " の速度: " << speed << "\n";
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
```

### ステップ 3: JSONファイルを作成

`config.json` を作成:

```json
{
  "player": {
    "name": "Hero",
    "speed": 2.5
  }
}
```

### ステップ 4: コンパイル & 実行

```bash
# Windows (MinGW/MSYS2) - nlohmann/json のインクルードパスを指定
g++ -std=c++17 main.cpp -I <nlohmann_include_path> -o main.exe

# Linux/Mac
g++ -std=c++17 main.cpp -I <nlohmann_include_path> -o main -pthread

# vcpkg 使用時の例
g++ -std=c++17 main.cpp -I /path/to/vcpkg/installed/x64-windows/include -o main.exe

# 実行
./main
```

### ステップ 5: ライブチューニング！

プログラムを実行したまま `config.json` を編集してみてください：

```json
{
  "player": {
    "name": "Warrior",
    "speed": 5.0
  }
}
```

保存すると即座に反映されます！

---

## もっと便利な使い方

### 自動バインディング

```cpp
livetuner::NlohmannBinder binder("config.json");

float speed;
std::string name;

binder.bind("player.speed", speed, 1.0f);
binder.bind("player.name", name, std::string("Player"));

while (true) {
    if (binder.update()) {
        // speed と name が自動更新される！
        std::cout << name << ": " << speed << "\n";
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
```

### 配列を扱う

```cpp
// JSON: {"colors": [255, 128, 64]}
auto colors = params.get<std::vector<int>>("colors", {0, 0, 0});

// または個別に
int r = params.get<int>("colors[0]", 0);
int g = params.get<int>("colors[1]", 0);
int b = params.get<int>("colors[2]", 0);
```

### ネストされたオブジェクト

```cpp
// JSON: {"game": {"settings": {"volume": 0.8}}}
float volume = params.get<float>("game.settings.volume", 1.0f);
```

### エラーハンドリング

```cpp
params.set_error_callback([](const livetuner::ErrorInfo& error) {
    std::cerr << "エラー: " << error.message << "\n";
});
```

---

## よくある使用例

### ゲーム開発

```cpp
livetuner::NlohmannBinder binder("game_config.json");

float player_speed, enemy_speed, gravity;
int max_enemies;
bool debug_mode;

binder.bind("player.speed", player_speed, 5.0f);
binder.bind("enemy.speed", enemy_speed, 3.0f);
binder.bind("physics.gravity", gravity, 9.8f);
binder.bind("spawn.max_enemies", max_enemies, 10);
binder.bind("debug", debug_mode, false);

while (game_running) {
    binder.update();  // 実行中に値を変更可能！
    
    // ゲームロジック
    player.move(player_speed);
    physics.apply_gravity(gravity);
    // ...
}
```

### シミュレーション

```cpp
livetuner::NlohmannParams params("simulation.json");

while (simulation_running) {
    if (params.update()) {
        // パラメータを再取得
        auto time_step = params.get<double>("sim.time_step", 0.01);
        auto iterations = params.get<int>("sim.iterations", 100);
        auto damping = params.get<float>("sim.damping", 0.99f);
        
        // シミュレーションパラメータを更新
        simulation.set_time_step(time_step);
        simulation.set_iterations(iterations);
    }
    
    simulation.step();
}
```

### アルゴリズムのチューニング

```cpp
livetuner::NlohmannParams params("algorithm.json");

while (running) {
    if (params.update()) {
        // アルゴリズムパラメータを調整
        auto learning_rate = params.get<double>("ml.learning_rate", 0.001);
        auto batch_size = params.get<int>("ml.batch_size", 32);
        auto dropout = params.get<float>("ml.dropout", 0.5f);
        
        model.set_learning_rate(learning_rate);
        model.set_batch_size(batch_size);
    }
    
    model.train();
}
```

---

## トラブルシューティング

### Q: 値が更新されない

✅ `update()` を定期的に呼び出していますか？  
✅ JSONのパスが正しいですか？（`"player.speed"` など）  
✅ JSONファイルの構文は正しいですか？

### Q: コンパイルエラー

✅ C++17が有効になっていますか？ (`-std=c++17`)  
✅ `Json.hpp` が正しいパスにありますか？  
✅ Windows の場合、マルチスレッド対応でコンパイルしていますか？

### Q: リンクエラー (pthread)

Linux/Mac では `-pthread` オプションが必要です：

```bash
g++ -std=c++17 main.cpp -o main -pthread
```

---

## 次のステップ

- 📖 [詳細ドキュメント](NLOHMANN_ADAPTER.md) を読む
- 💡 [サンプルコード](examples/example_nlohmann.cpp) を試す
- 🧪 [テストコード](Test/test_nlohmann_adapter.cpp) を確認する

---

**Happy Live Tuning! 🎮🔧**
