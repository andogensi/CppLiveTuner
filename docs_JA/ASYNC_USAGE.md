# 非同期API ドキュメント

CppLiveTunerライブラリの非同期・ノンブロッキングAPIの詳細ドキュメントです。

## 概要

ライブチューニングでは、パラメータの変更を待機する際にメインスレッドをブロックしたくない場合があります。
特にゲームループやGUIアプリケーションでは、フレームレートを維持しながらパラメータを監視する必要があります。

CppLiveTunerは以下のAPIバリエーションを提供します：

### 単一値API

| 関数 | 特徴 | 用途 |
|------|------|------|
| `tune_try()` | 即座にチェック、ノンブロッキング | ゲームループ、リアルタイム処理 |
| `tune_timeout()` | タイムアウト付きの待機 | 長時間フリーズの防止 |
| `tune_async<T>()` | `std::future`を返す非同期版 | バックグラウンド処理 |
| `tune_async<T>(callback)` | コールバック呼び出し | イベント駆動型プログラミング |

### Params API

| 関数 | 特徴 | 用途 |
|------|------|------|
| `params.update()` | 即座にチェック、ノンブロッキング | 手動更新ループ |
| `params.start_watching()` + `poll()` | バックグラウンド監視 | 自動更新 |
| `params.on_change(callback)` | 変更時にコールバック | イベント駆動 |

---

## 1. tune_try - ノンブロッキング即座チェック

最も軽量なAPIです。呼び出し時にファイルが変更されているかを即座にチェックします。

### シグネチャ

```cpp
template<typename T>
bool tune_try(T& value);
```

### 対応型

- `int`, `float`, `double`
- その他 `>>` 演算子が定義された型

### 使用例

```cpp
#include "LiveTuner.h"

int main() {
    float speed = 1.0f;
    
    // ゲームループでの使用例
    while (game_running) {
        // ノンブロッキングでパラメータをチェック
        if (tune_try(speed)) {
            std::cout << "速度が更新されました: " << speed << "\n";
        }
        
        // ゲームの更新（ブロックされない！）
        update_game(speed);
        render_frame();
        
        // 60 FPS
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    return 0;
}
```

### パフォーマンス

`tune_try()` は以下の最適化を行っています：

1. **ファイル更新日時のキャッシュ**: 10ms以内の連続呼び出しでは、ファイルを開かずにキャッシュから判定
2. **軽量なファイルシステムチェック**: `std::filesystem::last_write_time()` を使用
3. **条件付きファイル読み取り**: 更新が検出された場合のみファイルを開く

---

## 2. tune_timeout - タイムアウト付き待機

指定した時間内にパラメータが読み込めなければ `false` を返します。

### シグネチャ

```cpp
template<typename T>
bool tune_timeout(T& value, std::chrono::milliseconds timeout);
```

### 使用例

```cpp
#include "LiveTuner.h"
#include <chrono>

int main() {
    float difficulty = 1.0f;
    
    std::cout << "難易度をparams.txtに入力してください（5秒以内）...\n";
    
    // 5秒間待機
    if (tune_timeout(difficulty, std::chrono::seconds(5))) {
        std::cout << "難易度を設定: " << difficulty << "\n";
    } else {
        std::cout << "タイムアウト！デフォルト値を使用します\n";
        difficulty = 1.0f;
    }
    
    start_game(difficulty);
    return 0;
}
```

---

## 3. tune_async - Future版

`std::future<T>` を返す非同期版です。バックグラウンドスレッドで入力を待機します。

### シグネチャ

```cpp
template<typename T>
std::future<T> tune_async();
```

### 使用例

```cpp
#include "LiveTuner.h"
#include <future>

int main() {
    // 非同期でパラメータ待機を開始
    auto future = tune_async<float>();
    
    // メインスレッドは他の処理を続行
    std::cout << "バックグラウンドでパラメータを待機中...\n";
    
    // 定期的に結果をチェック
    while (future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
        std::cout << "ロード中...\n";
        show_loading_screen();
    }
    
    // 結果を取得
    float value = future.get();
    std::cout << "パラメータを受信: " << value << "\n";
    
    return 0;
}
```

---

## 4. tune_async(callback) - コールバック版

コールバック関数を指定して、パラメータが読み込まれた時に呼び出されます。

### シグネチャ

```cpp
template<typename T, typename Callback>
void tune_async(Callback callback);
```

### 使用例

```cpp
#include "LiveTuner.h"
#include <atomic>

int main() {
    std::atomic<bool> config_loaded{false};
    float config_value = 0.0f;
    
    // コールバックを設定して非同期待機を開始
    tune_async<float>([&](float value) {
        config_value = value;
        std::cout << "コールバックで値を受信: " << value << "\n";
        config_loaded = true;
    });
    
    // メインスレッドは即座に続行
    std::cout << "メインスレッドは続行中...\n";
    
    // 設定が読み込まれるまでスプラッシュ画面を表示
    while (!config_loaded) {
        show_splash_screen();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    start_with_config(config_value);
    return 0;
}
```

### 注意事項

- コールバックは別スレッドで呼び出されます
- スレッドは `detach()` されるため、明示的な終了待機が必要な場合は `std::atomic` などを使用してください

---

## 5. Params::update - ノンブロッキング更新

名前付きパラメータを使用する場合の推奨API。

### 使用例

```cpp
#include "LiveTuner.h"

int main() {
    livetuner::Params params("game_config.json");
    
    float player_speed = 5.0f;
    float gravity = 9.8f;
    int max_enemies = 10;
    bool god_mode = false;
    
    params.bind("player_speed", player_speed, 5.0f);
    params.bind("gravity", gravity, 9.8f);
    params.bind("max_enemies", max_enemies, 10);
    params.bind("god_mode", god_mode, false);
    
    while (game_running) {
        // ノンブロッキングで全パラメータをチェック・更新
        if (params.update()) {
            std::cout << "設定が更新されました！\n";
            std::cout << "  速度: " << player_speed << "\n";
            std::cout << "  重力: " << gravity << "\n";
            std::cout << "  敵の最大数: " << max_enemies << "\n";
            std::cout << "  無敵モード: " << (god_mode ? "ON" : "OFF") << "\n";
        }
        
        update_game();
        render_frame();
    }
    
    return 0;
}
```

---

## 6. Params::start_watching + poll - バックグラウンド監視

OSネイティブAPIを使用したバックグラウンドファイル監視。

### 使用例

```cpp
#include "LiveTuner.h"

int main() {
    livetuner::Params params("config.json");
    
    float speed = 1.0f;
    params.bind("speed", speed, 1.0f);
    
    // バックグラウンド監視を開始
    params.start_watching();
    
    while (game_running) {
        // poll()で変更をチェック（CPU負荷ほぼゼロ）
        if (params.poll()) {
            std::cout << "速度が " << speed << " に変更されました\n";
        }
        
        player.move(speed);
        render();
    }
    
    // 監視を停止（デストラクタでも自動停止）
    params.stop_watching();
    
    return 0;
}
```

### start_watching vs update の違い

| 方式 | CPU負荷 | レイテンシ | 用途 |
|------|---------|-----------|------|
| `update()` | 低〜中（ポーリング） | キャッシュ期間（10ms） | シンプルな用途 |
| `start_watching()` + `poll()` | ほぼゼロ | OSイベント依存（ほぼ即座） | 本格的なゲーム開発 |

---

## 7. on_change コールバック

パラメータ変更時に自動的にコールバックを呼び出します。

### 使用例

```cpp
#include "LiveTuner.h"

int main() {
    livetuner::Params params("audio_config.json");
    
    float master_volume = 1.0f;
    float music_volume = 0.8f;
    float sfx_volume = 1.0f;
    
    params.bind("master_volume", master_volume, 1.0f);
    params.bind("music_volume", music_volume, 0.8f);
    params.bind("sfx_volume", sfx_volume, 1.0f);
    
    // 変更時のコールバックを設定
    params.on_change([&]() {
        std::cout << "オーディオ設定が変更されました\n";
        
        // オーディオエンジンに反映
        audio_engine.setMasterVolume(master_volume);
        audio_engine.setMusicVolume(music_volume);
        audio_engine.setSFXVolume(sfx_volume);
    });
    
    params.start_watching();
    
    while (running) {
        params.poll();  // 変更があればon_changeが自動的に呼ばれる
        // ...
    }
    
    return 0;
}
```

---

## パラメータファイルの形式

### params.txt（単一値用）

```
# コメント行（#で始まる行は無視されます）
2.5

# 空行も無視されます
```

### config.json（名前付きパラメータ用）

```json
{
    "player_speed": 5.0,
    "jump_height": 10.0,
    "gravity": 9.8,
    "god_mode": false,
    "player_name": "Hero"
}
```

### config.yaml

```yaml
# ゲーム設定
player_speed: 5.0
jump_height: 10.0
gravity: 9.8
god_mode: false
player_name: Hero
```

### config.ini

```ini
# ゲーム設定
player_speed = 5.0
jump_height = 10.0
gravity = 9.8
god_mode = false
player_name = Hero
```

---

## 使用シナリオ

### ゲーム開発 - パラメータチューニング

```cpp
void game_loop() {
    livetuner::Params params("gameplay.json");
    
    float jump_force, move_speed, air_control;
    
    params.bind("jump_force", jump_force, 15.0f);
    params.bind("move_speed", move_speed, 5.0f);
    params.bind("air_control", air_control, 0.3f);
    
    params.start_watching();
    
    while (!should_quit) {
        params.poll();
        
        // パラメータを変更するたびに即座に反映！
        // 再起動不要でゲームフィールを調整できる
        player.setJumpForce(jump_force);
        player.setMoveSpeed(move_speed);
        player.setAirControl(air_control);
        
        update();
        render();
    }
}
```

### シェーダー開発 - リアルタイムプレビュー

```cpp
void render_loop() {
    livetuner::Params params("shader_params.json");
    
    float exposure, contrast, saturation;
    float bloom_threshold, bloom_intensity;
    
    params.bind("exposure", exposure, 1.0f);
    params.bind("contrast", contrast, 1.0f);
    params.bind("saturation", saturation, 1.0f);
    params.bind("bloom_threshold", bloom_threshold, 0.8f);
    params.bind("bloom_intensity", bloom_intensity, 1.0f);
    
    params.start_watching();
    
    while (rendering) {
        params.poll();
        
        // シェーダーのユニフォームを即座に更新
        postprocess_shader.setFloat("exposure", exposure);
        postprocess_shader.setFloat("contrast", contrast);
        postprocess_shader.setFloat("saturation", saturation);
        postprocess_shader.setFloat("bloom_threshold", bloom_threshold);
        postprocess_shader.setFloat("bloom_intensity", bloom_intensity);
        
        render_scene();
        apply_postprocess();
        present();
    }
}
```

### AI/機械学習 - ハイパーパラメータ調整

```cpp
void training_loop() {
    livetuner::Params params("hyperparams.json");
    
    float learning_rate, momentum, weight_decay;
    int batch_size;
    
    params.bind("learning_rate", learning_rate, 0.001f);
    params.bind("momentum", momentum, 0.9f);
    params.bind("weight_decay", weight_decay, 0.0001f);
    params.bind("batch_size", batch_size, 32);
    
    params.on_change([&]() {
        std::cout << "ハイパーパラメータが更新されました\n";
        optimizer.setLearningRate(learning_rate);
        optimizer.setMomentum(momentum);
        optimizer.setWeightDecay(weight_decay);
    });
    
    params.start_watching();
    
    for (int epoch = 0; epoch < num_epochs; ++epoch) {
        params.poll();
        train_epoch(batch_size);
        evaluate();
    }
}
```

---

## トラブルシューティング

### tune_try() が常に false を返す

- パラメータファイルに有効な値が含まれているか確認
- コメント行や空行だけではないか確認
- 値の型が正しいか確認（整数を期待しているのに小数が入力されている等）

### コールバックが呼ばれない

- `poll()` がループ内で呼ばれているか確認
- `start_watching()` が呼ばれているか確認
- メインスレッドが先に終了していないか確認

### ファイル変更が検出されない

- ファイルが正しく保存されているか確認（エディタの自動保存設定）
- ファイルパスが正しいか確認
- イベント駆動モードが有効か確認: `tune_is_event_driven()`

---

## 参照

- [README.md](README.md) - メインドキュメント
- [LiveTuner.h](include/LiveTuner.h) - ソースコード
