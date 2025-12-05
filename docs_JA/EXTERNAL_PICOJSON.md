# 外部picojsonサポート

## 概要

CppLiveTunerは、組み込みバージョンの代わりに外部のpicojsonインストールを使用することをサポートしています。この機能は以下のような場合に便利です：

- プロジェクトで既にpicojsonを使用している場合
- 特定のバージョンのpicojsonが必要な場合
- コードの重複を減らしたい場合

## 使用方法

### オプション1: 組み込みpicojsonを使用（デフォルト）

デフォルトでは、CppLiveTunerは独自のpicojsonコピーを含んでいます。通常通りヘッダーをインクルードするだけで使用できます：

```cpp
#include "LiveTuner.h"

// picojsonはlivetuner::picojsonとして利用可能
```

### オプション2: 外部picojsonを使用

独自のpicojsonインストールを使用するには：

1. LiveTuner.hをインクルードする前に `LIVETUNER_USE_EXTERNAL_PICOJSON` を定義
2. まず独自のpicojsonヘッダーをインクルード
3. LiveTuner.hをインクルード

```cpp
#define LIVETUNER_USE_EXTERNAL_PICOJSON
#include "picojson.h"  // 外部のpicojson
#include "LiveTuner.h"
```

またはコンパイラフラグで指定：

```bash
g++ -std=c++17 -DLIVETUNER_USE_EXTERNAL_PICOJSON \
    -I/path/to/picojson/include \
    -Iinclude your_program.cpp -pthread -o program
```

## 実装詳細

実装ではプリプロセッサガードを使用して、組み込みpicojsonを条件付きでインクルードしています：

```cpp
#ifndef LIVETUNER_USE_EXTERNAL_PICOJSON

namespace picojson {
    // ... 組み込みpicojsonコード ...
} // namespace picojson

#endif // LIVETUNER_USE_EXTERNAL_PICOJSON
```

`LIVETUNER_USE_EXTERNAL_PICOJSON` が定義されている場合：
- 組み込みpicojsonコードはコンパイルから完全に除外される
- LiveTunerは外部ソースからpicojsonが利用可能であることを期待する
- コードの重複や名前空間の競合は発生しない

## テスト

組み込みpicojson機能を検証するためのテストプログラムが用意されています：

```bash
cd CppLiveTuner
g++ -std=c++17 -Iinclude Test/test_embedded_picojson.cpp -o build/test_embedded_picojson.exe -pthread
./build/test_embedded_picojson.exe
```

期待される出力：
```
=== Testing Embedded picojson (Default Behavior) ===

Test 1: Creating test JSON file...
  ✓ Test file created: test_embedded_picojson_config.json

Test 2: Using LiveTuner::Params with embedded picojson...
  Values read from JSON:
    speed: 3.14
    gravity: 9.8
    debug: true
    name: EmbeddedPicojsonTest
  ✓ All values correct!

Test 3: Verify embedded picojson namespace is accessible...
  ✓ picojson is embedded within livetuner namespace
  ✓ JSON parsing works correctly (verified in Test 2)

=== All Tests Passed ===

LiveTuner successfully works with embedded picojson (default behavior)!
```

## メリット

### 組み込みpicojsonユーザー向け（デフォルト）
- 依存関係ゼロ
- そのまま動作
- セットアップ不要

### 外部picojsonユーザー向け
- コードの重複なし
- 好みのpicojsonバージョンを使用可能
- 依存関係のより良い制御
- picojsonが既にリンクされている場合、より小さなバイナリ

## 互換性

- 組み込みバージョンとAPI互換のpicojsonが必要
- 組み込みバージョンはpicojson v1.3.0に基づいています
- 外部picojsonは同じAPIを提供する必要があります（namespace picojson、valueクラス、parse関数など）

## 参照

- [GitHub上のpicojson](https://github.com/kazuho/picojson)
- [README.md](README.md) - メインドキュメント
- [README_JA.md](README_JA.md) - 日本語ドキュメント
